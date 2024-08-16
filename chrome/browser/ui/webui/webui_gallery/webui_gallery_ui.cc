// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/webui_gallery/webui_gallery_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/side_panel_shared_resources.h"
#include "chrome/grit/side_panel_shared_resources_map.h"
#include "chrome/grit/webui_gallery_resources.h"
#include "chrome/grit/webui_gallery_resources_map.h"
#include "components/favicon_base/favicon_url_parser.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/webui/color_change_listener/color_change_handler.h"

namespace {

void CreateAndAddWebuiGalleryUIHtmlSource(Profile* profile) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUIWebuiGalleryHost);

  webui::SetupWebUIDataSource(
      source,
      base::make_span(kWebuiGalleryResources, kWebuiGalleryResourcesSize),
      IDR_WEBUI_GALLERY_WEBUI_GALLERY_HTML);

  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::FrameSrc, "frame-src 'self';");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::FrameAncestors,
      "frame-ancestors 'self';");

  // TODO(colehorvitz): Promote to a place where it can be easily registered
  // by many WebUIs.
  source->AddString("opensInNewTab", "Opens in new tab");
  source->AddLocalizedString("backButton", IDS_ACCNAME_BACK);

  // Add shared SidePanel resources so that those elements can be demonstrated
  // as well.
  source->AddResourcePaths(base::make_span(kSidePanelSharedResources,
                                           kSidePanelSharedResourcesSize));

  content::URLDataSource::Add(
      profile, std::make_unique<FaviconSource>(
                   profile, chrome::FaviconUrlFormat::kFavicon2));
}

}  // namespace

WebuiGalleryUI::WebuiGalleryUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, false) {
  CreateAndAddWebuiGalleryUIHtmlSource(Profile::FromWebUI(web_ui));
}

WebuiGalleryUI::~WebuiGalleryUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(WebuiGalleryUI)

void WebuiGalleryUI::BindInterface(
    mojo::PendingReceiver<color_change_listener::mojom::PageHandler>
        pending_receiver) {
  color_provider_handler_ = std::make_unique<ui::ColorChangeHandler>(
      web_ui()->GetWebContents(), std::move(pending_receiver));
}
