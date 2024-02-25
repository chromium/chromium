// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_FILES_ONE_DRIVE_PAGE_HANDLER_FACTORY_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_FILES_ONE_DRIVE_PAGE_HANDLER_FACTORY_H_

#include <memory>

#include "chrome/browser/ui/webui/ash/settings/pages/files/mojom/one_drive_handler.mojom.h"
#include "chrome/browser/ui/webui/ash/settings/pages/files/one_drive_page_handler.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

class Profile;

namespace ash::settings {

class OneDrivePageHandlerFactory : public one_drive::mojom::PageHandlerFactory {
 public:
  OneDrivePageHandlerFactory(
      Profile* profile,
      mojo::PendingReceiver<one_drive::mojom::PageHandlerFactory> receiver);

  OneDrivePageHandlerFactory(const OneDrivePageHandlerFactory&) = delete;
  OneDrivePageHandlerFactory& operator=(const OneDrivePageHandlerFactory&) =
      delete;

  ~OneDrivePageHandlerFactory() override;

 private:
  // one_drive::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<one_drive::mojom::Page> page,
      mojo::PendingReceiver<one_drive::mojom::PageHandler> receiver) override;

  raw_ptr<Profile> profile_;

  std::unique_ptr<OneDrivePageHandler> page_handler_;
  mojo::Receiver<one_drive::mojom::PageHandlerFactory> page_factory_receiver_{
      this};
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_FILES_ONE_DRIVE_PAGE_HANDLER_FACTORY_H_
