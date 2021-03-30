// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/sync_internals/sync_internals_message_handler.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/sync_invalidations_service_factory.h"
#include "chrome/browser/sync/user_event_service_factory.h"
#include "chrome/common/channel_info.h"
#include "components/sync/base/model_type.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/driver/sync_internals_util.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"
#include "components/sync/engine/events/protocol_event.h"
#include "components/sync/invalidations/sync_invalidations_service.h"
#include "components/sync/js/js_event_details.h"
#include "components/sync/model/type_entities_count.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync/protocol/sync_invalidations_payload.pb.h"
#include "components/sync_user_events/user_event_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_ui.h"

using base::DictionaryValue;
using base::ListValue;
using base::Value;
using syncer::SyncInvalidationsService;
using syncer::SyncService;

namespace {

// Converts the string at |index| in |list| to an int, defaulting to 0 on error.
int64_t StringAtIndexToInt64(const base::ListValue* list, int index) {
  std::string str;
  if (list->GetString(index, &str)) {
    int64_t integer = 0;
    if (base::StringToInt64(str, &integer))
      return integer;
  }
  return 0;
}

// Returns whether the there is any value at the given |index|.
bool HasSomethingAtIndex(const base::ListValue* list, int index) {
  std::string str;
  if (list->GetString(index, &str)) {
    return !str.empty();
  }
  return false;
}

// Returns the initial state of the "include specifics" flag, based on whether
// or not the corresponding command-line switch is set.
bool GetIncludeSpecificsInitialState() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kSyncIncludeSpecificsInProtocolLog);
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
      syncer::sync_ui_util::kRequestStopKeepData,
      base::BindRepeating(
          &SyncInternalsMessageHandler::HandleRequestStopKeepData,
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
    const ListValue* args) {
  DCHECK(args->empty());
  AllowJavascript();

  // is_registered_ flag protects us from double-registering.  This could
  // happen on a page refresh, where the JavaScript gets re-run but the
  // message handler remains unchanged.
  SyncService* service = GetSyncService();
  if (service && !is_registered_) {
    service->AddObserver(this);
    service->AddProtocolEventObserver(this);
    js_controller_ = service->GetJsController();
    js_controller_->AddJsEventHandler(this);

    SyncInvalidationsService* invalidations_service =
        GetSyncInvalidationsService();
    if (invalidations_service) {
      invalidations_service->AddListener(this);
    }

    is_registered_ = true;
  }

  SendAboutInfoAndEntityCounts();
}

void SyncInternalsMessageHandler::HandleRequestListOfTypes(
    const ListValue* args) {
  DCHECK(args->empty());
  AllowJavascript();

  DictionaryValue event_details;
  auto type_list = std::make_unique<ListValue>();
  syncer::ModelTypeSet protocol_types = syncer::ProtocolTypes();
  for (syncer::ModelType type : protocol_types) {
    type_list->AppendString(ModelTypeToString(type));
  }
  event_details.Set(syncer::sync_ui_util::kTypes, std::move(type_list));
  FireWebUIListener(syncer::sync_ui_util::kOnReceivedListOfTypes,
                    event_details);
}

void SyncInternalsMessageHandler::HandleRequestIncludeSpecificsInitialState(
    const ListValue* args) {
  DCHECK(args->empty());
  AllowJavascript();

  DictionaryValue value;
  value.SetBoolean(syncer::sync_ui_util::kIncludeSpecifics,
                   GetIncludeSpecificsInitialState());

  FireWebUIListener(
      syncer::sync_ui_util::kOnReceivedIncludeSpecificsInitialState, value);
}

void SyncInternalsMessageHandler::HandleGetAllNodes(const ListValue* args) {
  DCHECK_EQ(1U, args->GetSize());
  AllowJavascript();

  std::string callback_id;
  bool success = args->GetString(0, &callback_id);
  DCHECK(success);

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
    const ListValue* args) {
  DCHECK_EQ(1U, args->GetSize());
  AllowJavascript();
  include_specifics_ = args->GetList()[0].GetBool();
}

void SyncInternalsMessageHandler::HandleWriteUserEvent(
    const base::ListValue* args) {
  DCHECK_EQ(2U, args->GetSize());
  AllowJavascript();

  Profile* profile = Profile::FromWebUI(web_ui());
  syncer::UserEventService* user_event_service =
      browser_sync::UserEventServiceFactory::GetForProfile(profile);

  sync_pb::UserEventSpecifics event_specifics;
  // Even though there's nothing to set inside the test event object, it needs
  // to be created so that later logic can discern our event type.
  event_specifics.mutable_test_event();

  // |event_time_usec| is required.
  event_specifics.set_event_time_usec(StringAtIndexToInt64(args, 0));

  // |navigation_id| is optional, treat empty string and 0 differently.
  if (HasSomethingAtIndex(args, 1)) {
    event_specifics.set_navigation_id(StringAtIndexToInt64(args, 1));
  }

  user_event_service->RecordUserEvent(event_specifics);
}

void SyncInternalsMessageHandler::HandleRequestStart(
    const base::ListValue* args) {
  DCHECK_EQ(0U, args->GetSize());

  SyncService* service = GetSyncService();
  if (!service)
    return;

  service->GetUserSettings()->SetSyncRequested(true);
  // If the service was previously stopped via StopAndClear(), then the
  // "first-setup-complete" bit was also cleared, and now the service wouldn't
  // fully start up. So set that too.
  service->GetUserSettings()->SetFirstSetupComplete(
      syncer::SyncFirstSetupCompleteSource::BASIC_FLOW);
}

void SyncInternalsMessageHandler::HandleRequestStopKeepData(
    const base::ListValue* args) {
  DCHECK_EQ(0U, args->GetSize());

  SyncService* service = GetSyncService();
  if (!service)
    return;

  service->GetUserSettings()->SetSyncRequested(false);
}

void SyncInternalsMessageHandler::HandleRequestStopClearData(
    const base::ListValue* args) {
  DCHECK_EQ(0U, args->GetSize());

  SyncService* service = GetSyncService();
  if (!service)
    return;

  service->StopAndClear();
}

void SyncInternalsMessageHandler::HandleTriggerRefresh(
    const base::ListValue* args) {
  SyncService* service = GetSyncService();
  if (!service)
    return;

  // Only allowed to trigger refresh/schedule nudges for protocol types, things
  // like PROXY_TABS are not allowed.
  service->TriggerRefresh(syncer::Intersection(service->GetActiveDataTypes(),
                                               syncer::ProtocolTypes()));
}

void SyncInternalsMessageHandler::OnReceivedAllNodes(
    const std::string& callback_id,
    std::unique_ptr<ListValue> nodes) {
  ResolveJavascriptCallback(base::Value(callback_id), *nodes);
}

void SyncInternalsMessageHandler::OnStateChanged(SyncService* sync) {
  SendAboutInfoAndEntityCounts();
}

void SyncInternalsMessageHandler::OnProtocolEvent(
    const syncer::ProtocolEvent& event) {
  std::unique_ptr<DictionaryValue> value(event.ToValue(include_specifics_));
  FireWebUIListener(syncer::sync_ui_util::kOnProtocolEvent, *value);
}

void SyncInternalsMessageHandler::OnInvalidationReceived(
    const std::string& payload) {
  sync_pb::SyncInvalidationsPayload payload_message;
  if (!payload_message.ParseFromString(payload)) {
    return;
  }

  base::ListValue data_types_list;
  for (const auto& data_type_invalidation :
       payload_message.data_type_invalidations()) {
    const int field_number = data_type_invalidation.data_type_id();
    syncer::ModelType type =
        syncer::GetModelTypeFromSpecificsFieldNumber(field_number);
    if (IsRealDataType(type)) {
      data_types_list.Append(syncer::ModelTypeToString(type));
    }
  }

  FireWebUIListener(syncer::sync_ui_util::kOnInvalidationReceived,
                    data_types_list);
}

void SyncInternalsMessageHandler::HandleJsEvent(
    const std::string& name,
    const syncer::JsEventDetails& details) {
  DVLOG(1) << "Handling event: " << name << " with details "
           << details.ToString();
  FireWebUIListener(name, details.Get());
}

void SyncInternalsMessageHandler::SendAboutInfoAndEntityCounts() {
  std::unique_ptr<DictionaryValue> value = about_sync_data_delegate_.Run(
      GetSyncService(),
      chrome::GetChannelName(chrome::WithExtendedStable(true)));
  FireWebUIListener(syncer::sync_ui_util::kOnAboutInfoUpdated, *value);

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
  ListValue count_list;
  for (const syncer::TypeEntitiesCount& count : entity_counts) {
    DictionaryValue count_dictionary;
    count_dictionary.SetStringPath(syncer::sync_ui_util::kModelType,
                                   ModelTypeToString(count.type));
    count_dictionary.SetIntPath(syncer::sync_ui_util::kEntities,
                                count.entities);
    count_dictionary.SetIntPath(syncer::sync_ui_util::kNonTombstoneEntities,
                                count.non_tombstone_entities);
    count_list.Append(std::move(count_dictionary));
  }

  DictionaryValue event_details;
  event_details.SetPath(syncer::sync_ui_util::kEntityCounts,
                        std::move(count_list));
  FireWebUIListener(syncer::sync_ui_util::kOnEntityCountsUpdated,
                    std::move(event_details));
}

SyncService* SyncInternalsMessageHandler::GetSyncService() {
  return ProfileSyncServiceFactory::GetForProfile(
      Profile::FromWebUI(web_ui())->GetOriginalProfile());
}

SyncInvalidationsService*
SyncInternalsMessageHandler::GetSyncInvalidationsService() {
  return SyncInvalidationsServiceFactory::GetForProfile(
      Profile::FromWebUI(web_ui())->GetOriginalProfile());
}

void SyncInternalsMessageHandler::UnregisterModelNotifications() {
  SyncService* service = GetSyncService();
  if (!service)
    return;

  // Cannot use ScopedObserver to do all the tracking because most don't follow
  // AddObserver/RemoveObserver method naming style.
  if (is_registered_) {
    DCHECK(js_controller_);
    service->RemoveObserver(this);
    service->RemoveProtocolEventObserver(this);
    js_controller_->RemoveJsEventHandler(this);
    js_controller_ = nullptr;

    SyncInvalidationsService* invalidations_service =
        GetSyncInvalidationsService();
    if (invalidations_service) {
      invalidations_service->RemoveListener(this);
    }

    is_registered_ = false;
  }
}
