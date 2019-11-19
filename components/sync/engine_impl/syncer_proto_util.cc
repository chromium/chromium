// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine_impl/syncer_proto_util.h"

#include <map>

#include "base/format_macros.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/time.h"
#include "components/sync/engine_impl/cycle/sync_cycle_context.h"
#include "components/sync/engine_impl/net/server_connection_manager.h"
#include "components/sync/engine_impl/syncer.h"
#include "components/sync/engine_impl/syncer_types.h"
#include "components/sync/engine_impl/traffic_logger.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "components/sync/protocol/sync_protocol_error.h"
#include "components/sync/syncable/entry.h"
#include "components/sync/syncable/syncable_proto_util.h"
#include "google_apis/google_api_keys.h"

using std::string;
using std::stringstream;
using sync_pb::ClientToServerMessage;
using sync_pb::ClientToServerResponse;

namespace syncer {

using syncable::BASE_VERSION;
using syncable::CTIME;
using syncable::ID;
using syncable::IS_DEL;
using syncable::IS_DIR;
using syncable::IS_UNSYNCED;
using syncable::MTIME;
using syncable::PARENT_ID;

namespace {

// Time to backoff syncing after receiving a throttled response.
const int kSyncDelayAfterThrottled = 2 * 60 * 60;  // 2 hours

void LogResponseProfilingData(const ClientToServerResponse& response) {
  if (response.has_profiling_data()) {
    stringstream response_trace;
    response_trace << "Server response trace:";

    if (response.profiling_data().has_user_lookup_time()) {
      response_trace << " user lookup: "
                     << response.profiling_data().user_lookup_time() << "ms";
    }

    if (response.profiling_data().has_meta_data_write_time()) {
      response_trace << " meta write: "
                     << response.profiling_data().meta_data_write_time()
                     << "ms";
    }

    if (response.profiling_data().has_meta_data_read_time()) {
      response_trace << " meta read: "
                     << response.profiling_data().meta_data_read_time() << "ms";
    }

    if (response.profiling_data().has_file_data_write_time()) {
      response_trace << " file write: "
                     << response.profiling_data().file_data_write_time()
                     << "ms";
    }

    if (response.profiling_data().has_file_data_read_time()) {
      response_trace << " file read: "
                     << response.profiling_data().file_data_read_time() << "ms";
    }

    if (response.profiling_data().has_total_request_time()) {
      response_trace << " total time: "
                     << response.profiling_data().total_request_time() << "ms";
    }
    DVLOG(1) << response_trace.str();
  }
}

SyncerError ServerConnectionErrorAsSyncerError(
    const HttpResponse::ServerConnectionCode server_status,
    int net_error_code,
    int http_status_code) {
  switch (server_status) {
    case HttpResponse::CONNECTION_UNAVAILABLE:
      return SyncerError::NetworkConnectionUnavailable(net_error_code);
    case HttpResponse::IO_ERROR:
      return SyncerError(SyncerError::NETWORK_IO_ERROR);
    case HttpResponse::SYNC_SERVER_ERROR:
      // This means the server returned a non-401 HTTP error.
      return SyncerError::HttpError(http_status_code);
    case HttpResponse::SYNC_AUTH_ERROR:
      // This means the server returned an HTTP 401 (unauthorized) error.
      return SyncerError::HttpError(http_status_code);
    case HttpResponse::SERVER_CONNECTION_OK:
    case HttpResponse::NONE:
    default:
      NOTREACHED();
      return SyncerError();
  }
}

SyncProtocolErrorType PBErrorTypeToSyncProtocolErrorType(
    const sync_pb::SyncEnums::ErrorType& error_type) {
  switch (error_type) {
    case sync_pb::SyncEnums::SUCCESS:
      return SYNC_SUCCESS;
    case sync_pb::SyncEnums::NOT_MY_BIRTHDAY:
      return NOT_MY_BIRTHDAY;
    case sync_pb::SyncEnums::THROTTLED:
      return THROTTLED;
    case sync_pb::SyncEnums::CLEAR_PENDING:
      return CLEAR_PENDING;
    case sync_pb::SyncEnums::TRANSIENT_ERROR:
      return TRANSIENT_ERROR;
    case sync_pb::SyncEnums::MIGRATION_DONE:
      return MIGRATION_DONE;
    case sync_pb::SyncEnums::DISABLED_BY_ADMIN:
      return DISABLED_BY_ADMIN;
    case sync_pb::SyncEnums::PARTIAL_FAILURE:
      return PARTIAL_FAILURE;
    case sync_pb::SyncEnums::CLIENT_DATA_OBSOLETE:
      return CLIENT_DATA_OBSOLETE;
    case sync_pb::SyncEnums::UNKNOWN:
      return UNKNOWN_ERROR;
    default:
      NOTREACHED();
      return UNKNOWN_ERROR;
  }
}

ClientAction PBActionToClientAction(const sync_pb::SyncEnums::Action& action) {
  switch (action) {
    case sync_pb::SyncEnums::UPGRADE_CLIENT:
      return UPGRADE_CLIENT;
    case sync_pb::SyncEnums::DEPRECATED_CLEAR_USER_DATA_AND_RESYNC:
    case sync_pb::SyncEnums::DEPRECATED_ENABLE_SYNC_ON_ACCOUNT:
    case sync_pb::SyncEnums::DEPRECATED_STOP_AND_RESTART_SYNC:
    case sync_pb::SyncEnums::DEPRECATED_DISABLE_SYNC_ON_CLIENT:
    case sync_pb::SyncEnums::UNKNOWN_ACTION:
      return UNKNOWN_ACTION;
    default:
      NOTREACHED();
      return UNKNOWN_ACTION;
  }
}

// Returns true iff |message| is an initial GetUpdates request.
bool IsVeryFirstGetUpdates(const ClientToServerMessage& message) {
  if (!message.has_get_updates())
    return false;
  DCHECK_LT(0, message.get_updates().from_progress_marker_size());
  for (int i = 0; i < message.get_updates().from_progress_marker_size(); ++i) {
    if (!message.get_updates().from_progress_marker(i).token().empty())
      return false;
  }
  return true;
}

// Returns true iff |message| should contain a store birthday.
bool IsBirthdayRequired(const ClientToServerMessage& message) {
  if (message.has_clear_server_data())
    return false;
  if (message.has_commit())
    return true;
  if (message.has_get_updates())
    return !IsVeryFirstGetUpdates(message);
  NOTIMPLEMENTED();
  return true;
}

SyncProtocolError ErrorCodeToSyncProtocolError(
    const sync_pb::SyncEnums::ErrorType& error_type) {
  SyncProtocolError error;
  error.error_type = PBErrorTypeToSyncProtocolErrorType(error_type);
  if (error_type == sync_pb::SyncEnums::CLEAR_PENDING ||
      error_type == sync_pb::SyncEnums::NOT_MY_BIRTHDAY) {
    error.action = DISABLE_SYNC_ON_CLIENT;
  } else if (error_type == sync_pb::SyncEnums::CLIENT_DATA_OBSOLETE) {
    error.action = RESET_LOCAL_SYNC_DATA;
  } else if (error_type == sync_pb::SyncEnums::DISABLED_BY_ADMIN) {
    error.action = STOP_SYNC_FOR_DISABLED_ACCOUNT;
  }  // There is no other action we can compute for legacy server.
  return error;
}

// Verifies the store birthday, alerting/resetting as appropriate if there's a
// mismatch. Return false if the syncer should be stuck.
bool ProcessResponseBirthday(const ClientToServerResponse& response,
                             SyncCycleContext* context) {
  const std::string& local_birthday = context->birthday();

  if (local_birthday.empty()) {
    if (!response.has_store_birthday()) {
      DLOG(WARNING) << "Expected a birthday on first sync.";
      return false;
    }

    DVLOG(1) << "New store birthday: " << response.store_birthday();
    context->set_birthday(response.store_birthday());
    return true;
  }

  // Error situation, but we're not stuck.
  if (!response.has_store_birthday()) {
    DLOG(WARNING) << "No birthday in server response?";
    return true;
  }

  if (response.store_birthday() != local_birthday) {
    DLOG(WARNING) << "Birthday changed, showing syncer stuck";
    return false;
  }

  return true;
}

void SaveBagOfChipsFromResponse(const sync_pb::ClientToServerResponse& response,
                                SyncCycleContext* context) {
  if (!response.has_new_bag_of_chips())
    return;
  std::string bag_of_chips;
  if (response.new_bag_of_chips().SerializeToString(&bag_of_chips))
    context->set_bag_of_chips(bag_of_chips);
}

}  // namespace

ModelTypeSet GetTypesToMigrate(const ClientToServerResponse& response) {
  ModelTypeSet to_migrate;
  for (int i = 0; i < response.migrated_data_type_id_size(); i++) {
    int field_number = response.migrated_data_type_id(i);
    ModelType model_type = GetModelTypeFromSpecificsFieldNumber(field_number);
    if (!IsRealDataType(model_type)) {
      DLOG(WARNING) << "Unknown field number " << field_number;
      continue;
    }
    to_migrate.Put(model_type);
  }
  return to_migrate;
}

SyncProtocolError ConvertErrorPBToSyncProtocolError(
    const sync_pb::ClientToServerResponse_Error& error) {
  SyncProtocolError sync_protocol_error;
  sync_protocol_error.error_type =
      PBErrorTypeToSyncProtocolErrorType(error.error_type());
  sync_protocol_error.error_description = error.error_description();
  sync_protocol_error.url = error.url();
  sync_protocol_error.action = PBActionToClientAction(error.action());

  if (error.error_data_type_ids_size() > 0) {
    // THROTTLED and PARTIAL_FAILURE are currently the only error codes
    // that uses |error_data_types|.
    // In both cases, |error_data_types| are throttled.
    for (int i = 0; i < error.error_data_type_ids_size(); ++i) {
      int field_number = error.error_data_type_ids(i);
      ModelType model_type = GetModelTypeFromSpecificsFieldNumber(field_number);
      if (!IsRealDataType(model_type)) {
        DLOG(WARNING) << "Unknown field number " << field_number;
        continue;
      }
      sync_protocol_error.error_data_types.Put(model_type);
    }
  }

  return sync_protocol_error;
}

// static
bool SyncerProtoUtil::IsSyncDisabledByAdmin(
    const sync_pb::ClientToServerResponse& response) {
  return (response.has_error_code() &&
          response.error_code() == sync_pb::SyncEnums::DISABLED_BY_ADMIN);
}

// static
SyncProtocolError SyncerProtoUtil::GetProtocolErrorFromResponse(
    const sync_pb::ClientToServerResponse& response,
    SyncCycleContext* context) {
  SyncProtocolError sync_protocol_error;

  // The DISABLED_BY_ADMIN error overrides other errors sent by the server.
  if (IsSyncDisabledByAdmin(response)) {
    sync_protocol_error.error_type = DISABLED_BY_ADMIN;
    sync_protocol_error.action = STOP_SYNC_FOR_DISABLED_ACCOUNT;
  } else if (!ProcessResponseBirthday(response, context)) {
    // If sync isn't disabled, first check for a birthday mismatch error.
    if (response.error_code() == sync_pb::SyncEnums::CLIENT_DATA_OBSOLETE) {
      // Server indicates that client needs to reset sync data.
      sync_protocol_error.error_type = CLIENT_DATA_OBSOLETE;
      sync_protocol_error.action = RESET_LOCAL_SYNC_DATA;
    } else {
      sync_protocol_error.error_type = NOT_MY_BIRTHDAY;
      sync_protocol_error.action = DISABLE_SYNC_ON_CLIENT;
    }
  } else if (response.has_error()) {
    // This is a new server. Just get the error from the protocol.
    sync_protocol_error = ConvertErrorPBToSyncProtocolError(response.error());
  } else {
    // Legacy server implementation. Compute the error based on |error_code|.
    sync_protocol_error = ErrorCodeToSyncProtocolError(response.error_code());
  }
  return sync_protocol_error;
}

// static
void SyncerProtoUtil::SetProtocolVersion(ClientToServerMessage* msg) {
  const int current_version =
      ClientToServerMessage::default_instance().protocol_version();
  msg->set_protocol_version(current_version);
}

// static
bool SyncerProtoUtil::PostAndProcessHeaders(ServerConnectionManager* scm,
                                            SyncCycle* cycle,
                                            const ClientToServerMessage& msg,
                                            ClientToServerResponse* response) {
  ServerConnectionManager::PostBufferParams params;
  DCHECK(msg.has_protocol_version());
  DCHECK_EQ(msg.protocol_version(),
            ClientToServerMessage::default_instance().protocol_version());
  msg.SerializeToString(&params.buffer_in);

  UMA_HISTOGRAM_ENUMERATION("Sync.PostedClientToServerMessage",
                            msg.message_contents(),
                            ClientToServerMessage::Contents_MAX + 1);

  std::map<int, std::string> progress_marker_token_per_data_type;

  if (msg.has_get_updates()) {
    UMA_HISTOGRAM_ENUMERATION("Sync.PostedGetUpdatesOrigin",
                              msg.get_updates().get_updates_origin(),
                              sync_pb::SyncEnums::GetUpdatesOrigin_ARRAYSIZE);

    for (const sync_pb::DataTypeProgressMarker& progress_marker :
         msg.get_updates().from_progress_marker()) {
      progress_marker_token_per_data_type[progress_marker.data_type_id()] =
          progress_marker.token();
      UMA_HISTOGRAM_ENUMERATION(
          "Sync.PostedDataTypeGetUpdatesRequest",
          ModelTypeHistogramValue(GetModelTypeFromSpecificsFieldNumber(
              progress_marker.data_type_id())));
    }
  }

  const base::Time start_time = base::Time::Now();

  // Fills in params.buffer_out and params.response.
  if (!scm->PostBufferWithCachedAuth(&params)) {
    LOG(WARNING) << "Error posting from syncer:" << params.response;
    return false;
  }

  if (!response->ParseFromString(params.buffer_out)) {
    DLOG(WARNING) << "Error parsing response from sync server";
    return false;
  }

  UMA_HISTOGRAM_MEDIUM_TIMES("Sync.PostedClientToServerMessageLatency",
                             base::Time::Now() - start_time);

  if (response->error_code() != sync_pb::SyncEnums::SUCCESS) {
    base::UmaHistogramSparse("Sync.PostedClientToServerMessageError",
                             response->error_code());
  }

  return true;
}

base::TimeDelta SyncerProtoUtil::GetThrottleDelay(
    const ClientToServerResponse& response) {
  base::TimeDelta throttle_delay =
      base::TimeDelta::FromSeconds(kSyncDelayAfterThrottled);
  if (response.has_client_command()) {
    const sync_pb::ClientCommand& command = response.client_command();
    if (command.has_throttle_delay_seconds()) {
      throttle_delay =
          base::TimeDelta::FromSeconds(command.throttle_delay_seconds());
    }
  }
  return throttle_delay;
}

// static
void SyncerProtoUtil::AddRequiredFieldsToClientToServerMessage(
    const SyncCycle* cycle,
    sync_pb::ClientToServerMessage* msg) {
  DCHECK(msg);
  SetProtocolVersion(msg);
  const std::string birthday = cycle->context()->birthday();
  if (!birthday.empty())
    msg->set_store_birthday(birthday);
  DCHECK(msg->has_store_birthday() || !IsBirthdayRequired(*msg));
  msg->mutable_bag_of_chips()->ParseFromString(
      cycle->context()->bag_of_chips());
  msg->set_api_key(google_apis::GetAPIKey());
  msg->mutable_client_status()->CopyFrom(cycle->context()->client_status());
  msg->set_invalidator_client_id(cycle->context()->invalidator_client_id());
}

// static
SyncerError SyncerProtoUtil::PostClientToServerMessage(
    const ClientToServerMessage& msg,
    ClientToServerResponse* response,
    SyncCycle* cycle,
    ModelTypeSet* partial_failure_data_types) {
  DCHECK(response);
  DCHECK(msg.has_protocol_version());
  DCHECK(msg.has_store_birthday() || !IsBirthdayRequired(msg));
  DCHECK(msg.has_bag_of_chips());
  DCHECK(msg.has_api_key());
  DCHECK(msg.has_client_status());
  DCHECK(msg.has_invalidator_client_id());

  LogClientToServerMessage(msg);
  if (!PostAndProcessHeaders(cycle->context()->connection_manager(), cycle, msg,
                             response)) {
    // There was an error establishing communication with the server.
    // We can not proceed beyond this point.
    const HttpResponse::ServerConnectionCode server_status =
        cycle->context()->connection_manager()->server_status();

    DCHECK_NE(server_status, HttpResponse::NONE);
    DCHECK_NE(server_status, HttpResponse::SERVER_CONNECTION_OK);

    return ServerConnectionErrorAsSyncerError(
        server_status, cycle->context()->connection_manager()->net_error_code(),
        cycle->context()->connection_manager()->http_status_code());
  }
  LogClientToServerResponse(*response);

  // Remember a bag of chips if it has been sent by the server.
  SaveBagOfChipsFromResponse(*response, cycle->context());

  SyncProtocolError sync_protocol_error =
      GetProtocolErrorFromResponse(*response, cycle->context());

  // Inform the delegate of the error we got.
  cycle->delegate()->OnSyncProtocolError(sync_protocol_error);

  // Update our state for any other commands we've received.
  if (response->has_client_command()) {
    const sync_pb::ClientCommand& command = response->client_command();
    if (command.has_max_commit_batch_size()) {
      cycle->context()->set_max_commit_batch_size(
          command.max_commit_batch_size());
    }

    if (command.has_set_sync_poll_interval()) {
      base::TimeDelta interval =
          base::TimeDelta::FromSeconds(command.set_sync_poll_interval());
      if (interval.is_zero()) {
        DLOG(WARNING) << "Received zero poll interval from server. Ignoring.";
      } else {
        cycle->context()->set_poll_interval(interval);
        cycle->delegate()->OnReceivedPollIntervalUpdate(interval);
      }
    }

    if (command.has_sessions_commit_delay_seconds()) {
      std::map<ModelType, base::TimeDelta> delay_map;
      delay_map[SESSIONS] =
          base::TimeDelta::FromSeconds(command.sessions_commit_delay_seconds());
      delay_map[FAVICON_TRACKING] =
          base::TimeDelta::FromSeconds(command.sessions_commit_delay_seconds());
      delay_map[FAVICON_IMAGES] =
          base::TimeDelta::FromSeconds(command.sessions_commit_delay_seconds());
      cycle->delegate()->OnReceivedCustomNudgeDelays(delay_map);
    }

    if (command.has_client_invalidation_hint_buffer_size()) {
      cycle->delegate()->OnReceivedClientInvalidationHintBufferSize(
          command.client_invalidation_hint_buffer_size());
    }

    if (command.has_gu_retry_delay_seconds()) {
      cycle->delegate()->OnReceivedGuRetryDelay(
          base::TimeDelta::FromSeconds(command.gu_retry_delay_seconds()));
    }

    if (command.custom_nudge_delays_size() > 0) {
      // Note that because this happens after the sessions_commit_delay_seconds
      // handling, any SESSIONS value in this map will override the one in
      // sessions_commit_delay_seconds.
      std::map<ModelType, base::TimeDelta> delay_map;
      for (int i = 0; i < command.custom_nudge_delays_size(); ++i) {
        ModelType type = GetModelTypeFromSpecificsFieldNumber(
            command.custom_nudge_delays(i).datatype_id());
        if (ProtocolTypes().Has(type)) {
          delay_map[type] = base::TimeDelta::FromMilliseconds(
              command.custom_nudge_delays(i).delay_ms());
        }
      }
      cycle->delegate()->OnReceivedCustomNudgeDelays(delay_map);
    }
  }

  // Now do any special handling for the error type and decide on the return
  // value.
  switch (sync_protocol_error.error_type) {
    case UNKNOWN_ERROR:
      LOG(WARNING) << "Sync protocol out-of-date. The server is using a more "
                   << "recent version.";
      return SyncerError(SyncerError::SERVER_RETURN_UNKNOWN_ERROR);
    case SYNC_SUCCESS:
      LogResponseProfilingData(*response);
      return SyncerError(SyncerError::SYNCER_OK);
    case THROTTLED:
      if (sync_protocol_error.error_data_types.Empty()) {
        DLOG(WARNING) << "Client fully throttled by syncer.";
        cycle->delegate()->OnThrottled(GetThrottleDelay(*response));
      } else {
        // This is a special case, since server only throttle some of datatype,
        // so can treat this case as partial failure.
        DLOG(WARNING) << "Some types throttled by syncer.";
        cycle->delegate()->OnTypesThrottled(
            sync_protocol_error.error_data_types, GetThrottleDelay(*response));
        if (partial_failure_data_types != nullptr) {
          *partial_failure_data_types = sync_protocol_error.error_data_types;
        }
        return SyncerError(SyncerError::SYNCER_OK);
      }
      return SyncerError(SyncerError::SERVER_RETURN_THROTTLED);
    case TRANSIENT_ERROR:
      return SyncerError(SyncerError::SERVER_RETURN_TRANSIENT_ERROR);
    case MIGRATION_DONE:
      LOG_IF(ERROR, 0 >= response->migrated_data_type_id_size())
          << "MIGRATION_DONE but no types specified.";
      cycle->delegate()->OnReceivedMigrationRequest(
          GetTypesToMigrate(*response));
      return SyncerError(SyncerError::SERVER_RETURN_MIGRATION_DONE);
    case CLEAR_PENDING:
      return SyncerError(SyncerError::SERVER_RETURN_CLEAR_PENDING);
    case NOT_MY_BIRTHDAY:
      return SyncerError(SyncerError::SERVER_RETURN_NOT_MY_BIRTHDAY);
    case DISABLED_BY_ADMIN:
      return SyncerError(SyncerError::SERVER_RETURN_DISABLED_BY_ADMIN);
    case PARTIAL_FAILURE:
      // This only happens when partial backoff during GetUpdates.
      if (!sync_protocol_error.error_data_types.Empty()) {
        DLOG(WARNING)
            << "Some types got partial failure by syncer during GetUpdates.";
        cycle->delegate()->OnTypesBackedOff(
            sync_protocol_error.error_data_types);
      }
      if (partial_failure_data_types != nullptr) {
        *partial_failure_data_types = sync_protocol_error.error_data_types;
      }
      return SyncerError(SyncerError::SYNCER_OK);
    case CLIENT_DATA_OBSOLETE:
      return SyncerError(SyncerError::SERVER_RETURN_CLIENT_DATA_OBSOLETE);
    default:
      NOTREACHED();
      return SyncerError();
  }
}

// static
bool SyncerProtoUtil::ShouldMaintainPosition(
    const sync_pb::SyncEntity& sync_entity) {
  // Maintain positions for bookmarks that are not server-defined top-level
  // folders.
  return GetModelType(sync_entity) == BOOKMARKS &&
         !(sync_entity.folder() &&
           !sync_entity.server_defined_unique_tag().empty());
}

// static
bool SyncerProtoUtil::ShouldMaintainHierarchy(
    const sync_pb::SyncEntity& sync_entity) {
  // Maintain hierarchy for bookmarks or top-level items.
  return GetModelType(sync_entity) == BOOKMARKS ||
         sync_entity.parent_id_string() == "0";
}

// static
const std::string& SyncerProtoUtil::NameFromSyncEntity(
    const sync_pb::SyncEntity& entry) {
  if (entry.has_non_unique_name())
    return entry.non_unique_name();
  return entry.name();
}

// static
const std::string& SyncerProtoUtil::NameFromCommitEntryResponse(
    const sync_pb::CommitResponse_EntryResponse& entry) {
  if (entry.has_non_unique_name())
    return entry.non_unique_name();
  return entry.name();
}

std::string SyncerProtoUtil::SyncEntityDebugString(
    const sync_pb::SyncEntity& entry) {
  const std::string& mtime_str =
      GetTimeDebugString(ProtoTimeToTime(entry.mtime()));
  const std::string& ctime_str =
      GetTimeDebugString(ProtoTimeToTime(entry.ctime()));
  return base::StringPrintf(
      "id: %s, parent_id: %s, "
      "version: %" PRId64
      "d, "
      "mtime: %" PRId64
      "d (%s), "
      "ctime: %" PRId64
      "d (%s), "
      "name: %s, "
      "d, "
      "%s ",
      entry.id_string().c_str(), entry.parent_id_string().c_str(),
      entry.version(), entry.mtime(), mtime_str.c_str(), entry.ctime(),
      ctime_str.c_str(), entry.name().c_str(),
      entry.deleted() ? "deleted, " : "");
}

namespace {
std::string GetUpdatesResponseString(
    const sync_pb::GetUpdatesResponse& response) {
  std::string output;
  output.append("GetUpdatesResponse:\n");
  for (int i = 0; i < response.entries_size(); i++) {
    output.append(SyncerProtoUtil::SyncEntityDebugString(response.entries(i)));
    output.append("\n");
  }
  return output;
}
}  // namespace

std::string SyncerProtoUtil::ClientToServerResponseDebugString(
    const ClientToServerResponse& response) {
  // Add more handlers as needed.
  std::string output;
  if (response.has_get_updates())
    output.append(GetUpdatesResponseString(response.get_updates()));
  return output;
}

}  // namespace syncer
