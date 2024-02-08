// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_APP_MANAGEMENT_WEB_APP_SETTINGS_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_APP_MANAGEMENT_WEB_APP_SETTINGS_PAGE_HANDLER_H_

#include "chrome/browser/ui/webui/app_management/app_management_page_handler_base.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "ui/webui/resources/cr_components/app_management/app_management.mojom.h"

class Profile;

// PageHandler for the chrome://app-settings page. Connects directly to the
// WebAppProvider to manage settings for web apps.
class WebAppSettingsPageHandler : public AppManagementPageHandlerBase {
 public:
  WebAppSettingsPageHandler(
      mojo::PendingReceiver<app_management::mojom::PageHandler> receiver,
      mojo::PendingRemote<app_management::mojom::Page> page,
      Profile* profile,
      AppManagementPageHandlerBase::Delegate& delegate);

  WebAppSettingsPageHandler(const WebAppSettingsPageHandler&) = delete;
  WebAppSettingsPageHandler& operator=(const WebAppSettingsPageHandler&) =
      delete;

  ~WebAppSettingsPageHandler() override;
};

#endif  // CHROME_BROWSER_UI_WEBUI_APP_MANAGEMENT_WEB_APP_SETTINGS_PAGE_HANDLER_H_
