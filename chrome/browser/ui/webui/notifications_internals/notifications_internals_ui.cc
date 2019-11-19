// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/notifications_internals/notifications_internals_ui.h"

#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/notifications_internals/notifications_internals_ui_message_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"

NotificationsInternalsUI::NotificationsInternalsUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  content::WebUIDataSource* html_source = content::WebUIDataSource::Create(
      chrome::kChromeUINotificationsInternalsHost);
  html_source->AddResourcePath("notifications_internals.css",
                               IDR_NOTIFICATIONS_INTERNALS_CSS);
  html_source->AddResourcePath("notifications_internals.js",
                               IDR_NOTIFICATIONS_INTERNALS_JS);
  html_source->AddResourcePath("notifications_internals_browser_proxy.js",
                               IDR_NOTIFICATIONS_INTERNALS_BROWSER_PROXY_JS);
  html_source->SetDefaultResource(IDR_NOTIFICATIONS_INTERNALS_HTML);

  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, html_source);

  web_ui->AddMessageHandler(
      std::make_unique<NotificationsInternalsUIMessageHandler>(profile));
}

NotificationsInternalsUI::~NotificationsInternalsUI() = default;
