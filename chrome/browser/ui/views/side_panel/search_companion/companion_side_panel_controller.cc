// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/search_companion/companion_side_panel_controller.h"

#include "chrome/browser/companion/core/features.h"
#include "chrome/browser/companion/core/utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/side_panel/companion/companion_tab_helper.h"
#include "chrome/browser/ui/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/companion_side_panel_web_view.h"
#include "chrome/browser/ui/views/side_panel/search_companion/search_companion_side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/webui/side_panel/companion/companion_page_handler.h"
#include "chrome/browser/ui/webui/side_panel/companion/companion_side_panel_untrusted_ui.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "components/google/core/common/google_util.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "third_party/blink/public/mojom/frame/user_activation_notification_type.mojom.h"

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

void CompanionSidePanelController::ShowCompanionSidePanel(
    SidePanelOpenTrigger side_panel_open_trigger) {
  if (Browser* browser = chrome::FindBrowserWithWebContents(web_contents_)) {
    auto* coordinator =
        SearchCompanionSidePanelCoordinator::GetOrCreateForBrowser(browser);
    coordinator->Show(side_panel_open_trigger);
  }
}

void CompanionSidePanelController::UpdateNewTabButton(GURL url_to_open) {
  open_in_new_tab_url_ = url_to_open;
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents_);
  if (!browser) {
    return;
  }
  SidePanelUtil::GetSidePanelCoordinatorForBrowser(browser)
      ->UpdateNewTabButtonState();
}

void CompanionSidePanelController::OnCompanionSidePanelClosed() {
  open_in_new_tab_url_ = GURL();
}

content::WebContents*
CompanionSidePanelController::GetCompanionWebContentsForTesting() {
  return web_contents();
}

std::unique_ptr<views::View>
CompanionSidePanelController::CreateCompanionWebView() {
  auto companion_web_view = std::make_unique<CompanionSidePanelWebView>(
      Profile::FromBrowserContext(web_contents_->GetBrowserContext()));

  // Observe on the webcontents for opening links in new tab.
  Observe(companion_web_view->GetWebContents());

  return companion_web_view;
}

GURL CompanionSidePanelController::GetOpenInNewTabUrl() {
  return open_in_new_tab_url_;
}

bool CompanionSidePanelController::IsSiteTrusted(const GURL& url) {
  if (google_util::IsGoogleDomainUrl(
          url, google_util::ALLOW_SUBDOMAIN,
          google_util::DISALLOW_NON_STANDARD_PORTS)) {
    return true;
  }

  // This is a workaround for local development where the URL may be a
  // non-Google domain like *.proxy.googlers.com. If the Finch flag for
  // Companion homepage is not set to a Google domain, make sure the request is
  // coming from the CSC homepage.
  if (net::registry_controlled_domains::SameDomainOrHost(
          url, GURL(companion::GetHomepageURLForCompanion()),
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES)) {
    return true;
  }

  return false;
}

// This method is called when the WebContents wants to open a link in a new
// tab. This delegate does not override AddNewContents(), so the WebContents
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
  // Ensure that the navigation is coming from a page we trust before
  // redirecting to main browser.
  if (!IsSiteTrusted(source_render_frame_host->GetLastCommittedURL())) {
    return;
  }

  // The window.open from the Search Companion is caught here and ignored.
  // Instead we create another navigation toward the same URL targeting a frame
  // outside of the side panel.
  //
  // This navigation is created from this component, so we consider it to be
  // browser initiated. In particular, we do not plumb all the parameters from
  // the original navigation. For instance we do not populate the
  // `initiator_frame_token`. This means some security properties like sandbox
  // flags are lost along the way.
  //
  // This is not problematic because we trust the original navigation was
  // initiated from the expected origin.
  //
  // Specifically, we need the navigation to be considered browser-initiated, as
  // renderer-initiated navigation history entries may be skipped if the
  // document does not receive any user interaction (like in our case). See
  // https://issuetracker.google.com/285038653
  content::OpenURLParams params(url, referrer, disposition, transition,
                                /*is_renderer_initiated=*/false);

  bool open_in_current_tab = companion::ShouldOpenLinksInCurrentTab();
  params.disposition = open_in_current_tab
                           ? WindowOpenDisposition::CURRENT_TAB
                           : WindowOpenDisposition::NEW_FOREGROUND_TAB;

  Browser* browser = chrome::FindBrowserWithWebContents(web_contents_);
  if (!browser) {
    return;
  }

  // Open the url in the desired tab.
  content::WebContents* tab_web_contents = browser->OpenURL(params);

  if (open_in_current_tab) {
    // Add metrics to record the open trigger for the companion page as a link
    // click from side panel. Note, the user can click on links even before the
    // metrics is consumed, e.g. a double click. Either way, just overwrite the
    // metrics if it already exists.
    auto* tab_helper =
        companion::CompanionTabHelper::FromWebContents(tab_web_contents);
    tab_helper->SetMostRecentSidePanelOpenTrigger(
        SidePanelOpenTrigger::kOpenedInNewTabFromSidePanel);
  }

  // If a new tab was opened, open companion side panel in it.
  if (tab_web_contents && !open_in_current_tab) {
    SidePanelUI::GetSidePanelUIForBrowser(browser)->Show(
        SidePanelEntry::Id::kSearchCompanion,
        SidePanelOpenTrigger::kOpenedInNewTabFromSidePanel);
  }

  // Notify server that a link was opened in browser.
  auto* companion_helper =
      companion::CompanionTabHelper::FromWebContents(tab_web_contents);
  if (companion_helper) {
    auto* pref_service = browser->profile()->GetPrefs();
    bool is_entry_point_default_pinned =
        pref_service ? pref_service
                           ->GetDefaultPrefValue(
                               prefs::kSidePanelCompanionEntryPinnedToToolbar)
                           ->GetBool()
                     : false;
    auto metadata = side_panel::mojom::LinkOpenMetadata::New(
        (open_in_current_tab
             ? side_panel::mojom::LinkOpenMetadata::LinkOpenAction::kClobbered
             : side_panel::mojom::LinkOpenMetadata::LinkOpenAction::kNewTab),
        is_entry_point_default_pinned);
    companion_helper->AddCompanionFinishedLoadingCallback(base::BindOnce(
        &CompanionSidePanelController::NotifyLinkClick, base::Unretained(this),
        url, std::move(metadata), tab_web_contents));
  }
}

void CompanionSidePanelController::NotifyLinkClick(
    GURL opened_url,
    side_panel::mojom::LinkOpenMetadataPtr metadata,
    content::WebContents* main_tab_contents) {
  auto* companion_helper =
      companion::CompanionTabHelper::FromWebContents(main_tab_contents);
  if (companion_helper && companion_helper->GetCompanionPageHandler()) {
    companion_helper->GetCompanionPageHandler()->NotifyLinkOpened(
        opened_url, std::move(metadata));
  }
}

void CompanionSidePanelController::AddCompanionFinishedLoadingCallback(
    CompanionTabHelper::CompanionLoadedCallback callback) {
  if (has_companion_loaded) {
    std::move(callback).Run();
    return;
  }
  companion_loaded_callbacks_.push_back(std::move(callback));
}

void CompanionSidePanelController::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  // Ensure the iframe that holds the Search Companion webpage is the one
  // finished loading instead of the WebUI
  if (validated_url.host() !=
      GURL(companion::GetHomepageURLForCompanion()).host()) {
    return;
  }
  has_companion_loaded = true;
  for (auto& callback : companion_loaded_callbacks_) {
    std::move(callback).Run();
  }
  companion_loaded_callbacks_.clear();
}

}  // namespace companion
