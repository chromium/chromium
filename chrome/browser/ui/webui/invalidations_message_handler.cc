// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/invalidations_message_handler.h"

#include <utility>

#include "base/bind.h"
#include "chrome/browser/invalidation/profile_invalidation_provider_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/invalidation/impl/invalidation_logger.h"
#include "components/invalidation/impl/invalidation_switches.h"
#include "components/invalidation/impl/profile_invalidation_provider.h"
#include "components/invalidation/public/invalidation_handler.h"
#include "components/invalidation/public/invalidation_service.h"
#include "content/public/browser/web_ui.h"

namespace invalidation {
class InvalidationLogger;
}  // namespace invalidation

namespace syncer {
class ObjectIdInvalidationMap;
}  // namespace syncer

namespace {

invalidation::ProfileInvalidationProvider* GetInvalidationProvider(
    Profile* profile) {
  return invalidation::ProfileInvalidationProviderFactory::GetForProfile(
      profile);
}

}  // namespace

InvalidationsMessageHandler::InvalidationsMessageHandler() : logger_(nullptr) {}

InvalidationsMessageHandler::~InvalidationsMessageHandler() {
  if (logger_)
    logger_->UnregisterObserver(this);
}

void InvalidationsMessageHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "doneLoading", base::BindRepeating(&InvalidationsMessageHandler::UIReady,
                                         base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "requestDetailedStatus",
      base::BindRepeating(
          &InvalidationsMessageHandler::HandleRequestDetailedStatus,
          base::Unretained(this)));
}

void InvalidationsMessageHandler::UIReady(const base::ListValue* args) {
  invalidation::ProfileInvalidationProvider* invalidation_provider =
      GetInvalidationProvider(Profile::FromWebUI(web_ui()));
  if (invalidation_provider) {
    logger_ = invalidation_provider->GetInvalidationService()->
        GetInvalidationLogger();
  }
  if (logger_ && !logger_->IsObserverRegistered(this))
    logger_->RegisterObserver(this);
  UpdateContent(args);
}

void InvalidationsMessageHandler::HandleRequestDetailedStatus(
    const base::ListValue* args) {
  invalidation::ProfileInvalidationProvider* invalidation_provider =
      GetInvalidationProvider(Profile::FromWebUI(web_ui()));
  if (invalidation_provider) {
    invalidation_provider->GetInvalidationService()->RequestDetailedStatus(
        base::Bind(&InvalidationsMessageHandler::OnDetailedStatus,
                   weak_ptr_factory_.GetWeakPtr()));
  }
}

void InvalidationsMessageHandler::UpdateContent(const base::ListValue* args) {
  if (logger_)
    logger_->EmitContent();
}

void InvalidationsMessageHandler::OnRegistrationChange(
    const std::multiset<std::string>& registered_handlers) {
  base::ListValue list_of_handlers;
  for (auto it = registered_handlers.begin(); it != registered_handlers.end();
       ++it) {
    list_of_handlers.AppendString(*it);
  }
  web_ui()->CallJavascriptFunctionUnsafe("chrome.invalidations.updateHandlers",
                                         list_of_handlers);
}

void InvalidationsMessageHandler::OnStateChange(
    const syncer::InvalidatorState& new_state,
    const base::Time& last_changed_timestamp) {
  std::string state(syncer::InvalidatorStateToString(new_state));
  web_ui()->CallJavascriptFunctionUnsafe(
      "chrome.invalidations.updateInvalidatorState", base::Value(state),
      base::Value(last_changed_timestamp.ToJsTime()));
}

void InvalidationsMessageHandler::OnUpdateIds(
    const std::string& handler_name,
    const syncer::ObjectIdCountMap& ids) {
  base::ListValue list_of_objects;
  for (auto it = ids.begin(); it != ids.end(); ++it) {
    std::unique_ptr<base::DictionaryValue> dic(new base::DictionaryValue());
    dic->SetString("name", (it->first).name());
    dic->SetInteger("source", (it->first).source());
    dic->SetInteger("totalCount", it->second);
    list_of_objects.Append(std::move(dic));
  }
  web_ui()->CallJavascriptFunctionUnsafe("chrome.invalidations.updateIds",
                                         base::Value(handler_name),
                                         list_of_objects);
}
void InvalidationsMessageHandler::OnDebugMessage(
    const base::DictionaryValue& details) {}

void InvalidationsMessageHandler::OnInvalidation(
    const syncer::ObjectIdInvalidationMap& new_invalidations) {
  std::unique_ptr<base::ListValue> invalidations_list =
      new_invalidations.ToValue();
  web_ui()->CallJavascriptFunctionUnsafe(
      "chrome.invalidations.logInvalidations", *invalidations_list);
}

void InvalidationsMessageHandler::OnDetailedStatus(
    const base::DictionaryValue& network_details) {
  web_ui()->CallJavascriptFunctionUnsafe(
      "chrome.invalidations.updateDetailedStatus", network_details);
}
