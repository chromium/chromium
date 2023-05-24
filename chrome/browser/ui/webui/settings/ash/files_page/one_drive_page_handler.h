// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_ASH_FILES_PAGE_ONE_DRIVE_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_ASH_FILES_PAGE_ONE_DRIVE_PAGE_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/settings/ash/files_page/mojom/one_drive_handler.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class Profile;

namespace ash::settings {

// Page handler for settings related to OneDrive.
class OneDrivePageHandler : public one_drive::mojom::PageHandler {
 public:
  OneDrivePageHandler(
      mojo::PendingReceiver<one_drive::mojom::PageHandler> receiver,
      mojo::PendingRemote<one_drive::mojom::Page> page,
      Profile* profile);

  OneDrivePageHandler(const OneDrivePageHandler&) = delete;
  OneDrivePageHandler& operator=(const OneDrivePageHandler&) = delete;

  ~OneDrivePageHandler() override;

 private:
  // one_drive::mojom::PageHandler:
  void GetUserEmailAddress(GetUserEmailAddressCallback callback) override;

  raw_ptr<Profile> profile_;
  mojo::Remote<one_drive::mojom::Page> page_;
  mojo::Receiver<one_drive::mojom::PageHandler> receiver_{this};
  base::WeakPtrFactory<OneDrivePageHandler> weak_ptr_factory_{this};
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_ASH_FILES_PAGE_ONE_DRIVE_PAGE_HANDLER_H_
