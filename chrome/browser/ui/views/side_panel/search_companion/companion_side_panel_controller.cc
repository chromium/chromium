// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/search_companion/companion_side_panel_controller.h"

#include "chrome/browser/companion/core/features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/search_companion/search_companion_side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_web_ui_view.h"
#include "chrome/browser/ui/webui/side_panel/companion/companion_side_panel_untrusted_ui.h"
#include "chrome/common/webui_url_constants.h"

namespace companion {

CompanionSidePanelController::CompanionSidePanelController(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {}

CompanionSidePanelController::~CompanionSidePanelController() = default;

void CompanionSidePanelController::CreateAndRegisterEntry() {
  auto* registry = SidePanelRegistry::Get(web_contents_);
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents_);
  if (!registry || !browser ||
      registry->GetEntryForKey(
          SidePanelEntry::Key(SidePanelEntry::Id::kSearchCompanion))) {
    return;
  }

  auto* coordinator =
      SearchCompanionSidePanelCoordinator::GetOrCreateForBrowser(browser);

  auto entry = std::make_unique<SidePanelEntry>(
      SidePanelEntry::Id::kSearchCompanion, coordinator->name(),
      ui::ImageModel::FromVectorIcon(coordinator->icon(), ui::kColorIcon,
                                     /*icon_size=*/16),
      base::BindRepeating(
          &companion::CompanionSidePanelController::CreateCompanionWebView,
          base::Unretained(this)),
      base::BindRepeating(
          &companion::CompanionSidePanelController::GetOpenInNewTabUrl,
          base::Unretained(this)));
  registry->Register(std::move(entry));
}

void CompanionSidePanelController::DeregisterEntry() {
  auto* registry = SidePanelRegistry::Get(web_contents_);
  if (!registry) {
    return;
  }

  registry->Deregister(
      SidePanelEntry::Key(SidePanelEntry::Id::kSearchCompanion));
}

void CompanionSidePanelController::ShowCompanionSidePanel() {
  if (Browser* browser = chrome::FindBrowserWithWebContents(web_contents_)) {
    auto* coordinator =
        SearchCompanionSidePanelCoordinator::GetOrCreateForBrowser(browser);
    coordinator->Show();
  }
}

void CompanionSidePanelController::UpdateNewTabButton(GURL url_to_open) {
  open_in_new_tab_url_ = url_to_open;
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents_);
  BrowserView* browser_view =
      browser ? BrowserView::GetBrowserViewForBrowser(browser) : nullptr;
  if (browser_view) {
    browser_view->side_panel_coordinator()->UpdateNewTabButtonState();
  }
}

content::WebContents*
CompanionSidePanelController::GetCompanionWebContentsForTesting() {
  return web_contents();
}

std::unique_ptr<views::View>
CompanionSidePanelController::CreateCompanionWebView() {
  auto wrapper =
      std::make_unique<BubbleContentsWrapperT<CompanionSidePanelUntrustedUI>>(
          GURL(chrome::kChromeUIUntrustedCompanionSidePanelURL),
          Profile::FromBrowserContext(web_contents_->GetBrowserContext()),
          /*webui_resizes_host=*/false,
          /*esc_closes_ui=*/false);
  auto companion_web_view =
      std::make_unique<SidePanelWebUIViewT<CompanionSidePanelUntrustedUI>>(
          base::RepeatingClosure(), base::RepeatingClosure(),
          std::move(wrapper));

  // Observe on the webcontents for opening links in new tab.
  Observe(companion_web_view->GetWebContents());

  return companion_web_view;
}

GURL CompanionSidePanelController::GetOpenInNewTabUrl() {
  return open_in_new_tab_url_;
}

// This method is called when the WebContents wants to open a link in a new
// tab. This delegate does not override AddNewContents(), so the webcontents
// is not actually created. Instead it forwards the parameters to the real
// browser.
void CompanionSidePanelController::DidOpenRequestedURL(
    content::WebContents* new_contents,
    content::RenderFrameHost* source_render_frame_host,
    const GURL& url,
    const content::Referrer& referrer,
    WindowOpenDisposition disposition,
    ui::PageTransition transition,
    bool started_from_context_menu,
    bool renderer_initiated) {
  content::OpenURLParams params(url, referrer, disposition, transition,
                                renderer_initiated);

  // If the navigation is initiated by the renderer process, we must set an
  // initiator origin.
  if (renderer_initiated) {
    params.initiator_origin = url::Origin::Create(url);
  }

  bool open_in_current_tab = companion::features::kOpenLinksInCurrentTab.Get();
  params.disposition = open_in_current_tab
                           ? WindowOpenDisposition::CURRENT_TAB
                           : WindowOpenDisposition::NEW_FOREGROUND_TAB;

  Browser* browser = chrome::FindBrowserWithWebContents(web_contents_);
  if (!browser) {
    return;
  }

  // Open the url in the desired tab.
  content::WebContents* new_tab_web_contents = browser->OpenURL(params);

  // If a new tab was opened, open companion side panel in it.
  if (new_tab_web_contents && !open_in_current_tab) {
    BrowserView::GetBrowserViewForBrowser(browser)
        ->side_panel_coordinator()
        ->Show(SidePanelEntry::Id::kSearchCompanion);
  }
}

}  // namespace companion
