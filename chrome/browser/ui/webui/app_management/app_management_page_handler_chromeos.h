// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_APP_MANAGEMENT_APP_MANAGEMENT_PAGE_HANDLER_CHROMEOS_H_
#define CHROME_BROWSER_UI_WEBUI_APP_MANAGEMENT_APP_MANAGEMENT_PAGE_HANDLER_CHROMEOS_H_

#include "chrome/browser/ui/webui/app_management/app_management_page_handler_base.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "ui/webui/resources/cr_components/app_management/app_management.mojom.h"

class Profile;

// PageHandler for the ChromeOS App Management page.
class AppManagementPageHandlerChromeOs : public AppManagementPageHandlerBase {
 public:
  AppManagementPageHandlerChromeOs(
      mojo::PendingReceiver<app_management::mojom::PageHandler> receiver,
      mojo::PendingRemote<app_management::mojom::Page> page,
      Profile* profile,
      AppManagementPageHandlerBase::Delegate& delegate);

  AppManagementPageHandlerChromeOs(const AppManagementPageHandlerChromeOs&) =
      delete;
  AppManagementPageHandlerChromeOs& operator=(
      const AppManagementPageHandlerChromeOs&) = delete;

  ~AppManagementPageHandlerChromeOs() override;
};

#endif  // CHROME_BROWSER_UI_WEBUI_APP_MANAGEMENT_APP_MANAGEMENT_PAGE_HANDLER_CHROMEOS_H_
