// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/sync_internals/sync_internals_message_handler.h"

#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_invalidations_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/sync/user_event_service_factory.h"
#include "chrome/common/channel_info.h"
#include "components/sync/base/command_line_switches.h"
#include "components/sync/base/model_type.h"
#include "components/sync/engine/events/protocol_event.h"
#include "components/sync/invalidations/sync_invalidations_service.h"
#include "components/sync/model/type_entities_count.h"
#include "components/sync/protocol/sync_invalidations_payload.pb.h"
#include "components/sync/protocol/user_event_specifics.pb.h"
#include "components/sync/service/sync_internals_util.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "components/sync_user_events/user_event_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_ui.h"

using syncer::SyncInvalidationsService;
using syncer::SyncService;

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

SyncInternalsMessageHandler::SyncInternalsMessageHandler()
    : SyncInternalsMessageHandler(base::BindRepeating(
          &syncer::sync_ui_util::ConstructAboutInformation,
          syncer::sync_ui_util::IncludeSensitiveData(true))) {
  // This class serves to display debug information to the user, so it's fine to
  // include sensitive data in ConstructAboutInformation() above.
}

SyncInternalsMessageHandler::SyncInternalsMessageHandler(
    AboutSyncDataDelegate about_sync_data_delegate)
    : include_specifics_(GetIncludeSpecificsInitialState()),
      about_sync_data_delegate_(std::move(about_sync_data_delegate)) {}

SyncInternalsMessageHandler::~SyncInternalsMessageHandler() {
  UnregisterModelNotifications();
}

void SyncInternalsMessageHandler::OnJavascriptDisallowed() {
  // Invaliding weak ptrs works well here because the only weak ptr we vend is
  // to the sync side to give us information that should be used to populate the
  // javascript side. If javascript is disallowed, we don't care about updating
  // the UI with data, so dropping those callbacks is fine.
  weak_ptr_factory_.InvalidateWeakPtrs();
  UnregisterModelNotifications();
}

void SyncInternalsMessageHandler::RegisterMessages() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  web_ui()->RegisterMessageCallback(
      syncer::sync_ui_util::kRequestDataAndRegisterForUpdates,
      base::BindRepeating(
          &SyncInternalsMessageHandler::HandleRequestDataAndRegisterForUpdates,
          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      syncer::sync_ui_util::kRequestListOfTypes,
      base::BindRepeating(
          &SyncInternalsMessageHandler::HandleRequestListOfTypes,
          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      syncer::sync_ui_util::kRequestIncludeSpecificsInitialState,
      base::BindRepeating(&SyncInternalsMessageHandler::
                              HandleRequestIncludeSpecificsInitialState,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      syncer::sync_ui_util::kSetIncludeSpecifics,
      base::BindRepeating(
          &SyncInternalsMessageHandler::HandleSetIncludeSpecifics,
          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      syncer::sync_ui_util::kWriteUserEvent,
      base::BindRepeating(&SyncInternalsMessageHandler::HandleWriteUserEvent,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      syncer::sync_ui_util::kRequestStart,
      base::BindRepeating(&SyncInternalsMessageHandler::HandleRequestStart,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      syncer::sync_ui_util::kRequestStopClearData,
      base::BindRepeating(
          &SyncInternalsMessageHandler::HandleRequestStopClearData,
          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      syncer::sync_ui_util::kTriggerRefresh,
      base::BindRepeating(&SyncInternalsMessageHandler::HandleTriggerRefresh,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      syncer::sync_ui_util::kGetAllNodes,
      base::BindRepeating(&SyncInternalsMessageHandler::HandleGetAllNodes,
                          base::Unretained(this)));
}

void SyncInternalsMessageHandler::HandleRequestDataAndRegisterForUpdates(
    const base::Value::List& args) {
  DCHECK(args.empty());
  AllowJavascript();

  // is_registered_ flag protects us from double-registering.  This could
  // happen on a page refresh, where the JavaScript gets re-run but the
  // message handler remains unchanged.
  SyncService* service = GetSyncService();
  if (service && !is_registered_) {
    service->AddObserver(this);
    service->AddProtocolEventObserver(this);

    GetSyncInvalidationsService()->AddListener(this);

    is_registered_ = true;
  }

  SendAboutInfoAndEntityCounts();
}

void SyncInternalsMessageHandler::HandleRequestListOfTypes(
    const base::Value::List& args) {
  DCHECK(args.empty());
  AllowJavascript();

  base::Value::Dict event_details;
  base::Value::List type_list;
  syncer::ModelTypeSet protocol_types = syncer::ProtocolTypes();
  for (syncer::ModelType type : protocol_types) {
    type_list.Append(ModelTypeToDebugString(type));
  }
  event_details.Set(syncer::sync_ui_util::kTypes, std::move(type_list));
  FireWebUIListener(syncer::sync_ui_util::kOnReceivedListOfTypes,
                    event_details);
}

void SyncInternalsMessageHandler::HandleRequestIncludeSpecificsInitialState(
    const base::Value::List& args) {
  DCHECK(args.empty());
  AllowJavascript();

  base::Value::Dict value;
  value.Set(syncer::sync_ui_util::kIncludeSpecifics,
            GetIncludeSpecificsInitialState());

  FireWebUIListener(
      syncer::sync_ui_util::kOnReceivedIncludeSpecificsInitialState, value);
}

void SyncInternalsMessageHandler::HandleGetAllNodes(
    const base::Value::List& args) {
  DCHECK_EQ(1U, args.size());
  AllowJavascript();

  const std::string& callback_id = args[0].GetString();

  SyncService* service = GetSyncService();
  if (service) {
    // This opens up the possibility of non-javascript code calling us
    // asynchronously, and potentially at times we're not allowed to call into
    // the javascript side. We guard against this by invalidating this weak ptr
    // should javascript become disallowed.
    service->GetAllNodesForDebugging(
        base::BindOnce(&SyncInternalsMessageHandler::OnReceivedAllNodes,
                       weak_ptr_factory_.GetWeakPtr(), callback_id));
  }
}

void SyncInternalsMessageHandler::HandleSetIncludeSpecifics(
    const base::Value::List& args) {
  DCHECK_EQ(1U, args.size());
  AllowJavascript();
  include_specifics_ = args[0].GetBool();
}

void SyncInternalsMessageHandler::HandleWriteUserEvent(
    const base::Value::List& args) {
  DCHECK_EQ(2U, args.size());
  AllowJavascript();

  Profile* profile = Profile::FromWebUI(web_ui());
  syncer::UserEventService* user_event_service =
      browser_sync::UserEventServiceFactory::GetForProfile(profile);

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

  user_event_service->RecordUserEvent(event_specifics);
}

void SyncInternalsMessageHandler::HandleRequestStart(
    const base::Value::List& args) {
  DCHECK_EQ(0U, args.size());

  SyncService* service = GetSyncService();
  if (!service) {
    return;
  }

  service->SetSyncFeatureRequested();
  // If the service was previously stopped via StopAndClear(), then the
  // "first-setup-complete" bit was also cleared, and now the service wouldn't
  // fully start up. So set that too.
  service->GetUserSettings()->SetInitialSyncFeatureSetupComplete(
      syncer::SyncFirstSetupCompleteSource::BASIC_FLOW);
}

void SyncInternalsMessageHandler::HandleRequestStopClearData(
    const base::Value::List& args) {
  DCHECK_EQ(0U, args.size());

  SyncService* service = GetSyncService();
  if (!service) {
    return;
  }

  service->StopAndClear();
}

void SyncInternalsMessageHandler::HandleTriggerRefresh(
    const base::Value::List& args) {
  SyncService* service = GetSyncService();
  if (!service) {
    return;
  }

  service->TriggerRefresh(syncer::ModelTypeSet::All());
}

void SyncInternalsMessageHandler::OnReceivedAllNodes(
    const std::string& callback_id,
    base::Value::List nodes) {
  ResolveJavascriptCallback(base::Value(callback_id), nodes);
}

void SyncInternalsMessageHandler::OnStateChanged(SyncService* sync) {
  SendAboutInfoAndEntityCounts();
}

void SyncInternalsMessageHandler::OnProtocolEvent(
    const syncer::ProtocolEvent& event) {
  FireWebUIListener(syncer::sync_ui_util::kOnProtocolEvent,
                    event.ToValue(include_specifics_));
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
    syncer::ModelType type =
        syncer::GetModelTypeFromSpecificsFieldNumber(field_number);
    if (IsRealDataType(type)) {
      data_types_list.Append(syncer::ModelTypeToDebugString(type));
    }
  }

  FireWebUIListener(syncer::sync_ui_util::kOnInvalidationReceived,
                    data_types_list);
}

void SyncInternalsMessageHandler::SendAboutInfoAndEntityCounts() {
  base::Value::Dict value = about_sync_data_delegate_.Run(
      GetSyncService(),
      chrome::GetChannelName(chrome::WithExtendedStable(true)));
  FireWebUIListener(syncer::sync_ui_util::kOnAboutInfoUpdated, value);

  if (SyncService* service = GetSyncService()) {
    service->GetEntityCountsForDebugging(
        BindOnce(&SyncInternalsMessageHandler::OnGotEntityCounts,
                 weak_ptr_factory_.GetWeakPtr()));
  } else {
    OnGotEntityCounts({});
  }
}

void SyncInternalsMessageHandler::OnGotEntityCounts(
    const std::vector<syncer::TypeEntitiesCount>& entity_counts) {
  base::Value::List count_list;
  for (const syncer::TypeEntitiesCount& count : entity_counts) {
    base::Value::Dict count_dictionary;
    count_dictionary.Set(syncer::sync_ui_util::kModelType,
                         ModelTypeToDebugString(count.type));
    count_dictionary.Set(syncer::sync_ui_util::kEntities, count.entities);
    count_dictionary.Set(syncer::sync_ui_util::kNonTombstoneEntities,
                         count.non_tombstone_entities);
    count_list.Append(std::move(count_dictionary));
  }

  base::Value::Dict event_details;
  event_details.Set(syncer::sync_ui_util::kEntityCounts, std::move(count_list));
  FireWebUIListener(syncer::sync_ui_util::kOnEntityCountsUpdated,
                    event_details);
}

SyncService* SyncInternalsMessageHandler::GetSyncService() {
  return SyncServiceFactory::GetForProfile(
      Profile::FromWebUI(web_ui())->GetOriginalProfile());
}

SyncInvalidationsService*
SyncInternalsMessageHandler::GetSyncInvalidationsService() {
  return SyncInvalidationsServiceFactory::GetForProfile(
      Profile::FromWebUI(web_ui())->GetOriginalProfile());
}

void SyncInternalsMessageHandler::UnregisterModelNotifications() {
  SyncService* service = GetSyncService();
  if (!service) {
    return;
  }

  // Cannot use ScopedObserver to do all the tracking because most don't follow
  // AddObserver/RemoveObserver method naming style.
  if (is_registered_) {
    service->RemoveObserver(this);
    service->RemoveProtocolEventObserver(this);
    GetSyncInvalidationsService()->RemoveListener(this);

    is_registered_ = false;
  }
}
