// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_sync/sync_internals_message_handler.h"

#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/sync/base/command_line_switches.h"
#include "components/sync/base/data_type.h"
#include "components/sync/engine/events/protocol_event.h"
#include "components/sync/invalidations/sync_invalidations_service.h"
#include "components/sync/model/type_entities_count.h"
#include "components/sync/protocol/sync_invalidations_payload.pb.h"
#include "components/sync/protocol/user_event_specifics.pb.h"
#include "components/sync/service/sync_internals_util.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "components/sync_user_events/user_event_service.h"

namespace browser_sync {

namespace {

// Converts the string at |index| in |list| to an int, defaulting to 0 on error.
int64_t StringAtIndexToInt64(const base::Value::List& list, size_t index) {
  if (list.size() > index && list[index].is_string()) {
    int64_t integer = 0;
    if (base::StringToInt64(list[index].GetString(), &integer)) {
      return integer;
    }
  }
  return 0;
}

// Returns whether the there is any value at the given |index|.
bool HasSomethingAtIndex(const base::Value::List& list, size_t index) {
  if (list.size() > index && list[index].is_string()) {
    return !list[index].GetString().empty();
  }
  return false;
}

// Returns the initial state of the "include specifics" flag, based on whether
// or not the corresponding command-line switch is set.
bool GetIncludeSpecificsInitialState() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      syncer::kSyncIncludeSpecificsInProtocolLog);
}

}  //  namespace

SyncInternalsMessageHandler::SyncInternalsMessageHandler(
    Delegate* delegate,
    signin::IdentityManager* identity_manager,
    syncer::SyncService* sync_service,
    syncer::SyncInvalidationsService* sync_invalidations_service,
    syncer::UserEventService* user_event_service,
    const std::string& channel)
    : SyncInternalsMessageHandler(
          delegate,
          base::BindRepeating(&syncer::sync_ui_util::ConstructAboutInformation,
                              syncer::sync_ui_util::IncludeSensitiveData(true)),
          identity_manager,
          sync_service,
          sync_invalidations_service,
          user_event_service,
          channel) {
  // This class serves to display debug information to the user, so it's fine to
  // include sensitive data in ConstructAboutInformation() above.
}

SyncInternalsMessageHandler::SyncInternalsMessageHandler(
    Delegate* delegate,
    GetAboutSyncDataCb get_about_sync_data_cb,
    signin::IdentityManager* identity_manager,
    syncer::SyncService* sync_service,
    syncer::SyncInvalidationsService* sync_invalidations_service,
    syncer::UserEventService* user_event_service,
    const std::string& channel)
    : include_specifics_(GetIncludeSpecificsInitialState()),
      delegate_(delegate),
      get_about_sync_data_cb_(std::move(get_about_sync_data_cb)),
      identity_manager_(identity_manager),
      sync_service_(sync_service),
      sync_invalidations_service_(sync_invalidations_service),
      user_event_service_(user_event_service),
      channel_(channel) {}

SyncInternalsMessageHandler::~SyncInternalsMessageHandler() = default;

void SyncInternalsMessageHandler::DisableMessagesToPage() {
  // Invaliding weak ptrs works well here because the only weak ptr we vend is
  // to the sync side to give us information that should be used to populate the
  // page side. If messages are disallowed, we don't care about updating
  // the UI with data, so dropping those callbacks is fine.
  weak_ptr_factory_.InvalidateWeakPtrs();
  sync_service_observation_.Reset();
  protocol_event_observation_.Reset();
  invalidations_observation_.Reset();
}

base::flat_map<std::string, SyncInternalsMessageHandler::PageMessageHandler>
SyncInternalsMessageHandler::GetMessageHandlerMap() {
  return {
      {syncer::sync_ui_util::kRequestDataAndRegisterForUpdates,
       base::BindRepeating(
           &SyncInternalsMessageHandler::HandleRequestDataAndRegisterForUpdates,
           base::Unretained(this))},
      {syncer::sync_ui_util::kRequestListOfTypes,
       base::BindRepeating(
           &SyncInternalsMessageHandler::HandleRequestListOfTypes,
           base::Unretained(this))},
      {syncer::sync_ui_util::kRequestIncludeSpecificsInitialState,
       base::BindRepeating(&SyncInternalsMessageHandler::
                               HandleRequestIncludeSpecificsInitialState,
                           base::Unretained(this))},
      {syncer::sync_ui_util::kSetIncludeSpecifics,
       base::BindRepeating(
           &SyncInternalsMessageHandler::HandleSetIncludeSpecifics,
           base::Unretained(this))},
      {syncer::sync_ui_util::kWriteUserEvent,
       base::BindRepeating(&SyncInternalsMessageHandler::HandleWriteUserEvent,
                           base::Unretained(this))},
      {syncer::sync_ui_util::kRequestStart,
       base::BindRepeating(&SyncInternalsMessageHandler::HandleRequestStart,
                           base::Unretained(this))},
      {syncer::sync_ui_util::kTriggerRefresh,
       base::BindRepeating(&SyncInternalsMessageHandler::HandleTriggerRefresh,
                           base::Unretained(this))},
      {syncer::sync_ui_util::kGetAllNodes,
       base::BindRepeating(&SyncInternalsMessageHandler::HandleGetAllNodes,
                           base::Unretained(this))},
  };
}

void SyncInternalsMessageHandler::HandleRequestDataAndRegisterForUpdates(
    const base::Value::List& args) {
  CHECK(args.empty());

  // Checking IsObserving() protects us from double-registering.  This could
  // happen on a page refresh, where the JavaScript gets re-run but the
  // message handler remains unchanged.
  if (sync_service_ && !sync_service_observation_.IsObserving()) {
    sync_service_observation_.Observe(sync_service_);
  }
  if (sync_service_ && !protocol_event_observation_.IsObserving()) {
    protocol_event_observation_.Observe(sync_service_);
  }
  if (sync_invalidations_service_ &&
      !invalidations_observation_.IsObserving()) {
    invalidations_observation_.Observe(sync_invalidations_service_);
  }

  SendAboutInfoAndEntityCounts();
}

void SyncInternalsMessageHandler::HandleRequestListOfTypes(
    const base::Value::List& args) {
  CHECK(args.empty());

  base::Value::Dict event_details;
  base::Value::List type_list;
  syncer::DataTypeSet protocol_types = syncer::ProtocolTypes();
  for (syncer::DataType type : protocol_types) {
    type_list.Append(DataTypeToDebugString(type));
  }
  event_details.Set(syncer::sync_ui_util::kTypes, std::move(type_list));
  base::ValueView event_args[] = {event_details};
  delegate_->SendEventToPage(syncer::sync_ui_util::kOnReceivedListOfTypes,
                             event_args);
}

void SyncInternalsMessageHandler::HandleRequestIncludeSpecificsInitialState(
    const base::Value::List& args) {
  CHECK(args.empty());

  base::Value::Dict value;
  value.Set(syncer::sync_ui_util::kIncludeSpecifics,
            GetIncludeSpecificsInitialState());

  base::ValueView event_args[] = {value};
  delegate_->SendEventToPage(
      syncer::sync_ui_util::kOnReceivedIncludeSpecificsInitialState,
      event_args);
}

void SyncInternalsMessageHandler::HandleGetAllNodes(
    const base::Value::List& args) {
  CHECK_EQ(1U, args.size());

  const std::string& callback_id = args[0].GetString();

  if (sync_service_) {
    // This opens up the possibility of non-javascript code calling us
    // asynchronously, and potentially at times we're not allowed to call into
    // the javascript side. We guard against this by invalidating this weak ptr
    // should javascript become disallowed.
    sync_service_->GetAllNodesForDebugging(
        base::BindOnce(&SyncInternalsMessageHandler::OnReceivedAllNodes,
                       weak_ptr_factory_.GetWeakPtr(), callback_id));
  }
}

void SyncInternalsMessageHandler::HandleSetIncludeSpecifics(
    const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  include_specifics_ = args[0].GetBool();
}

void SyncInternalsMessageHandler::HandleWriteUserEvent(
    const base::Value::List& args) {
  CHECK_EQ(2U, args.size());

  sync_pb::UserEventSpecifics event_specifics;
  // Even though there's nothing to set inside the test event object, it needs
  // to be created so that later logic can discern our event type.
  event_specifics.mutable_test_event();

  // |event_time_usec| is required.
  event_specifics.set_event_time_usec(StringAtIndexToInt64(args, 0u));

  // |navigation_id| is optional, treat empty string and 0 differently.
  if (HasSomethingAtIndex(args, 1)) {
    event_specifics.set_navigation_id(StringAtIndexToInt64(args, 1u));
  }

  user_event_service_->RecordUserEvent(event_specifics);
}

void SyncInternalsMessageHandler::HandleRequestStart(
    const base::Value::List& args) {
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  CHECK_EQ(0U, args.size());

  if (!identity_manager_ || !sync_service_) {
    return;
  }

  const bool can_use_api =
      identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin) &&
      !identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSync);
  if (!can_use_api) {
    return;
  }

  identity_manager_->GetPrimaryAccountMutator()->SetPrimaryAccount(
      identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin),
      signin::ConsentLevel::kSync,
      signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);
  sync_service_->SetSyncFeatureRequested();
  sync_service_->GetUserSettings()->SetInitialSyncFeatureSetupComplete(
      syncer::SyncFirstSetupCompleteSource::BASIC_FLOW);
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
}

void SyncInternalsMessageHandler::HandleTriggerRefresh(
    const base::Value::List& args) {
  if (!sync_service_) {
    return;
  }

  sync_service_->TriggerRefresh(syncer::DataTypeSet::All());
}

void SyncInternalsMessageHandler::OnReceivedAllNodes(
    const std::string& callback_id,
    base::Value::List nodes) {
  delegate_->ResolvePageCallback(callback_id, nodes);
}

void SyncInternalsMessageHandler::OnStateChanged(syncer::SyncService* sync) {
  SendAboutInfoAndEntityCounts();
}

void SyncInternalsMessageHandler::OnProtocolEvent(
    const syncer::ProtocolEvent& event) {
  base::Value::Dict dict = event.ToValue(include_specifics_);
  base::ValueView event_args[] = {dict};
  delegate_->SendEventToPage(syncer::sync_ui_util::kOnProtocolEvent,
                             event_args);
}

void SyncInternalsMessageHandler::OnInvalidationReceived(
    const std::string& payload) {
  sync_pb::SyncInvalidationsPayload payload_message;
  if (!payload_message.ParseFromString(payload)) {
    return;
  }

  base::Value::List data_types_list;
  for (const auto& data_type_invalidation :
       payload_message.data_type_invalidations()) {
    const int field_number = data_type_invalidation.data_type_id();
    syncer::DataType type =
        syncer::GetDataTypeFromSpecificsFieldNumber(field_number);
    if (IsRealDataType(type)) {
      data_types_list.Append(syncer::DataTypeToDebugString(type));
    }
  }

  base::ValueView event_args[] = {data_types_list};
  delegate_->SendEventToPage(syncer::sync_ui_util::kOnInvalidationReceived,
                             event_args);
}

void SyncInternalsMessageHandler::SendAboutInfoAndEntityCounts() {
  base::Value::Dict value =
      get_about_sync_data_cb_.Run(sync_service_, channel_);
  base::ValueView event_args[] = {value};
  delegate_->SendEventToPage(syncer::sync_ui_util::kOnAboutInfoUpdated,
                             event_args);

  if (sync_service_) {
    sync_service_->GetEntityCountsForDebugging(
        BindRepeating(&SyncInternalsMessageHandler::OnGotEntityCounts,
                      weak_ptr_factory_.GetWeakPtr()));
  }
}

void SyncInternalsMessageHandler::OnGotEntityCounts(
    const syncer::TypeEntitiesCount& entity_counts) {
  base::Value::Dict count_dictionary;
  count_dictionary.Set(syncer::sync_ui_util::kDataType,
                       DataTypeToDebugString(entity_counts.type));
  count_dictionary.Set(syncer::sync_ui_util::kEntities, entity_counts.entities);
  count_dictionary.Set(syncer::sync_ui_util::kNonTombstoneEntities,
                       entity_counts.non_tombstone_entities);

  base::Value::Dict event_details;
  event_details.Set(syncer::sync_ui_util::kEntityCounts,
                    std::move(count_dictionary));
  base::ValueView event_args[] = {event_details};
  delegate_->SendEventToPage(syncer::sync_ui_util::kOnEntityCountsUpdated,
                             event_args);
}

}  // namespace browser_sync
