// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/tasks/load_stream_from_store_task.h"

#include <algorithm>
#include <utility>

#include "base/check.h"
#include "base/time/time.h"
#include "components/feed/core/proto/v2/store.pb.h"
#include "components/feed/core/proto/v2/wire/reliability_logging_enums.pb.h"
#include "components/feed/core/v2/config.h"
#include "components/feed/core/v2/feed_store.h"
#include "components/feed/core/v2/feed_stream.h"
#include "components/feed/core/v2/feedstore_util.h"
#include "components/feed/core/v2/proto_util.h"
#include "components/feed/core/v2/protocol_translator.h"
#include "components/feed/core/v2/public/feed_api.h"
#include "components/feed/core/v2/scheduling.h"
#include "components/feed/core/v2/types.h"

namespace feed {

LoadStreamFromStoreTask::Result::Result() = default;
LoadStreamFromStoreTask::Result::~Result() = default;
LoadStreamFromStoreTask::Result::Result(Result&&) = default;
LoadStreamFromStoreTask::Result& LoadStreamFromStoreTask::Result::operator=(
    Result&&) = default;

LoadStreamFromStoreTask::LoadStreamFromStoreTask(
    LoadType load_type,
    FeedStream* feed_stream,
    const StreamType& stream_type,
    FeedStore* store,
    bool missed_last_refresh,
    bool is_web_feed_subscriber,
    base::OnceCallback<void(Result)> callback)
    : load_type_(load_type),
      feed_stream_(feed_stream),
      stream_type_(stream_type),
      store_(store),
      missed_last_refresh_(missed_last_refresh),
      is_web_feed_subscriber_(is_web_feed_subscriber),
      result_callback_(std::move(callback)),
      update_request_(std::make_unique<StreamModelUpdateRequest>()) {}

LoadStreamFromStoreTask::~LoadStreamFromStoreTask() = default;

void LoadStreamFromStoreTask::Run() {
  store_->LoadStream(
      stream_type_,
      base::BindOnce(&LoadStreamFromStoreTask::LoadStreamDone, GetWeakPtr()));
}

void LoadStreamFromStoreTask::LoadStreamDone(
    FeedStore::LoadStreamResult result) {
  if (result.read_error) {
    Complete(LoadStreamStatus::kFailedWithStoreError,
             feedwire::DiscoverCardReadCacheResult::FAILED);
    return;
  }
  pending_actions_ = std::move(result.pending_actions);

  if (result.stream_structures.empty()) {
    Complete(LoadStreamStatus::kNoStreamDataInStore,
             feedwire::DiscoverCardReadCacheResult::EMPTY_SESSION);
    return;
  }
  if (!ignore_account_) {
    if (result.stream_data.signed_in()) {
      const AccountInfo& account_info = feed_stream_->GetAccountInfo();
      if (result.stream_data.gaia() != account_info.gaia ||
          result.stream_data.email() != account_info.email) {
        Complete(LoadStreamStatus::kDataInStoreIsForAnotherUser,
                 feedwire::DiscoverCardReadCacheResult::FAILED);
        return;
      }
    }
  }

  content_ids_ = feedstore::GetContentIds(result.stream_data);
  if (!ignore_staleness_) {
    content_age_ =
        base::Time::Now() - feedstore::GetLastAddedTime(result.stream_data);

    const feedstore::Metadata& metadata = feed_stream_->GetMetadata();

    if (ContentInvalidFromAge(metadata, result.stream_type, content_age_,
                              is_web_feed_subscriber_)) {
      Complete(LoadStreamStatus::kDataInStoreIsExpired,
               feedwire::DiscoverCardReadCacheResult::STALE);
      return;
    }
    if (content_age_.is_negative()) {
      stale_reason_ = LoadStreamStatus::kDataInStoreIsStaleTimestampInFuture;
    } else if (ShouldWaitForNewContent(metadata, result.stream_type,
                                       content_age_, is_web_feed_subscriber_)) {
      stale_reason_ = LoadStreamStatus::kDataInStoreIsStale;
    } else if (missed_last_refresh_) {
      stale_reason_ = LoadStreamStatus::kDataInStoreStaleMissedLastRefresh;
    }
  }

  if (load_type_ == LoadType::kLoadNoContent) {
    Complete(LoadStreamStatus::kLoadedFromStore,
             feedwire::DiscoverCardReadCacheResult::CACHE_READ_OK);
    return;
  }

  std::vector<ContentId> referenced_content_ids;
  for (const feedstore::StreamStructureSet& structure_set :
       result.stream_structures) {
    for (const feedstore::StreamStructure& structure :
         structure_set.structures()) {
      if (structure.type() == feedstore::StreamStructure::CONTENT) {
        referenced_content_ids.push_back(structure.content_id());
      }
    }
  }

  store_->ReadContent(
      stream_type_, std::move(referenced_content_ids),
      {result.stream_data.shared_state_ids().begin(),
       result.stream_data.shared_state_ids().end()},
      base::BindOnce(&LoadStreamFromStoreTask::LoadContentDone, GetWeakPtr()));

  update_request_->stream_data = std::move(result.stream_data);

  // Move stream structures into the update request.
  // These need sorted by sequence number, and then inserted into
  // |update_request_->stream_structures|.
  std::sort(result.stream_structures.begin(), result.stream_structures.end(),
            [](const feedstore::StreamStructureSet& a,
               const feedstore::StreamStructureSet& b) {
              return a.sequence_number() < b.sequence_number();
            });

  for (feedstore::StreamStructureSet& structure_set :
       result.stream_structures) {
    update_request_->max_structure_sequence_number =
        structure_set.sequence_number();
    for (feedstore::StreamStructure& structure :
         *structure_set.mutable_structures()) {
      update_request_->stream_structures.push_back(std::move(structure));
    }
  }
}

void LoadStreamFromStoreTask::LoadContentDone(
    std::vector<feedstore::Content> content,
    std::vector<feedstore::StreamSharedState> shared_states) {
  update_request_->content = std::move(content);
  update_request_->shared_states = std::move(shared_states);

  update_request_->source =
      StreamModelUpdateRequest::Source::kInitialLoadFromStore;

  Complete(LoadStreamStatus::kLoadedFromStore,
           feedwire::DiscoverCardReadCacheResult::CACHE_READ_OK);
}

void LoadStreamFromStoreTask::Complete(
    LoadStreamStatus status,
    feedwire::DiscoverCardReadCacheResult reliability_result) {
  Result task_result;
  task_result.reliability_result = reliability_result;

  task_result.pending_actions = std::move(pending_actions_);
  if (status == LoadStreamStatus::kLoadedFromStore &&
      load_type_ == LoadType::kFullLoad) {
    task_result.update_request = std::move(update_request_);
  }
  if (status == LoadStreamStatus::kLoadedFromStore &&
      stale_reason_ != LoadStreamStatus::kNoStatus) {
    task_result.status = stale_reason_;
    task_result.reliability_result =
        feedwire::DiscoverCardReadCacheResult::STALE;
  } else {
    task_result.status = status;
  }
  task_result.content_age = content_age_;
  task_result.content_ids = content_ids_;
  std::move(result_callback_).Run(std::move(task_result));
  TaskComplete();
}

}  // namespace feed
