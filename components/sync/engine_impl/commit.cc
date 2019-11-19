// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine_impl/commit.h"

#include <utility>

#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/trace_event/trace_event.h"
#include "components/sync/base/data_type_histogram.h"
#include "components/sync/engine_impl/commit_processor.h"
#include "components/sync/engine_impl/commit_util.h"
#include "components/sync/engine_impl/cycle/sync_cycle.h"
#include "components/sync/engine_impl/events/commit_request_event.h"
#include "components/sync/engine_impl/events/commit_response_event.h"
#include "components/sync/engine_impl/syncer.h"
#include "components/sync/engine_impl/syncer_proto_util.h"

namespace syncer {

namespace {
// The number of random ASCII bytes we'll add to CommitMessage. We choose 256
// because it is not too large (to hurt performance and compression ratio), but
// it is not too small to easily be canceled out using statistical analysis.
const size_t kPaddingSize = 256;

std::string RandASCIIString(size_t length) {
  std::string result;
  const int kMin = static_cast<int>(' ');
  const int kMax = static_cast<int>('~');
  result.reserve(length);
  for (size_t i = 0; i < length; ++i)
    result.push_back(static_cast<char>(base::RandInt(kMin, kMax)));
  return result;
}

}  // namespace

Commit::Commit(ContributionMap contributions,
               const sync_pb::ClientToServerMessage& message,
               ExtensionsActivity::Records extensions_activity_buffer)
    : contributions_(std::move(contributions)),
      message_(message),
      extensions_activity_buffer_(extensions_activity_buffer),
      cleaned_up_(false) {}

Commit::~Commit() {
  DCHECK(cleaned_up_);
}

// static
std::unique_ptr<Commit> Commit::Init(ModelTypeSet requested_types,
                                     ModelTypeSet enabled_types,
                                     size_t max_entries,
                                     const std::string& account_name,
                                     const std::string& cache_guid,
                                     bool cookie_jar_mismatch,
                                     bool cookie_jar_empty,
                                     CommitProcessor* commit_processor,
                                     ExtensionsActivity* extensions_activity) {
  // Gather per-type contributions.
  ContributionMap contributions = commit_processor->GatherCommitContributions(
      requested_types, max_entries, cookie_jar_mismatch, cookie_jar_empty);

  // Give up if no one had anything to commit.
  if (contributions.empty())
    return nullptr;

  sync_pb::ClientToServerMessage message;
  message.set_message_contents(sync_pb::ClientToServerMessage::COMMIT);
  message.set_share(account_name);

  sync_pb::CommitMessage* commit_message = message.mutable_commit();
  commit_message->set_cache_guid(cache_guid);

  // Set padding to mitigate CRIME attack.
  commit_message->set_padding(RandASCIIString(kPaddingSize));

  // Set extensions activity if bookmark commits are present.
  ExtensionsActivity::Records extensions_activity_buffer;
  if (extensions_activity != nullptr) {
    ContributionMap::const_iterator it = contributions.find(BOOKMARKS);
    if (it != contributions.end() && it->second->GetNumEntries() != 0) {
      commit_util::AddExtensionsActivityToMessage(
          extensions_activity, &extensions_activity_buffer, commit_message);
    }
  }

  // Set the client config params.
  commit_util::AddClientConfigParamsToMessage(
      enabled_types, cookie_jar_mismatch, commit_message);

  // Finally, serialize all our contributions.
  for (const auto& contribution : contributions) {
    contribution.second->AddToCommitMessage(&message);
  }

  // If we made it this far, then we've successfully prepared a commit message.
  return std::make_unique<Commit>(std::move(contributions), message,
                                  extensions_activity_buffer);
}

SyncerError Commit::PostAndProcessResponse(
    NudgeTracker* nudge_tracker,
    SyncCycle* cycle,
    StatusController* status,
    ExtensionsActivity* extensions_activity) {
  ModelTypeSet request_types;
  for (ContributionMap::const_iterator it = contributions_.begin();
       it != contributions_.end(); ++it) {
    ModelType request_type = it->first;
    request_types.Put(request_type);
    UMA_HISTOGRAM_ENUMERATION("Sync.PostedDataTypeCommitRequest",
                              ModelTypeHistogramValue(request_type));
  }

  if (cycle->context()->debug_info_getter()) {
    sync_pb::DebugInfo* debug_info = message_.mutable_debug_info();
    cycle->context()->debug_info_getter()->GetDebugInfo(debug_info);
  }

  DVLOG(1) << "Sending commit message.";
  SyncerProtoUtil::AddRequiredFieldsToClientToServerMessage(cycle, &message_);

  CommitRequestEvent request_event(base::Time::Now(),
                                   message_.commit().entries_size(),
                                   request_types, message_);
  cycle->SendProtocolEvent(request_event);

  TRACE_EVENT_BEGIN0("sync", "PostCommit");
  sync_pb::ClientToServerResponse response;
  const SyncerError post_result = SyncerProtoUtil::PostClientToServerMessage(
      message_, &response, cycle, nullptr);
  TRACE_EVENT_END0("sync", "PostCommit");

  // TODO(rlarocque): Use result that includes errors captured later?
  CommitResponseEvent response_event(base::Time::Now(), post_result, response);
  cycle->SendProtocolEvent(response_event);

  if (post_result.value() != SyncerError::SYNCER_OK) {
    LOG(WARNING) << "Post commit failed";
    return post_result;
  }

  if (!response.has_commit()) {
    LOG(WARNING) << "Commit response has no commit body!";
    return SyncerError(SyncerError::SERVER_RESPONSE_VALIDATION_FAILED);
  }

  size_t message_entries = message_.commit().entries_size();
  size_t response_entries = response.commit().entryresponse_size();
  if (message_entries != response_entries) {
    LOG(ERROR) << "Commit response has wrong number of entries! "
               << "Expected: " << message_entries << ", "
               << "Got: " << response_entries;
    return SyncerError(SyncerError::SERVER_RESPONSE_VALIDATION_FAILED);
  }

  if (cycle->context()->debug_info_getter()) {
    // Clear debug info now that we have successfully sent it to the server.
    DVLOG(1) << "Clearing client debug info.";
    cycle->context()->debug_info_getter()->ClearDebugInfo();
  }

  // Let the contributors process the responses to each of their requests.
  SyncerError processing_result = SyncerError(SyncerError::SYNCER_OK);
  for (ContributionMap::const_iterator it = contributions_.begin();
       it != contributions_.end(); ++it) {
    TRACE_EVENT1("sync", "ProcessCommitResponse", "type",
                 ModelTypeToString(it->first));
    SyncerError type_result =
        it->second->ProcessCommitResponse(response, status);
    if (type_result.value() == SyncerError::SERVER_RETURN_CONFLICT) {
      nudge_tracker->RecordCommitConflict(it->first);
    }
    if (processing_result.value() == SyncerError::SYNCER_OK &&
        type_result.value() != SyncerError::SYNCER_OK) {
      processing_result = type_result;
    }
  }

  // Handle bookmarks' special extensions activity stats.
  if (extensions_activity != nullptr &&
      cycle->status_controller()
              .model_neutral_state()
              .num_successful_bookmark_commits == 0) {
    extensions_activity->PutRecords(extensions_activity_buffer_);
  }

  return processing_result;
}

void Commit::CleanUp() {
  for (ContributionMap::const_iterator it = contributions_.begin();
       it != contributions_.end(); ++it) {
    it->second->CleanUp();
  }
  cleaned_up_ = true;
}

}  // namespace syncer
