// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NOTIFICATIONS_INTERNALS_NOTIFICATIONS_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_NOTIFICATIONS_INTERNALS_NOTIFICATIONS_INTERNALS_UI_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/notifications_internals/notifications_internals.mojom-forward.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/internal_webui_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

class Profile;
class NotificationsInternalsUIPageHandler;
class NotificationsInternalsUI;

class NotificationsInternalsUIConfig
    : public content::DefaultInternalWebUIConfig<NotificationsInternalsUI> {
 public:
  NotificationsInternalsUIConfig()
      : DefaultInternalWebUIConfig(
            chrome::kChromeUINotificationsInternalsHost) {}

  // content::WebUIConfig:
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

// The WebUI for chrome://notifications-internals.
class NotificationsInternalsUI : public ui::MojoWebUIController {
 public:
  explicit NotificationsInternalsUI(content::WebUI* web_ui);

  NotificationsInternalsUI(const NotificationsInternalsUI&) = delete;
  NotificationsInternalsUI& operator=(const NotificationsInternalsUI&) = delete;

  ~NotificationsInternalsUI() override;

  // Instantiates the implementor of the
  // notifications_internals::mojom::PageHandler mojo interface passing the
  // pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<notifications_internals::mojom::PageHandler>
          receiver);

 private:
  raw_ptr<Profile> profile_;
  std::unique_ptr<NotificationsInternalsUIPageHandler> page_handler_;
  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_NOTIFICATIONS_INTERNALS_NOTIFICATIONS_INTERNALS_UI_H_
