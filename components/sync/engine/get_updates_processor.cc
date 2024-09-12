// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/get_updates_processor.h"

#include <stddef.h>

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/not_fatal_until.h"
#include "base/trace_event/trace_event.h"
#include "components/sync/engine/cycle/status_controller.h"
#include "components/sync/engine/cycle/sync_cycle.h"
#include "components/sync/engine/events/get_updates_response_event.h"
#include "components/sync/engine/get_updates_delegate.h"
#include "components/sync/engine/nigori/keystore_keys_handler.h"
#include "components/sync/engine/syncer_proto_util.h"
#include "components/sync/engine/update_handler.h"
#include "components/sync/protocol/data_type_progress_marker.pb.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync/protocol/sync_entity.pb.h"
#include "net/http/http_status_code.h"
#include "third_party/protobuf/src/google/protobuf/repeated_field.h"

namespace syncer {

namespace {

using SyncEntityList = std::vector<const sync_pb::SyncEntity*>;
using TypeSyncEntityMap = std::map<DataType, SyncEntityList>;
using TypeToIndexMap = std::map<DataType, size_t>;

bool ShouldRequestEncryptionKey(SyncCycleContext* context) {
  return context->data_type_registry()
      ->keystore_keys_handler()
      ->NeedKeystoreKey();
}

std::vector<std::vector<uint8_t>> ExtractKeystoreKeys(
    const sync_pb::ClientToServerResponse& update_response) {
  const google::protobuf::RepeatedPtrField<std::string>& encryption_keys =
      update_response.get_updates().encryption_keys();
  std::vector<std::vector<uint8_t>> keystore_keys;
  keystore_keys.reserve(encryption_keys.size());
  for (const std::string& key : encryption_keys) {
    keystore_keys.emplace_back(key.begin(), key.end());
  }
  return keystore_keys;
}

// Populates keystore encryption keys to KeystoreKeysHandler, returns true on
// success and false otherwise.
bool HandleGetEncryptionKeyResponse(
    const sync_pb::ClientToServerResponse& update_response,
    SyncCycleContext* context) {
  if (update_response.get_updates().encryption_keys_size() == 0) {
    LOG(ERROR) << "Failed to receive encryption key from server.";
    return false;
  }

  std::vector<std::vector<uint8_t>> keystore_keys =
      ExtractKeystoreKeys(update_response);

  bool success =
      context->data_type_registry()->keystore_keys_handler()->SetKeystoreKeys(
          keystore_keys);

  DVLOG(1) << "GetUpdates returned "
           << update_response.get_updates().encryption_keys_size()
           << "encryption keys. Nigori keystore key " << (success ? "" : "not ")
           << "updated.";
  return success;
}

// Given a GetUpdates response, iterates over all the returned items and
// divides them according to their type.  Outputs a map from data types to
// received SyncEntities.  The output map will have entries (possibly empty)
// for all types in |requested_types|.
void PartitionUpdatesByType(const sync_pb::GetUpdatesResponse& gu_response,
                            DataTypeSet requested_types,
                            TypeSyncEntityMap* updates_by_type) {
  for (DataType type : requested_types) {
    updates_by_type->insert(std::make_pair(type, SyncEntityList()));
  }
  for (const sync_pb::SyncEntity& update : gu_response.entries()) {
    DataType type = GetDataTypeFromSpecifics(update.specifics());
    if (!IsRealDataType(type)) {
      NOTREACHED_IN_MIGRATION() << "Received update with invalid type.";
      continue;
    }

    auto it = updates_by_type->find(type);
    if (it == updates_by_type->end()) {
      DLOG(WARNING) << "Received update for unexpected type, or the type is "
                       "throttled or failed with partial failure:"
                    << DataTypeToDebugString(type);
      continue;
    }

    it->second.push_back(&update);
  }
}

// Builds a map of DataTypes to indices to progress markers in the given
// |gu_response| message.  The map is returned in the |index_map| parameter.
void PartitionProgressMarkersByType(
    const sync_pb::GetUpdatesResponse& gu_response,
    const DataTypeSet& request_types,
    TypeToIndexMap* index_map) {
  for (int i = 0; i < gu_response.new_progress_marker_size(); ++i) {
    int field_number = gu_response.new_progress_marker(i).data_type_id();
    DataType data_type = GetDataTypeFromSpecificsFieldNumber(field_number);
    if (!IsRealDataType(data_type)) {
      DLOG(WARNING) << "Unknown field number " << field_number;
      continue;
    }
    if (!request_types.Has(data_type)) {
      DLOG(WARNING)
          << "Skipping unexpected progress marker for non-enabled type "
          << DataTypeToDebugString(data_type);
      continue;
    }
    index_map->insert(std::make_pair(data_type, i));
  }
}

void PartitionContextMutationsByType(
    const sync_pb::GetUpdatesResponse& gu_response,
    const DataTypeSet& request_types,
    TypeToIndexMap* index_map) {
  for (int i = 0; i < gu_response.context_mutations_size(); ++i) {
    int field_number = gu_response.context_mutations(i).data_type_id();
    DataType data_type = GetDataTypeFromSpecificsFieldNumber(field_number);
    if (!IsRealDataType(data_type)) {
      DLOG(WARNING) << "Unknown field number " << field_number;
      continue;
    }
    if (!request_types.Has(data_type)) {
      DLOG(WARNING)
          << "Skipping unexpected context mutation for non-enabled type "
          << DataTypeToDebugString(data_type);
      continue;
    }
    index_map->insert(std::make_pair(data_type, i));
  }
}

// Initializes the parts of the GetUpdatesMessage that depend on shared state,
// like the ShouldRequestEncryptionKey() status.  This is kept separate from the
// other of the message-building functions to make the rest of the code easier
// to test.
void InitDownloadUpdatesContext(SyncCycle* cycle,
                                sync_pb::ClientToServerMessage* message) {
  message->set_share(cycle->context()->account_name());
  message->set_message_contents(sync_pb::ClientToServerMessage::GET_UPDATES);

  sync_pb::GetUpdatesMessage* get_updates = message->mutable_get_updates();

  // We want folders for our associated types, always.  If we were to set
  // this to false, the server would send just the non-container items
  // (e.g. Bookmark URLs but not their containing folders).
  get_updates->set_fetch_folders(true);

  bool need_encryption_key = ShouldRequestEncryptionKey(cycle->context());
  get_updates->set_need_encryption_key(need_encryption_key);

  get_updates->mutable_caller_info()->set_notifications_enabled(
      cycle->context()->notifications_enabled());
}

}  // namespace

GetUpdatesProcessor::GetUpdatesProcessor(UpdateHandlerMap* update_handler_map,
                                         const GetUpdatesDelegate& delegate)
    : update_handler_map_(update_handler_map), delegate_(delegate) {}

GetUpdatesProcessor::~GetUpdatesProcessor() = default;

SyncerError GetUpdatesProcessor::DownloadUpdates(DataTypeSet* request_types,
                                                 SyncCycle* cycle) {
  TRACE_EVENT0("sync", "DownloadUpdates");

  has_more_updates_to_download_ = false;

  sync_pb::ClientToServerMessage message;
  InitDownloadUpdatesContext(cycle, &message);
  PrepareGetUpdates(*request_types, &message);

  SyncerError result = ExecuteDownloadUpdates(request_types, cycle, &message);
  cycle->mutable_status_controller()->set_last_download_updates_result(result);
  return result;
}

void GetUpdatesProcessor::PrepareGetUpdates(
    const DataTypeSet& gu_types,
    sync_pb::ClientToServerMessage* message) {
  sync_pb::GetUpdatesMessage* get_updates = message->mutable_get_updates();

  for (DataType type : gu_types) {
    auto handler_it = update_handler_map_->find(type);
    CHECK(handler_it != update_handler_map_->end(), base::NotFatalUntil::M130)
        << "Failed to look up handler for " << DataTypeToDebugString(type);
    sync_pb::DataTypeProgressMarker* progress_marker =
        get_updates->add_from_progress_marker();
    *progress_marker = handler_it->second->GetDownloadProgress();
    DCHECK(!progress_marker->has_gc_directive());

    sync_pb::DataTypeContext context = handler_it->second->GetDataTypeContext();
    if (!context.context().empty()) {
      *get_updates->add_client_contexts() = std::move(context);
    }
    if (delegate_->IsNotificationInfoRequired()) {
      handler_it->second->CollectPendingInvalidations(
          progress_marker->mutable_get_update_triggers());
    }
  }

  delegate_->HelpPopulateGuMessage(get_updates);
}

SyncerError GetUpdatesProcessor::ExecuteDownloadUpdates(
    DataTypeSet* request_types,
    SyncCycle* cycle,
    sync_pb::ClientToServerMessage* msg) {
  sync_pb::ClientToServerResponse update_response;
  StatusController* status = cycle->mutable_status_controller();
  bool need_encryption_key = ShouldRequestEncryptionKey(cycle->context());

  if (cycle->context()->debug_info_getter()) {
    *msg->mutable_debug_info() =
        cycle->context()->debug_info_getter()->GetDebugInfo();
  }

  SyncerProtoUtil::AddRequiredFieldsToClientToServerMessage(cycle, msg);

  cycle->SendProtocolEvent(
      *(delegate_->GetNetworkRequestEvent(base::Time::Now(), *msg)));

  DataTypeSet partial_failure_data_types;

  SyncerError result = SyncerProtoUtil::PostClientToServerMessage(
      *msg, &update_response, cycle, &partial_failure_data_types);

  DVLOG(2) << SyncerProtoUtil::ClientToServerResponseDebugString(
      update_response);

  if (!partial_failure_data_types.empty()) {
    request_types->RemoveAll(partial_failure_data_types);
  }

  if (result.type() != SyncerError::Type::kSuccess) {
    GetUpdatesResponseEvent response_event(base::Time::Now(), update_response,
                                           result);
    cycle->SendProtocolEvent(response_event);

    // Sync authorization expires every 60 mintues, so SYNC_AUTH_ERROR will
    // appear every 60 minutes, and then sync services will refresh the
    // authorization. Therefore SYNC_AUTH_ERROR is excluded here to reduce the
    // ERROR messages in the log.
    if (result.type() != SyncerError::Type::kHttpError ||
        result.GetHttpErrorOrDie() != net::HTTP_UNAUTHORIZED) {
      LOG(ERROR) << "PostClientToServerMessage() failed during GetUpdates "
                    "with error "
                 << result.ToString();
    }

    return result;
  }

  DVLOG(1) << "GetUpdates returned "
           << update_response.get_updates().entries_size() << " updates.";

  if (cycle->context()->debug_info_getter()) {
    // Clear debug info now that we have successfully sent it to the server.
    DVLOG(1) << "Clearing client debug info.";
    cycle->context()->debug_info_getter()->ClearDebugInfo();
  }

  if (need_encryption_key ||
      update_response.get_updates().encryption_keys_size() > 0) {
    status->set_last_get_key_failed(
        !HandleGetEncryptionKeyResponse(update_response, cycle->context()));
  }

  SyncerError process_result =
      ProcessResponse(update_response.get_updates(), *request_types, status);

  GetUpdatesResponseEvent response_event(base::Time::Now(), update_response,
                                         process_result);
  cycle->SendProtocolEvent(response_event);

  DVLOG(1) << "GetUpdates result: " << process_result.ToString();

  return process_result;
}

SyncerError GetUpdatesProcessor::ProcessResponse(
    const sync_pb::GetUpdatesResponse& gu_response,
    const DataTypeSet& gu_types,
    StatusController* status_controller) {
  status_controller->increment_num_updates_downloaded_by(
      gu_response.entries_size());

  // The changes remaining field is used to prevent the client from looping.  If
  // that field is being set incorrectly, we're in big trouble.
  if (!gu_response.has_changes_remaining()) {
    return SyncerError::ProtocolViolationError();
  }

  TypeSyncEntityMap updates_by_type;
  PartitionUpdatesByType(gu_response, gu_types, &updates_by_type);
  DCHECK_EQ(gu_types.size(), updates_by_type.size());

  TypeToIndexMap progress_index_by_type;
  PartitionProgressMarkersByType(gu_response, gu_types,
                                 &progress_index_by_type);
  if (gu_types.size() != progress_index_by_type.size()) {
    NOTREACHED_IN_MIGRATION()
        << "Missing progress markers in GetUpdates response.";
    return SyncerError::ProtocolViolationError();
  }

  TypeToIndexMap context_by_type;
  PartitionContextMutationsByType(gu_response, gu_types, &context_by_type);

  // Iterate over these maps in parallel, processing updates for each type.
  auto progress_marker_iter = progress_index_by_type.begin();
  auto updates_iter = updates_by_type.begin();
  for (; (progress_marker_iter != progress_index_by_type.end() &&
          updates_iter != updates_by_type.end());
       ++progress_marker_iter, ++updates_iter) {
    DCHECK_EQ(progress_marker_iter->first, updates_iter->first);
    DataType type = progress_marker_iter->first;

    auto update_handler_iter = update_handler_map_->find(type);

    sync_pb::DataTypeContext context;
    auto context_iter = context_by_type.find(type);
    if (context_iter != context_by_type.end()) {
      context.CopyFrom(gu_response.context_mutations(context_iter->second));
    }

    if (update_handler_iter != update_handler_map_->end()) {
      update_handler_iter->second->ProcessGetUpdatesResponse(
          gu_response.new_progress_marker(progress_marker_iter->second),
          context, updates_iter->second, status_controller);
    } else {
      DLOG(WARNING) << "Ignoring received updates of a type we can't handle.  "
                    << "Type is: " << DataTypeToDebugString(type);
      continue;
    }
  }
  DCHECK(progress_marker_iter == progress_index_by_type.end() &&
         updates_iter == updates_by_type.end());

  has_more_updates_to_download_ = gu_response.changes_remaining() != 0;
  return SyncerError::Success();
}

void GetUpdatesProcessor::ApplyUpdates(const DataTypeSet& gu_types,
                                       StatusController* status_controller) {
  for (const auto& [type, update_handler] : *update_handler_map_) {
    if (gu_types.Has(type)) {
      update_handler->ApplyUpdates(status_controller, /*cycle_done=*/true);
    }
  }
}

bool GetUpdatesProcessor::HasMoreUpdatesToDownload() const {
  return has_more_updates_to_download_;
}

}  // namespace syncer
