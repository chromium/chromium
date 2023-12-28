// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NEARBY_INTERNALS_NEARBY_INTERNALS_PUSH_NOTIFICATION_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_NEARBY_INTERNALS_NEARBY_INTERNALS_PUSH_NOTIFICATION_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace content {
class BrowserContext;
}  // namespace content

class NearbyInternalsPushNotificationHandler
    : public content::WebUIMessageHandler {
 public:
  explicit NearbyInternalsPushNotificationHandler(
      content::BrowserContext* context);
  NearbyInternalsPushNotificationHandler(
      const NearbyInternalsPushNotificationHandler&) = delete;
  NearbyInternalsPushNotificationHandler& operator=(
      const NearbyInternalsPushNotificationHandler&) = delete;
  ~NearbyInternalsPushNotificationHandler() override;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

 private:
  void Initialize(const base::Value::List& args);
  void HandleAddPushNotificationClient(const base::Value::List& args);

  const raw_ptr<content::BrowserContext> context_;
  base::WeakPtrFactory<NearbyInternalsPushNotificationHandler>
      weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_NEARBY_INTERNALS_NEARBY_INTERNALS_PUSH_NOTIFICATION_HANDLER_H_
