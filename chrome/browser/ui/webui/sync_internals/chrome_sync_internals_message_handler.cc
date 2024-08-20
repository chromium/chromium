// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/sync_internals/chrome_sync_internals_message_handler.h"

#include <string>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_invalidations_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/sync/user_event_service_factory.h"
#include "chrome/common/channel_info.h"

ChromeSyncInternalsMessageHandler::ChromeSyncInternalsMessageHandler(
    AboutSyncDataDelegate about_sync_data_delegate)
    : SyncInternalsMessageHandler(std::move(about_sync_data_delegate)) {}

syncer::SyncService* ChromeSyncInternalsMessageHandler::GetSyncService() {
  return SyncServiceFactory::GetForProfile(
      Profile::FromWebUI(web_ui())->GetOriginalProfile());
}

syncer::SyncInvalidationsService*
ChromeSyncInternalsMessageHandler::GetSyncInvalidationsService() {
  return SyncInvalidationsServiceFactory::GetForProfile(
      Profile::FromWebUI(web_ui())->GetOriginalProfile());
}

syncer::UserEventService*
ChromeSyncInternalsMessageHandler::GetUserEventService() {
  return browser_sync::UserEventServiceFactory::GetForProfile(
      Profile::FromWebUI(web_ui())->GetOriginalProfile());
}

std::string ChromeSyncInternalsMessageHandler::GetChannel() {
  return chrome::GetChannelName(chrome::WithExtendedStable(true));
}

void ChromeSyncInternalsMessageHandler::SendEventToPage(
    std::string_view event_name,
    base::span<const base::ValueView> args) {
  std::vector<base::ValueView> event_name_and_args;
  event_name_and_args.push_back(event_name);
  base::ranges::copy(args, std::back_inserter(event_name_and_args));
  base::span<base::ValueView> mutable_span(event_name_and_args);
  // `mutable_span` will be implicitly converted to a const one. Declaring
  // std::vector<const base::ValueView> above is not an option, because
  // vector elements must be mutable.
  web_ui()->CallJavascriptFunctionUnsafe("cr.webUIListenerCallback",
                                         std::move(mutable_span));
}

void ChromeSyncInternalsMessageHandler::ResolvePageCallback(
    const base::ValueView callback_id,
    const base::ValueView response) {
  ResolveJavascriptCallback(callback_id, response);
}

void ChromeSyncInternalsMessageHandler::RegisterMessages() {
  for (const auto& [message, handler] : GetMessageHandlerMap()) {
    web_ui()->RegisterMessageCallback(
        message,
        base::BindRepeating(
            &ChromeSyncInternalsMessageHandler::AllowJavascriptAndHandleMessage,
            base::Unretained(this), handler));
  }
}

void ChromeSyncInternalsMessageHandler::OnJavascriptDisallowed() {
  DisableMessagesToPage();
}

void ChromeSyncInternalsMessageHandler::AllowJavascriptAndHandleMessage(
    const PageMessageHandler& handler,
    const base::Value::List& args) {
  AllowJavascript();
  handler.Run(args);
}
