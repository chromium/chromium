// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/extended_updates/extended_updates_ui.h"

#include "base/containers/span.h"
#include "chrome/browser/ash/extended_updates/extended_updates_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/extended_updates/extended_updates.mojom.h"
#include "chrome/browser/ui/webui/ash/extended_updates/extended_updates_page_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/extended_updates_resources.h"
#include "chrome/grit/extended_updates_resources_map.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace ash::extended_updates {

ExtendedUpdatesUI::ExtendedUpdatesUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      Profile::FromWebUI(web_ui), chrome::kChromeUIExtendedUpdatesDialogHost);

  webui::SetupWebUIDataSource(
      source,
      base::make_span(kExtendedUpdatesResources, kExtendedUpdatesResourcesSize),
      IDR_EXTENDED_UPDATES_EXTENDED_UPDATES_HTML);
}

ExtendedUpdatesUI::~ExtendedUpdatesUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(ExtendedUpdatesUI)

void ExtendedUpdatesUI::BindInterface(
    mojo::PendingReceiver<ash::extended_updates::mojom::PageHandlerFactory>
        receiver) {
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

void ExtendedUpdatesUI::CreatePageHandler(
    mojo::PendingRemote<ash::extended_updates::mojom::Page> page,
    mojo::PendingReceiver<ash::extended_updates::mojom::PageHandler> receiver) {
  DCHECK(page);
  page_handler_ = std::make_unique<ExtendedUpdatesPageHandler>(
      std::move(page), std::move(receiver));
}

ExtendedUpdatesUIConfig::ExtendedUpdatesUIConfig()
    : DefaultWebUIConfig(content::kChromeUIScheme,
                         chrome::kChromeUIExtendedUpdatesDialogHost) {}

ExtendedUpdatesUIConfig::~ExtendedUpdatesUIConfig() = default;

bool ExtendedUpdatesUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return ash::ExtendedUpdatesController::Get()->IsOptInEligible(
      browser_context);
}

}  // namespace ash::extended_updates
