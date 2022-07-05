// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_MANAGE_MIRRORSYNC_MANAGE_MIRRORSYNC_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_MANAGE_MIRRORSYNC_MANAGE_MIRRORSYNC_UI_H_

#include "chrome/browser/ui/webui/chromeos/manage_mirrorsync/manage_mirrorsync.mojom.h"
#include "chrome/browser/ui/webui/chromeos/manage_mirrorsync/manage_mirrorsync_page_handler.h"
#include "ui/web_dialogs/web_dialog_ui.h"

namespace chromeos {

// The WebUI for chrome://manage-mirrorsync.
class ManageMirrorSyncUI
    : public ui::MojoWebDialogUI,
      public chromeos::manage_mirrorsync::mojom::PageHandlerFactory {
 public:
  explicit ManageMirrorSyncUI(content::WebUI* web_ui);

  ManageMirrorSyncUI(const ManageMirrorSyncUI&) = delete;
  ManageMirrorSyncUI& operator=(const ManageMirrorSyncUI&) = delete;

  ~ManageMirrorSyncUI() override;

  // Instantiates implementor of the mojom::PageHandlerFactory
  // mojo interface passing the pending receiver that will be internally bound.
  void BindInterface(mojo::PendingReceiver<
                     chromeos::manage_mirrorsync::mojom::PageHandlerFactory>
                         pending_receiver);

  // chromeos::manage_mirrorsync::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingReceiver<chromeos::manage_mirrorsync::mojom::PageHandler>
          pending_page_handler) override;

 private:
  std::unique_ptr<ManageMirrorSyncPageHandler> page_handler_;
  mojo::Receiver<chromeos::manage_mirrorsync::mojom::PageHandlerFactory>
      factory_receiver_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_MANAGE_MIRRORSYNC_MANAGE_MIRRORSYNC_UI_H_
