// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_MANAGE_MIRRORSYNC_MANAGE_MIRRORSYNC_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_MANAGE_MIRRORSYNC_MANAGE_MIRRORSYNC_PAGE_HANDLER_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/chromeos/manage_mirrorsync/manage_mirrorsync.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {

// Handles communication from the chrome://manage-mirrorsync renderer process to
// the browser process exposing various methods for the JS to invoke.
class ManageMirrorSyncPageHandler
    : public chromeos::manage_mirrorsync::mojom::PageHandler {
 public:
  explicit ManageMirrorSyncPageHandler(
      mojo::PendingReceiver<chromeos::manage_mirrorsync::mojom::PageHandler>
          pending_page_handler);

  ManageMirrorSyncPageHandler(const ManageMirrorSyncPageHandler&) = delete;
  ManageMirrorSyncPageHandler& operator=(const ManageMirrorSyncPageHandler&) =
      delete;

  ~ManageMirrorSyncPageHandler() override;

  // chromeos::manage_mirrorsync::mojom::PageHandler:
  void GetSyncingPaths(GetSyncingPathsCallback callback) override;

 private:
  mojo::Receiver<chromeos::manage_mirrorsync::mojom::PageHandler> receiver_;

  base::WeakPtrFactory<ManageMirrorSyncPageHandler> weak_ptr_factory_{this};
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_MANAGE_MIRRORSYNC_MANAGE_MIRRORSYNC_PAGE_HANDLER_H_
