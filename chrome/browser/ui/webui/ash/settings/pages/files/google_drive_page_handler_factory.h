// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_FILES_GOOGLE_DRIVE_PAGE_HANDLER_FACTORY_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_FILES_GOOGLE_DRIVE_PAGE_HANDLER_FACTORY_H_

#include <memory>

#include "chrome/browser/ui/webui/ash/settings/pages/files/google_drive_page_handler.h"
#include "chrome/browser/ui/webui/ash/settings/pages/files/mojom/google_drive_handler.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

class Profile;

namespace ash::settings {

class GoogleDrivePageHandlerFactory
    : public google_drive::mojom::PageHandlerFactory {
 public:
  GoogleDrivePageHandlerFactory(
      Profile* profile,
      mojo::PendingReceiver<google_drive::mojom::PageHandlerFactory> receiver);

  GoogleDrivePageHandlerFactory(const GoogleDrivePageHandlerFactory&) = delete;
  GoogleDrivePageHandlerFactory& operator=(
      const GoogleDrivePageHandlerFactory&) = delete;

  ~GoogleDrivePageHandlerFactory() override;

 private:
  // google_drive::mojom::PageHandlerFactory:
  void CreatePageHandler(mojo::PendingRemote<google_drive::mojom::Page> page,
                         mojo::PendingReceiver<google_drive::mojom::PageHandler>
                             receiver) override;

  raw_ptr<Profile> profile_;

  std::unique_ptr<GoogleDrivePageHandler> page_handler_;
  mojo::Receiver<google_drive::mojom::PageHandlerFactory>
      page_factory_receiver_{this};
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_FILES_GOOGLE_DRIVE_PAGE_HANDLER_FACTORY_H_
