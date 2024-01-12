// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/stream_model.h"

#include <algorithm>
#include <sstream>
#include <utility>

#include "base/base64.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/json/string_escape.h"
#include "base/observer_list.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "components/feed/core/proto/v2/store.pb.h"
#include "components/feed/core/proto/v2/wire/content_id.pb.h"
#include "components/feed/core/v2/feedstore_util.h"
#include "components/feed/core/v2/proto_util.h"
#include "components/feed/core/v2/protocol_translator.h"
#include "components/feed/core/v2/types.h"

namespace feed {
namespace {
using Context = StreamModel::Context;
using UiUpdate = StreamModel::UiUpdate;
using StoreUpdate = StreamModel::StoreUpdate;

bool HasClearAll(const std::vector<feedstore::StreamStructure>& structures) {
  for (const feedstore::StreamStructure& data : structures) {
    if (data.operation() == feedstore::StreamStructure::CLEAR_ALL)
      return true;
  }
  return false;
}

void MergeSharedStateIds(const feedstore::StreamData& model_data,
                         feedstore::StreamData& update_request_data) {
  for (const auto& content_id : model_data.shared_state_ids()) {
    bool found = false;
    for (const auto& update_request_content_id :
         update_request_data.shared_state_ids()) {
      if (Equal(update_request_content_id, content_id)) {
        found = true;
        break;
      }
    }
    if (!found) {
      *update_request_data.add_shared_state_ids() = content_id;
    }
  }
}

}  // namespace

Context::Context() = default;
Context::~Context() = default;
UiUpdate::UiUpdate() = default;
UiUpdate::~UiUpdate() = default;
UiUpdate::UiUpdate(const UiUpdate&) = default;
UiUpdate& UiUpdate::operator=(const UiUpdate&) = default;
StoreUpdate::StoreUpdate() = default;
StoreUpdate::~StoreUpdate() = default;
StoreUpdate::StoreUpdate(StoreUpdate&&) = default;
StoreUpdate& StoreUpdate::operator=(StoreUpdate&&) = default;

StreamModel::StreamModel(Context* context,
                         const LoggingParameters& logging_parameters)
    : logging_parameters_(logging_parameters),
      content_map_(&(context->revision_generator)) {}

StreamModel::~StreamModel() = default;

void StreamModel::SetStoreObserver(StoreObserver* store_observer) {
  DCHECK(!store_observer || !store_observer_)
      << "Attempting to set store_observer multiple times";
  store_observer_ = store_observer;
}

void StreamModel::SetStreamType(const StreamType& stream_type) {
  stream_type_ = stream_type;
}

const StreamType& StreamModel::GetStreamType() const {
  return stream_type_;
}

void StreamModel::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void StreamModel::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

const feedstore::Content* StreamModel::FindContent(
    ContentRevision revision) const {
  return GetFinalFeatureTree()->FindContent(revision);
}

feedwire::ContentId StreamModel::FindContentId(ContentRevision revision) const {
  const feedstore::Content* content = FindContent(revision);
  return content ? content->content_id() : feedwire::ContentId();
}

const std::string* StreamModel::FindSharedStateData(
    const std::string& id) const {
  auto iter = shared_states_.find(id);
  if (iter != shared_states_.end()) {
    return &iter->second.data;
  }
  return nullptr;
}

std::vector<std::string> StreamModel::GetSharedStateIds() const {
  std::vector<std::string> ids;
  for (auto& entry : shared_states_) {
    ids.push_back(entry.first);
  }
  return ids;
}

void StreamModel::Update(
    std::unique_ptr<StreamModelUpdateRequest> update_request) {
  std::vector<feedstore::StreamStructure>& stream_structures =
      update_request->stream_structures;
  const bool has_clear_all = HasClearAll(stream_structures);

  switch (update_request->source) {
    case StreamModelUpdateRequest::Source::kNetworkUpdate:
      // In this case, the stream state has been saved to persistent
      // storage by the caller. Next sequence number is always 1.
      next_structure_sequence_number_ = 1;
      break;
    case StreamModelUpdateRequest::Source::kInitialLoadFromStore:
      // In this case, use max_structure_sequence_number to derive the next
      // sequence number.
      next_structure_sequence_number_ =
          update_request->max_structure_sequence_number + 1;
      break;
    case StreamModelUpdateRequest::Source::kNetworkLoadMore: {
      // In this case, |StreamModel| is responsible for triggering the update
      // to the store. There are two main cases:
      // 1. The update request has a CLEAR_ALL (this is unexpected).
      //    In this case, we want to overwrite all stored stream data, since
      //    the old data is no longer useful. Start using sequence number 0.
      // 2. The update request does not have a CLEAR_ALL.
      //    Save the new stream data with the next sequence number.
      if (has_clear_all) {
        next_structure_sequence_number_ = 0;
      } else {
        MergeSharedStateIds(stream_data_, update_request->stream_data);
      }
      // Never allow overwriting the root event ID.
      update_request->stream_data.set_root_event_id(
          stream_data_.root_event_id());

      // Note: We might be overwriting some shared-states unnecessarily.
      StoreUpdate store_update;
      store_update.stream_type = stream_type_;
      store_update.overwrite_stream_data = has_clear_all;
      store_update.update_request =
          std::make_unique<StreamModelUpdateRequest>(*update_request);
      store_update.sequence_number = next_structure_sequence_number_++;
      store_observer_->OnStoreChange(std::move(store_update));
      break;
    }
  }

  stream_data_ = update_request->stream_data;

  if (has_clear_all) {
    shared_states_.clear();
  }

  // Update the feature tree.
  for (const feedstore::StreamStructure& structure : stream_structures) {
    base_feature_tree_.ApplyStreamStructure(structure);
  }
  for (feedstore::Content& content : update_request->content) {
    base_feature_tree_.AddContent(std::move(content));
  }

  for (feedstore::StreamSharedState& shared_state :
       update_request->shared_states) {
    std::string id = ContentIdString(shared_state.content_id());
    if (!shared_states_.contains(id)) {
      shared_states_[id].data =
          std::move(*shared_state.mutable_shared_state_data());
    }
  }

  // TODO(harringtond): We're not using StreamData's content_id for anything.

  UpdateFlattenedTree();
}

EphemeralChangeId StreamModel::CreateEphemeralChange(
    std::vector<feedstore::DataOperation> operations) {
  const EphemeralChangeId id =
      ephemeral_changes_.AddEphemeralChange(std::move(operations))->id();

  UpdateFlattenedTree();

  return id;
}

void StreamModel::ExecuteOperations(
    std::vector<feedstore::DataOperation> operations) {
  for (const feedstore::DataOperation& operation : operations) {
    if (operation.has_structure()) {
      base_feature_tree_.ApplyStreamStructure(operation.structure());
    }
    if (operation.has_content()) {
      base_feature_tree_.AddContent(operation.content());
    }
  }

  if (store_observer_) {
    StoreUpdate store_update;
    store_update.stream_type = stream_type_;
    store_update.operations = std::move(operations);
    store_update.sequence_number = next_structure_sequence_number_++;
    store_observer_->OnStoreChange(std::move(store_update));
  }

  UpdateFlattenedTree();
}

bool StreamModel::CommitEphemeralChange(EphemeralChangeId id) {
  std::unique_ptr<stream_model::EphemeralChange> change =
      ephemeral_changes_.Remove(id);
  if (!change)
    return false;

  // Note: it's possible that the does change even upon commit because it
  // may change the order that operations are applied. ExecuteOperations
  // will ensure observers are updated.
  ExecuteOperations(change->GetOperations());
  return true;
}

bool StreamModel::RejectEphemeralChange(EphemeralChangeId id) {
  if (ephemeral_changes_.Remove(id)) {
    UpdateFlattenedTree();
    return true;
  }
  return false;
}

void StreamModel::UpdateFlattenedTree() {
  if (ephemeral_changes_.GetChangeList().empty()) {
    feature_tree_after_changes_.reset();
  } else {
    feature_tree_after_changes_ =
        ApplyEphemeralChanges(base_feature_tree_, ephemeral_changes_);
  }
  // Update list of visible content.
  std::vector<ContentRevision> new_state =
      GetFinalFeatureTree()->GetVisibleContent();
  const bool content_list_changed = content_list_ != new_state;
  content_list_ = std::move(new_state);

  // Pack and send UiUpdate.
  UiUpdate update;
  update.content_list_changed = content_list_changed;
  for (auto& entry : shared_states_) {
    SharedState& shared_state = entry.second;
    UiUpdate::SharedStateInfo info;
    info.shared_state_id = entry.first;
    info.updated = shared_state.updated;
    update.shared_states.push_back(std::move(info));

    shared_state.updated = false;
  }

  for (Observer& observer : observers_)
    observer.OnUiUpdate(update);
}

bool StreamModel::HasVisibleContent() {
  return !content_list_.empty();
}

stream_model::FeatureTree* StreamModel::GetFinalFeatureTree() {
  return feature_tree_after_changes_ ? feature_tree_after_changes_.get()
                                     : &base_feature_tree_;
}
const stream_model::FeatureTree* StreamModel::GetFinalFeatureTree() const {
  return const_cast<StreamModel*>(this)->GetFinalFeatureTree();
}

const std::string& StreamModel::GetNextPageToken() const {
  return stream_data_.next_page_token();
}

base::Time StreamModel::GetLastAddedTime() const {
  return feedstore::GetLastAddedTime(stream_data_);
}
ContentHashSet StreamModel::GetContentIds() const {
  return feedstore::GetContentIds(stream_data_);
}

ContentStats StreamModel::GetContentStats() const {
  ContentStats stats;
  for (auto content_revision : content_list_) {
    const feedstore::Content* content = FindContent(content_revision);
    if (content) {
      stats.total_content_frame_size_bytes += content->frame().size();
      stats.card_count++;
    }
  }

  for (auto& entry : shared_states_) {
    stats.shared_state_size += entry.second.data.size();
  }
  return stats;
}

const std::string& StreamModel::GetRootEventId() const {
  return stream_data_.root_event_id();
}

std::string StreamModel::DumpStateForTesting() {
  std::stringstream ss;
  ss << "StreamModel{\n";
  { ss << "root_event_id=" << base::Base64Encode(GetRootEventId()) << "'\n"; }
  ss << "next_page_token='" << GetNextPageToken() << "'\n";
  for (auto& entry : shared_states_) {
    ss << "shared_state[" << entry.first
       << "]=" << base::GetQuotedJSONString(entry.second.data.substr(0, 100))
       << "\n";
  }
  ss << GetFinalFeatureTree()->DumpStateForTesting();
  ss << "}StreamModel\n";
  return ss.str();
}

}  // namespace feed
