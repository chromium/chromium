// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/tasks/wait_for_store_initialize_task.h"

#include <vector>

#include "components/feed/core/proto/v2/store.pb.h"
#include "components/feed/core/v2/feed_store.h"
#include "components/feed/core/v2/feed_stream.h"
#include "components/feed/core/v2/feedstore_util.h"
#include "components/feed/core/v2/test/proto_printer.h"

namespace feed {

WaitForStoreInitializeTask::WaitForStoreInitializeTask(
    FeedStore* store,
    FeedStream* stream,
    base::OnceCallback<void(Result)> callback)
    : store_(*store), stream_(*stream), callback_(std::move(callback)) {}
WaitForStoreInitializeTask::~WaitForStoreInitializeTask() = default;

void WaitForStoreInitializeTask::Run() {
  store_->Initialize(
      base::BindOnce(&WaitForStoreInitializeTask::OnStoreInitialized,
                     weak_ptr_factory_.GetWeakPtr()));
}

void WaitForStoreInitializeTask::OnStoreInitialized() {
  store_->ReadStartupData(
      base::BindOnce(&WaitForStoreInitializeTask::ReadStartupDataDone,
                     weak_ptr_factory_.GetWeakPtr()));
  store_->ReadWebFeedStartupData(
      base::BindOnce(&WaitForStoreInitializeTask::WebFeedStartupDataDone,
                     weak_ptr_factory_.GetWeakPtr()));
}

void WaitForStoreInitializeTask::ReadStartupDataDone(
    FeedStore::StartupData startup_data) {
  if (startup_data.metadata &&
      startup_data.metadata->gaia() != stream_->GetAccountInfo().gaia) {
    store_->ClearAll(base::BindOnce(&WaitForStoreInitializeTask::ClearAllDone,
                                    weak_ptr_factory_.GetWeakPtr()));
    return;
  }
  // Single Web Feed Data is actively pruned and does not need to persist across
  // startups, and is being removed proactively here in the case that there
  // wasn't a chance to clean it up before the previous shutdown.
  const auto orig_size = startup_data.stream_data.size();
  std::erase_if(startup_data.stream_data, [&](const feedstore::StreamData& e) {
    return feedstore::StreamTypeFromKey(e.stream_key()).IsSingleWebFeed();
  });

  result_.startup_data = std::move(startup_data);

  if (result_.startup_data.stream_data.size() != orig_size) {
    store_->ClearAllStreamData(
        StreamKind::kSingleWebFeed,
        base::BindOnce(&WaitForStoreInitializeTask::ClearAllDone,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    MaybeUpgradeStreamSchema();
  }
}

void WaitForStoreInitializeTask::ClearAllDone(bool clear_ok) {
  DLOG_IF(ERROR, !clear_ok) << "FeedStore::ClearAll failed";
  // ClearAll just wiped metadata, so send nullptr.
  MaybeUpgradeStreamSchema();
}

void WaitForStoreInitializeTask::MaybeUpgradeStreamSchema() {
  feedstore::Metadata metadata;
  if (result_.startup_data.metadata)
    metadata = *result_.startup_data.metadata;

  if (metadata.stream_schema_version() != 1) {
    result_.startup_data.stream_data.clear();
    if (metadata.gaia().empty()) {
      metadata.set_gaia(stream_->GetAccountInfo().gaia);
    }
    store_->UpgradeFromStreamSchemaV0(
        std::move(metadata),
        base::BindOnce(&WaitForStoreInitializeTask::UpgradeDone,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }
  Done();
}

void WaitForStoreInitializeTask::UpgradeDone(feedstore::Metadata metadata) {
  result_.startup_data.metadata =
      std::make_unique<feedstore::Metadata>(std::move(metadata));
  Done();
}

void WaitForStoreInitializeTask::WebFeedStartupDataDone(
    FeedStore::WebFeedStartupData data) {
  result_.web_feed_startup_data = std::move(data);
  Done();
}

void WaitForStoreInitializeTask::Done() {
  if (++done_count_ == 2) {
    std::move(callback_).Run(std::move(result_));
    TaskComplete();
  }
}

}  // namespace feed
