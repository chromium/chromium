// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/syncer_proto_util.h"

#include <map>
#include <optional>

#include "base/format_macros.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/features.h"
#include "components/sync/base/time.h"
#include "components/sync/engine/cycle/sync_cycle_context.h"
#include "components/sync/engine/net/server_connection_manager.h"
#include "components/sync/engine/sync_protocol_error.h"
#include "components/sync/engine/syncer.h"
#include "components/sync/engine/traffic_logger.h"
#include "components/sync/protocol/data_type_progress_marker.pb.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "google_apis/google_api_keys.h"
#include "net/http/http_status_code.h"

using std::string;
using std::stringstream;
using sync_pb::ClientToServerMessage;
using sync_pb::ClientToServerResponse;

namespace syncer {
namespace {

// Time to backoff syncing after receiving a throttled response.
constexpr base::TimeDelta kSyncDelayAfterThrottled = base::Hours(2);

SyncerError ServerConnectionErrorAsSyncerError(
    const HttpResponse::ServerConnectionCode server_status,
    int net_error_code,
    int http_status_code) {
  switch (server_status) {
    case HttpResponse::CONNECTION_UNAVAILABLE:
      return SyncerError::NetworkError(net_error_code);
    case HttpResponse::SYNC_SERVER_ERROR:
    case HttpResponse::SYNC_AUTH_ERROR:
      // This means the server returned an HTTP error.
      return SyncerError::HttpError(
          static_cast<net::HttpStatusCode>(http_status_code));
    case HttpResponse::SERVER_CONNECTION_OK:
    case HttpResponse::NONE:
      NOTREACHED_IN_MIGRATION();
      return SyncerError::Success();
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
    case sync_pb::SyncEnums::ENCRYPTION_OBSOLETE:
      return ENCRYPTION_OBSOLETE;
  }

  NOTREACHED_IN_MIGRATION();
  return UNKNOWN_ERROR;
}

ClientAction PBActionToClientAction(const sync_pb::SyncEnums::Action& action) {
  switch (action) {
    case sync_pb::SyncEnums::UPGRADE_CLIENT:
      return UPGRADE_CLIENT;
    case sync_pb::SyncEnums::UNKNOWN_ACTION:
      return UNKNOWN_ACTION;
  }

  NOTREACHED_IN_MIGRATION();
  return UNKNOWN_ACTION;
}

// Returns true iff |message| is an initial GetUpdates request.
bool IsVeryFirstGetUpdates(const ClientToServerMessage& message) {
  if (!message.has_get_updates()) {
    return false;
  }
  DCHECK_LT(0, message.get_updates().from_progress_marker_size());
  for (int i = 0; i < message.get_updates().from_progress_marker_size(); ++i) {
    if (!message.get_updates().from_progress_marker(i).token().empty()) {
      return false;
    }
  }
  return true;
}

// Returns true iff |message| should contain a store birthday.
bool IsBirthdayRequired(const ClientToServerMessage& message) {
  if (message.has_clear_server_data()) {
    return false;
  }
  if (message.has_commit()) {
    return true;
  }
  if (message.has_get_updates()) {
    return !IsVeryFirstGetUpdates(message);
  }
  NOTIMPLEMENTED();
  return true;
}

SyncProtocolError ErrorCodeToSyncProtocolError(
    const sync_pb::SyncEnums::ErrorType& error_type) {
  SyncProtocolError error;
  error.error_type = PBErrorTypeToSyncProtocolErrorType(error_type);
  if (error_type == sync_pb::SyncEnums::NOT_MY_BIRTHDAY ||
      error_type == sync_pb::SyncEnums::ENCRYPTION_OBSOLETE) {
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
  if (!response.has_new_bag_of_chips()) {
    return;
  }
  std::string bag_of_chips;
  if (response.new_bag_of_chips().SerializeToString(&bag_of_chips)) {
    context->set_bag_of_chips(bag_of_chips);
  }
}

// Handle client commands returned by the server.
void ProcessClientCommand(const sync_pb::ClientCommand& command,
                          SyncCycle* cycle) {
  CHECK(cycle);

  // Update our state for any other commands we've received.
  if (command.has_max_commit_batch_size()) {
    cycle->context()->set_max_commit_batch_size(
        command.max_commit_batch_size());
  }

  if (command.has_set_sync_poll_interval()) {
    base::TimeDelta interval = base::Seconds(command.set_sync_poll_interval());
    if (interval.is_zero()) {
      DLOG(WARNING) << "Received zero poll interval from server. Ignoring.";
    } else {
      cycle->context()->set_poll_interval(interval);
      cycle->delegate()->OnReceivedPollIntervalUpdate(interval);
    }
  }

  if (command.has_gu_retry_delay_seconds() &&
      !base::FeatureList::IsEnabled(syncer::kSyncIgnoreGetUpdatesRetryDelay)) {
    cycle->delegate()->OnReceivedGuRetryDelay(
        base::Seconds(command.gu_retry_delay_seconds()));
  }

  if (command.custom_nudge_delays_size() > 0) {
    std::map<DataType, base::TimeDelta> delay_map;
    for (int i = 0; i < command.custom_nudge_delays_size(); ++i) {
      DataType type = GetDataTypeFromSpecificsFieldNumber(
          command.custom_nudge_delays(i).datatype_id());
      if (type != UNSPECIFIED) {
        delay_map[type] =
            base::Milliseconds(command.custom_nudge_delays(i).delay_ms());
      }
    }
    cycle->delegate()->OnReceivedCustomNudgeDelays(delay_map);
  }

  std::optional<int> max_tokens;
  if (command.has_extension_types_max_tokens()) {
    max_tokens = command.extension_types_max_tokens();
  }
  std::optional<base::TimeDelta> refill_interval;
  if (command.has_extension_types_refill_interval_seconds()) {
    refill_interval =
        base::Seconds(command.extension_types_refill_interval_seconds());
  }
  std::optional<base::TimeDelta> depleted_quota_nudge_delay;
  if (command.has_extension_types_depleted_quota_nudge_delay_seconds()) {
    depleted_quota_nudge_delay = base::Seconds(
        command.extension_types_depleted_quota_nudge_delay_seconds());
  }
  if (max_tokens || refill_interval || depleted_quota_nudge_delay) {
    cycle->delegate()->OnReceivedQuotaParamsForExtensionTypes(
        max_tokens, refill_interval, depleted_quota_nudge_delay);
  }
}

}  // namespace

DataTypeSet GetTypesToMigrate(const ClientToServerResponse& response) {
  return GetDataTypeSetFromSpecificsFieldNumberList(
      response.migrated_data_type_id());
}

SyncProtocolError ConvertErrorPBToSyncProtocolError(
    const sync_pb::ClientToServerResponse_Error& error) {
  return {.error_type = PBErrorTypeToSyncProtocolErrorType(error.error_type()),
          .error_description = error.error_description(),
          .action = PBActionToClientAction(error.action()),
          // THROTTLED and PARTIAL_FAILURE are currently the only error codes
          // using `error_data_types`. In both cases, the types are throttled.
          .error_data_types = error.error_data_type_ids_size() > 0
                                  ? GetDataTypeSetFromSpecificsFieldNumberList(
                                        error.error_data_type_ids())
                                  : DataTypeSet()};
}

// static
SyncerError SyncerProtoUtil::HandleClientToServerMessageResponse(
    const sync_pb::ClientToServerResponse& response,
    SyncCycle* cycle,
    DataTypeSet* partial_failure_data_types) {
  LogClientToServerResponse(response);

  // Remember a bag of chips if it has been sent by the server.
  SaveBagOfChipsFromResponse(response, cycle->context());

  SyncProtocolError sync_protocol_error =
      GetProtocolErrorFromResponse(response, cycle->context());

  // Inform the delegate of the error we got.
  cycle->delegate()->OnSyncProtocolError(sync_protocol_error);

  if (response.has_client_command()) {
    ProcessClientCommand(response.client_command(), cycle);
  }

  // Now do any special handling for the error type and decide on the return
  // value.
  // Partial failures (e.g. specific datatypes throttled or server returned
  // PARTIAL_FAILURE) are reported as success.
  bool should_report_success = false;
  switch (sync_protocol_error.error_type) {
    case UNKNOWN_ERROR:
      LOG(WARNING) << "Sync protocol out-of-date. The server is using a more "
                   << "recent version.";
      break;
    case SYNC_SUCCESS:
      should_report_success = true;
      break;
    case THROTTLED:
      if (sync_protocol_error.error_data_types.empty()) {
        DLOG(WARNING) << "Client fully throttled by syncer.";
        cycle->delegate()->OnThrottled(GetThrottleDelay(response));
      } else {
        // This is a special case, since server only throttle some of datatype,
        // so can treat this case as partial failure.
        DLOG(WARNING) << "Some types throttled by syncer.";
        cycle->delegate()->OnTypesThrottled(
            sync_protocol_error.error_data_types, GetThrottleDelay(response));
        if (partial_failure_data_types != nullptr) {
          *partial_failure_data_types = sync_protocol_error.error_data_types;
        }
        should_report_success = true;
      }
      break;
    case MIGRATION_DONE:
      LOG_IF(ERROR, 0 >= response.migrated_data_type_id_size())
          << "MIGRATION_DONE but no types specified.";
      cycle->delegate()->OnReceivedMigrationRequest(
          GetTypesToMigrate(response));
      break;
    case PARTIAL_FAILURE:
      // This only happens when partial backoff during GetUpdates.
      if (!sync_protocol_error.error_data_types.empty()) {
        DLOG(WARNING)
            << "Some types got partial failure by syncer during GetUpdates.";
        cycle->delegate()->OnTypesBackedOff(
            sync_protocol_error.error_data_types);
      }
      if (partial_failure_data_types != nullptr) {
        *partial_failure_data_types = sync_protocol_error.error_data_types;
      }
      should_report_success = true;
      break;
    case TRANSIENT_ERROR:
    case NOT_MY_BIRTHDAY:
    case DISABLED_BY_ADMIN:
    case CLIENT_DATA_OBSOLETE:
    case ENCRYPTION_OBSOLETE:
      break;
    case CONFLICT:
    case INVALID_MESSAGE:
      // These error types should not be used at this stage.
      NOTREACHED_IN_MIGRATION();
  }

  if (should_report_success) {
    return SyncerError::Success();
  }
  return SyncerError::ProtocolError(sync_protocol_error.error_type);
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
  } else if (response.has_error()) {
    // If the server provides explicit error information, just honor it.
    sync_protocol_error = ConvertErrorPBToSyncProtocolError(response.error());
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
  } else {
    // Legacy server implementation. Compute the error based on |error_code|.
    sync_protocol_error = ErrorCodeToSyncProtocolError(response.error_code());
  }

  // Trivially inferred actions.
  if (sync_protocol_error.action == UNKNOWN_ACTION &&
      sync_protocol_error.error_type == ENCRYPTION_OBSOLETE) {
    sync_protocol_error.action = DISABLE_SYNC_ON_CLIENT;
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
                                            const ClientToServerMessage& msg,
                                            ClientToServerResponse* response) {
  DCHECK(msg.has_protocol_version());
  DCHECK_EQ(msg.protocol_version(),
            ClientToServerMessage::default_instance().protocol_version());
  std::string buffer_in;
  msg.SerializeToString(&buffer_in);

  UMA_HISTOGRAM_ENUMERATION("Sync.PostedClientToServerMessage",
                            msg.message_contents(),
                            ClientToServerMessage::Contents_MAX + 1);

  if (msg.has_get_updates()) {
    UMA_HISTOGRAM_ENUMERATION("Sync.PostedGetUpdatesOrigin",
                              msg.get_updates().get_updates_origin(),
                              sync_pb::SyncEnums::GetUpdatesOrigin_ARRAYSIZE);

    for (const sync_pb::DataTypeProgressMarker& progress_marker :
         msg.get_updates().from_progress_marker()) {
      UMA_HISTOGRAM_ENUMERATION(
          "Sync.PostedDataTypeGetUpdatesRequest",
          DataTypeHistogramValue(GetDataTypeFromSpecificsFieldNumber(
              progress_marker.data_type_id())));
    }
  }

  const base::Time start_time = base::Time::Now();

  // Fills in buffer_out.
  std::string buffer_out;
  HttpResponse http_response =
      scm->PostBufferWithCachedAuth(buffer_in, &buffer_out);
  if (http_response.server_status != HttpResponse::SERVER_CONNECTION_OK) {
    LOG(WARNING) << "Error posting from syncer:" << http_response;
    return false;
  }

  if (!response->ParseFromString(buffer_out)) {
    DLOG(WARNING) << "Error parsing response from sync server";
    return false;
  }

  UMA_HISTOGRAM_MEDIUM_TIMES("Sync.PostedClientToServerMessageLatency",
                             base::Time::Now() - start_time);

  // The error can be specified in 2 different fields, so consider both of them.
  sync_pb::SyncEnums::ErrorType error_type =
      response->has_error() ? response->error().error_type()
                            : response->error_code();
  if (error_type != sync_pb::SyncEnums::SUCCESS) {
    base::UmaHistogramSparse("Sync.PostedClientToServerMessageError2",
                             error_type);
  }

  return true;
}

base::TimeDelta SyncerProtoUtil::GetThrottleDelay(
    const ClientToServerResponse& response) {
  base::TimeDelta throttle_delay = kSyncDelayAfterThrottled;
  if (response.has_client_command()) {
    const sync_pb::ClientCommand& command = response.client_command();
    if (command.has_throttle_delay_seconds()) {
      throttle_delay = base::Seconds(command.throttle_delay_seconds());
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
  if (!birthday.empty()) {
    msg->set_store_birthday(birthday);
  }
  DCHECK(msg->has_store_birthday() || !IsBirthdayRequired(*msg));
  msg->mutable_bag_of_chips()->ParseFromString(
      cycle->context()->bag_of_chips());
  msg->set_api_key(google_apis::GetAPIKey());
  msg->mutable_client_status()->CopyFrom(cycle->context()->client_status());
}

// static
SyncerError SyncerProtoUtil::PostClientToServerMessage(
    const ClientToServerMessage& msg,
    ClientToServerResponse* response,
    SyncCycle* cycle,
    DataTypeSet* partial_failure_data_types) {
  DCHECK(response);
  DCHECK(msg.has_protocol_version());
  DCHECK(msg.has_store_birthday() || !IsBirthdayRequired(msg));
  DCHECK(msg.has_bag_of_chips());
  DCHECK(msg.has_api_key());
  DCHECK(msg.has_client_status());

  LogClientToServerMessage(msg);
  if (!PostAndProcessHeaders(cycle->context()->connection_manager(), msg,
                             response)) {
    // There was an error establishing communication with the server.
    // We can not proceed beyond this point.
    const HttpResponse::ServerConnectionCode server_status =
        cycle->context()->connection_manager()->server_status();

    DCHECK_NE(server_status, HttpResponse::NONE);

    if (server_status == HttpResponse::SERVER_CONNECTION_OK) {
      // The server returned a response but there was a failure in processing
      // it.
      return SyncerError::ProtocolViolationError();
    }

    return ServerConnectionErrorAsSyncerError(
        server_status, cycle->context()->connection_manager()->net_error_code(),
        cycle->context()->connection_manager()->http_status_code());
  }

  return HandleClientToServerMessageResponse(*response, cycle,
                                             partial_failure_data_types);
}

// static
bool SyncerProtoUtil::ShouldMaintainPosition(
    const sync_pb::SyncEntity& sync_entity) {
  // Maintain positions for bookmarks that are not server-defined top-level
  // folders.
  return GetDataTypeFromSpecifics(sync_entity.specifics()) == BOOKMARKS &&
         !(sync_entity.folder() &&
           !sync_entity.server_defined_unique_tag().empty());
}

// static
bool SyncerProtoUtil::ShouldMaintainHierarchy(
    const sync_pb::SyncEntity& sync_entity) {
  // Maintain hierarchy for bookmarks or top-level items.
  return GetDataTypeFromSpecifics(sync_entity.specifics()) == BOOKMARKS ||
         sync_entity.parent_id_string() == "0";
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
  if (response.has_get_updates()) {
    output.append(GetUpdatesResponseString(response.get_updates()));
  }
  return output;
}

}  // namespace syncer
