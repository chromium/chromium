// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/side_panel/companion/companion_side_panel_untrusted_ui.h"

#include "chrome/browser/companion/core/utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/side_panel/companion/companion_side_panel_controller_utils.h"
#include "chrome/browser/ui/views/side_panel/companion/companion_utils.h"
#include "chrome/browser/ui/webui/side_panel/companion/companion_page_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/side_panel_companion_resources.h"
#include "chrome/grit/side_panel_companion_resources_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "url/gurl.h"

CompanionSidePanelUntrustedUI::CompanionSidePanelUntrustedUI(
    content::WebUI* web_ui)
    : UntrustedTopChromeWebUIController(web_ui) {
  // Set up the chrome-untrusted://companion-side-panel source.
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::CreateAndAdd(
          web_ui->GetWebContents()->GetBrowserContext(),
          chrome::kChromeUIUntrustedCompanionSidePanelURL);

  // Add required resources.
  html_source->UseStringsJs();
  html_source->AddResourcePaths(base::make_span(
      kSidePanelCompanionResources, kSidePanelCompanionResourcesSize));
  html_source->AddResourcePath("", IDR_SIDE_PANEL_COMPANION_COMPANION_HTML);
  // Allow untrusted mojo resources to be loaded.
  html_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome-untrusted://resources 'self';");
  // Allow the companion homepage URL to be embedded in this WebUI.
  GURL frameSrcUrl =
      GURL(companion::GetHomepageURLForCompanion()).GetWithEmptyPath();
  std::string frameSrcString = frameSrcUrl.is_valid()
                                   ? frameSrcUrl.spec()
                                   : companion::GetHomepageURLForCompanion();
  // Allow iframing accounts page due to potential redirects.
  std::string frameSrcDirective =
      std::string(
          "frame-src https://accounts.google.com https://consent.google.com ") +
      frameSrcString + ";";
  std::string formActionDirective =
      std::string("form-action ") + frameSrcString + ";";
  html_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::FrameSrc, frameSrcDirective);
  html_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::FormAction, formActionDirective);
  html_source->AddString("companion_origin", frameSrcString);

  // Add localized companion strings.
  html_source->AddLocalizedString(
      "network_error_page_top_line",
      IDS_SIDE_PANEL_COMPANION_ERROR_PAGE_FIRST_LINE);
  html_source->AddLocalizedString(
      "network_error_page_bottom_line",
      IDS_SIDE_PANEL_COMPANION_ERROR_PAGE_SECOND_LINE);

  Observe(web_ui->GetWebContents());
}

CompanionSidePanelUntrustedUI::~CompanionSidePanelUntrustedUI() = default;

void CompanionSidePanelUntrustedUI::BindInterface(
    mojo::PendingReceiver<side_panel::mojom::CompanionPageHandlerFactory>
        receiver) {
  companion_page_factory_receiver_.reset();
  companion_page_factory_receiver_.Bind(std::move(receiver));
}

void CompanionSidePanelUntrustedUI::CreateCompanionPageHandler(
    mojo::PendingReceiver<side_panel::mojom::CompanionPageHandler> receiver,
    mojo::PendingRemote<side_panel::mojom::CompanionPage> page) {
  companion_page_handler_ = std::make_unique<companion::CompanionPageHandler>(
      std::move(receiver), std::move(page), this);
}

void CompanionSidePanelUntrustedUI::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // We only care about error pages returning from two frames, the main WebUI
  // frame and the companion iframe. Ignore navigations from any other subframe
  // that could be nested within the companion.
  auto* parent_frame = navigation_handle->GetParentFrame();
  if (!navigation_handle->IsInPrimaryMainFrame() && parent_frame &&
      !parent_frame->IsInPrimaryMainFrame()) {
    return;
  }

  if (navigation_handle->IsErrorPage() && companion_page_handler_) {
    companion_page_handler_->OnNavigationError();
  }
}

base::WeakPtr<CompanionSidePanelUntrustedUI>
CompanionSidePanelUntrustedUI::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

CompanionSidePanelUntrustedUIConfig::CompanionSidePanelUntrustedUIConfig()
    : DefaultTopChromeWebUIConfig(
          content::kChromeUIUntrustedScheme,
          chrome::kChromeUIUntrustedCompanionSidePanelHost) {}

bool CompanionSidePanelUntrustedUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return companion::IsCompanionFeatureEnabled();
}

WEB_UI_CONTROLLER_TYPE_IMPL(CompanionSidePanelUntrustedUI)
