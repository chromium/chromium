// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_APP_MANAGEMENT_APP_MANAGEMENT_PAGE_HANDLER_FACTORY_H_
#define CHROME_BROWSER_UI_WEBUI_APP_MANAGEMENT_APP_MANAGEMENT_PAGE_HANDLER_FACTORY_H_

#include <memory>

#include "chrome/browser/ui/webui/app_management/app_management_page_handler_base.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/resources/cr_components/app_management/app_management.mojom.h"

class Profile;

class AppManagementPageHandlerFactory
    : public app_management::mojom::PageHandlerFactory {
 public:
  AppManagementPageHandlerFactory(
      Profile* profile,
      std::unique_ptr<AppManagementPageHandlerBase::Delegate> delegate);

  AppManagementPageHandlerFactory(const AppManagementPageHandlerFactory&) =
      delete;
  AppManagementPageHandlerFactory& operator=(
      const AppManagementPageHandlerFactory&) = delete;

  ~AppManagementPageHandlerFactory() override;

  void Bind(mojo::PendingReceiver<app_management::mojom::PageHandlerFactory>
                receiver);

 private:
  // app_management::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<app_management::mojom::Page> page,
      mojo::PendingReceiver<app_management::mojom::PageHandler> receiver)
      override;

  raw_ptr<Profile> profile_;

  std::unique_ptr<AppManagementPageHandlerBase::Delegate> delegate_;
  std::unique_ptr<AppManagementPageHandlerBase> page_handler_;

  mojo::Receiver<app_management::mojom::PageHandlerFactory>
      page_factory_receiver_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_APP_MANAGEMENT_APP_MANAGEMENT_PAGE_HANDLER_FACTORY_H_
