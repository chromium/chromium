// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_MANAGE_MIRRORSYNC_MANAGE_MIRRORSYNC_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_MANAGE_MIRRORSYNC_MANAGE_MIRRORSYNC_UI_H_

#include "chrome/browser/ui/webui/ash/manage_mirrorsync/manage_mirrorsync.mojom.h"
#include "chrome/browser/ui/webui/ash/manage_mirrorsync/manage_mirrorsync_page_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "ui/web_dialogs/web_dialog_ui.h"

namespace ash {

class ManageMirrorSyncUI;

// WebUIConfig for chrome://manage-mirrorsync
class ManageMirrorSyncUIConfig
    : public content::DefaultWebUIConfig<ManageMirrorSyncUI> {
 public:
  ManageMirrorSyncUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUIManageMirrorSyncHost) {}

  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

// The WebUI for chrome://manage-mirrorsync.
class ManageMirrorSyncUI : public ui::MojoWebDialogUI,
                           public manage_mirrorsync::mojom::PageHandlerFactory {
 public:
  explicit ManageMirrorSyncUI(content::WebUI* web_ui);

  ManageMirrorSyncUI(const ManageMirrorSyncUI&) = delete;
  ManageMirrorSyncUI& operator=(const ManageMirrorSyncUI&) = delete;

  ~ManageMirrorSyncUI() override;

  // Instantiates implementor of the mojom::PageHandlerFactory
  // mojo interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<manage_mirrorsync::mojom::PageHandlerFactory>
          pending_receiver);

  // manage_mirrorsync::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingReceiver<manage_mirrorsync::mojom::PageHandler>
          pending_page_handler) override;

 private:
  std::unique_ptr<ManageMirrorSyncPageHandler> page_handler_;
  mojo::Receiver<manage_mirrorsync::mojom::PageHandlerFactory>
      factory_receiver_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_MANAGE_MIRRORSYNC_MANAGE_MIRRORSYNC_UI_H_
