// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/ash/manage_mirrorsync/manage_mirrorsync_ui.h"

#include "ash/constants/ash_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/manage_mirrorsync_resources.h"
#include "chrome/grit/manage_mirrorsync_resources_map.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace ash {

bool ManageMirrorSyncUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return ash::features::IsDriveFsMirroringEnabled();
}

ManageMirrorSyncUI::ManageMirrorSyncUI(content::WebUI* web_ui)
    : ui::MojoWebDialogUI{web_ui} {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      Profile::FromWebUI(web_ui), chrome::kChromeUIManageMirrorSyncHost);
  webui::SetupWebUIDataSource(source,
                              base::make_span(kManageMirrorsyncResources,
                                              kManageMirrorsyncResourcesSize),
                              IDR_MANAGE_MIRRORSYNC_INDEX_HTML);
}

ManageMirrorSyncUI::~ManageMirrorSyncUI() = default;

void ManageMirrorSyncUI::BindInterface(
    mojo::PendingReceiver<manage_mirrorsync::mojom::PageHandlerFactory>
        pending_receiver) {
  if (factory_receiver_.is_bound()) {
    factory_receiver_.reset();
  }
  factory_receiver_.Bind(std::move(pending_receiver));
}

void ManageMirrorSyncUI::CreatePageHandler(
    mojo::PendingReceiver<manage_mirrorsync::mojom::PageHandler> receiver) {
  page_handler_ = std::make_unique<ManageMirrorSyncPageHandler>(
      std::move(receiver), Profile::FromWebUI(web_ui()));
}

WEB_UI_CONTROLLER_TYPE_IMPL(ManageMirrorSyncUI)

}  // namespace ash
