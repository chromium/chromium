// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SYNC_INTERNALS_CHROME_SYNC_INTERNALS_MESSAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SYNC_INTERNALS_CHROME_SYNC_INTERNALS_MESSAGE_HANDLER_H_

#include <string>

#include "components/browser_sync/sync_internals_message_handler.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace signin {
class IdentityManager;
}  // namespace signin

namespace syncer {
class SyncInvalidationsService;
class SyncService;
class UserEventService;
}  // namespace syncer

// Chrome-specific implementation of SyncInternalsMessageHandler.
class ChromeSyncInternalsMessageHandler
    : public browser_sync::SyncInternalsMessageHandler::Delegate,
      public content::WebUIMessageHandler {
 public:
  ChromeSyncInternalsMessageHandler(
      signin::IdentityManager* identity_manager,
      syncer::SyncService* sync_service,
      syncer::SyncInvalidationsService* sync_invalidations_service,
      syncer::UserEventService* user_event_service,
      const std::string& channel);

  ChromeSyncInternalsMessageHandler(const ChromeSyncInternalsMessageHandler&) =
      delete;
  ChromeSyncInternalsMessageHandler& operator=(
      const ChromeSyncInternalsMessageHandler&) = delete;

  ~ChromeSyncInternalsMessageHandler() override = default;

  // browser_sync::SyncInternalsMessageHandler overrides.
  void SendEventToPage(std::string_view event_name,
                       base::span<const base::ValueView> args) override;
  void ResolvePageCallback(const base::ValueView callback_id,
                           const base::ValueView response) override;

  // content::WebUIMessageHandler overrides.
  void RegisterMessages() override;
  void OnJavascriptDisallowed() override;

 private:
  // When handling a message page from the page, this class might want to reply
  // back, which requires javascript to be enabled. This wrapper ensures it.
  void AllowJavascriptAndHandleMessage(
      const browser_sync::SyncInternalsMessageHandler::PageMessageHandler&
          handler,
      const base::Value::List& args);

  browser_sync::SyncInternalsMessageHandler message_handler_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_SYNC_INTERNALS_CHROME_SYNC_INTERNALS_MESSAGE_HANDLER_H_
