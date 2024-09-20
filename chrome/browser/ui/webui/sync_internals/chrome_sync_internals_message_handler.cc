// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/sync_internals/chrome_sync_internals_message_handler.h"

#include <string>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/ranges/algorithm.h"

ChromeSyncInternalsMessageHandler::ChromeSyncInternalsMessageHandler(
    signin::IdentityManager* identity_manager,
    syncer::SyncService* sync_service,
    syncer::SyncInvalidationsService* sync_invalidations_service,
    syncer::UserEventService* user_event_service,
    const std::string& channel)
    : message_handler_(this,
                       identity_manager,
                       sync_service,
                       sync_invalidations_service,
                       user_event_service,
                       channel) {}

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
                                         mutable_span);
}

void ChromeSyncInternalsMessageHandler::ResolvePageCallback(
    const base::ValueView callback_id,
    const base::ValueView response) {
  ResolveJavascriptCallback(callback_id, response);
}

void ChromeSyncInternalsMessageHandler::RegisterMessages() {
  for (const auto& [message, handler] :
       message_handler_.GetMessageHandlerMap()) {
    web_ui()->RegisterMessageCallback(
        message,
        base::BindRepeating(
            &ChromeSyncInternalsMessageHandler::AllowJavascriptAndHandleMessage,
            base::Unretained(this), handler));
  }
}

void ChromeSyncInternalsMessageHandler::OnJavascriptDisallowed() {
  message_handler_.DisableMessagesToPage();
}

void ChromeSyncInternalsMessageHandler::AllowJavascriptAndHandleMessage(
    const browser_sync::SyncInternalsMessageHandler::PageMessageHandler&
        handler,
    const base::Value::List& args) {
  AllowJavascript();
  handler.Run(args);
}
