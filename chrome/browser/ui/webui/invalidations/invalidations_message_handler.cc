// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/invalidations/invalidations_message_handler.h"

#include <utility>

#include "base/bind.h"
#include "chrome/browser/invalidation/profile_invalidation_provider_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/invalidation/impl/invalidation_logger.h"
#include "components/invalidation/impl/invalidation_switches.h"
#include "components/invalidation/impl/profile_invalidation_provider.h"
#include "components/invalidation/public/invalidation_handler.h"
#include "components/invalidation/public/invalidation_service.h"
#include "components/invalidation/public/topic_invalidation_map.h"
#include "content/public/browser/web_ui.h"

namespace invalidation {
class InvalidationLogger;
}  // namespace invalidation

namespace {

invalidation::ProfileInvalidationProvider* GetInvalidationProvider(
    Profile* profile) {
  return invalidation::ProfileInvalidationProviderFactory::GetForProfile(
      profile);
}

}  // namespace

InvalidationsMessageHandler::InvalidationsMessageHandler() : logger_(nullptr) {}

InvalidationsMessageHandler::~InvalidationsMessageHandler() {
  // This handler can be destroyed without OnJavascriptDisallowed() ever being
  // called (https://crbug.com/1199198). Call it to ensure that `this` is
  // removed as an observer.
  OnJavascriptDisallowed();
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

void InvalidationsMessageHandler::OnJavascriptDisallowed() {
  if (logger_)
    logger_->UnregisterObserver(this);
}

void InvalidationsMessageHandler::UIReady(const base::Value::List& args) {
  AllowJavascript();
  invalidation::ProfileInvalidationProvider* invalidation_provider =
      GetInvalidationProvider(Profile::FromWebUI(web_ui()));
  if (invalidation_provider) {
    logger_ = invalidation_provider->GetInvalidationService()
                  ->GetInvalidationLogger();
  }
  if (logger_ && !logger_->IsObserverRegistered(this))
    logger_->RegisterObserver(this);
  UpdateContent(args);
}

void InvalidationsMessageHandler::HandleRequestDetailedStatus(
    const base::Value::List& args) {
  invalidation::ProfileInvalidationProvider* invalidation_provider =
      GetInvalidationProvider(Profile::FromWebUI(web_ui()));
  if (invalidation_provider) {
    invalidation_provider->GetInvalidationService()->RequestDetailedStatus(
        base::BindRepeating(&InvalidationsMessageHandler::OnDetailedStatus,
                            weak_ptr_factory_.GetWeakPtr()));
  }
}

void InvalidationsMessageHandler::UpdateContent(const base::Value::List& args) {
  if (logger_)
    logger_->EmitContent();
}

void InvalidationsMessageHandler::OnRegistrationChange(
    const std::set<std::string>& registered_handlers) {
  base::Value::List list_of_handlers;
  for (const auto& registered_handler : registered_handlers) {
    list_of_handlers.Append(registered_handler);
  }
  FireWebUIListener("handlers-updated", list_of_handlers);
}

void InvalidationsMessageHandler::OnStateChange(
    const invalidation::InvalidatorState& new_state,
    const base::Time& last_changed_timestamp) {
  std::string state(invalidation::InvalidatorStateToString(new_state));
  FireWebUIListener("state-updated", base::Value(state),
                    base::Value(last_changed_timestamp.ToJsTime()));
}

void InvalidationsMessageHandler::OnUpdatedTopics(
    const std::string& handler_name,
    const invalidation::TopicCountMap& topics) {
  base::Value::List list_of_objects;
  for (const auto& topic_item : topics) {
    base::Value::Dict dict;
    dict.Set("name", topic_item.first);
    // TODO(crbug.com/1056181): source has been deprecated and after Topic->
    // ObjectID refactoring completely makes no sense. It needs to be cleaned
    // up together with other ObjectID references in js counterpart. Pass 0
    // temporary to avoid changes in js counterpart.
    dict.Set("source", 0);
    dict.Set("totalCount", topic_item.second);
    list_of_objects.Append(std::move(dict));
  }
  FireWebUIListener("ids-updated", base::Value(handler_name), list_of_objects);
}
void InvalidationsMessageHandler::OnDebugMessage(
    const base::Value::Dict& details) {}

void InvalidationsMessageHandler::OnInvalidation(
    const invalidation::TopicInvalidationMap& new_invalidations) {
  FireWebUIListener("log-invalidations", new_invalidations.ToValue());
}

void InvalidationsMessageHandler::OnDetailedStatus(
    base::Value::Dict network_details) {
  FireWebUIListener("detailed-status-updated", network_details);
}
