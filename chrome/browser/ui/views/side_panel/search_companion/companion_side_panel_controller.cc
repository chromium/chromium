// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/search_companion/companion_side_panel_controller.h"

#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/companion/core/features.h"
#include "chrome/browser/companion/core/utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/companion/companion_tab_helper.h"
#include "chrome/browser/ui/views/side_panel/companion_side_panel_web_view.h"
#include "chrome/browser/ui/views/side_panel/search_companion/search_companion_side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_util.h"
#include "chrome/browser/ui/webui/side_panel/companion/companion_page_handler.h"
#include "chrome/browser/ui/webui/side_panel/companion/companion_side_panel_untrusted_ui.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "components/google/core/common/google_util.h"
#include "components/lens/buildflags.h"
#include "components/lens/lens_url_utils.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "third_party/blink/public/mojom/frame/user_activation_notification_type.mojom.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/webui/webui_allowlist.h"

#if BUILDFLAG(ENABLE_LENS_DESKTOP_GOOGLE_BRANDED_FEATURES)
#include "chrome/browser/ui/views/side_panel/lens/lens_unified_side_panel_view.h"
#endif

namespace companion {

CompanionSidePanelController::CompanionSidePanelController(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {}

CompanionSidePanelController::~CompanionSidePanelController() {
  panel_container_view_ = nullptr;
}

void CompanionSidePanelController::CreateAndRegisterEntry() {
  auto* registry = SidePanelRegistry::GetDeprecated(web_contents_);
  Browser* browser = chrome::FindBrowserWithTab(web_contents_);
  if (!browser) {
    // If no browser was found via WebContents, it is probably because the
    // web_contents has not been attached to a window yet. Since we are only
    // using the browser to find the SearchCompanionSidePanelCoordinator, and
    // then the name and icon which don't change, it is safe to grab the
    // LastActive browser as a fallback.
    browser = chrome::FindLastActive();
  }
  if (!registry || !browser ||
      registry->GetEntryForKey(
          SidePanelEntry::Key(SidePanelEntry::Id::kSearchCompanion))) {
    return;
  }

  auto entry = std::make_unique<SidePanelEntry>(
      SidePanelEntry::Id::kSearchCompanion,
      base::BindRepeating(
          &companion::CompanionSidePanelController::CreateCompanionWebView,
          base::Unretained(this)),
      base::BindRepeating(
          &companion::CompanionSidePanelController::GetOpenInNewTabUrl,
          base::Unretained(this)));
  registry->Register(std::move(entry));
  AddObserver();

  // Give Search Companion Server 3P Cookie Permissions
  auto* webui_allowlist = WebUIAllowlist::GetOrCreate(browser->profile());
  const url::Origin companion_origin = url::Origin::Create(
      GURL(chrome::kChromeUIUntrustedCompanionSidePanelURL));
  // Allow third party cookies from companion and sign-in based flows.
  webui_allowlist->RegisterAutoGrantedThirdPartyCookies(
      companion_origin,
      {ContentSettingsPattern::FromString("https://[*.]google.com"),
       ContentSettingsPattern::FromURL(GURL(GetHomepageURLForCompanion()))});
}

void CompanionSidePanelController::DeregisterEntry() {
  auto* registry = SidePanelRegistry::GetDeprecated(web_contents_);
  if (!registry) {
    return;
  }
  RemoveObserver();
  registry->Deregister(
      SidePanelEntry::Key(SidePanelEntry::Id::kSearchCompanion));
}

void CompanionSidePanelController::ShowCompanionSidePanel(
    SidePanelOpenTrigger side_panel_open_trigger) {
  if (Browser* browser = chrome::FindBrowserWithTab(web_contents_)) {
    if (is_lens_view_showing_) {
      is_lens_view_showing_ = false;
    }
    auto* coordinator =
        SearchCompanionSidePanelCoordinator::GetOrCreateForBrowser(browser);
    coordinator->Show(side_panel_open_trigger);
  }
}

void CompanionSidePanelController::UpdateNewTabButton(GURL url_to_open) {
  open_in_new_tab_url_ = url_to_open;
  Browser* browser = chrome::FindBrowserWithTab(web_contents_);
  if (!browser) {
    return;
  }
  UpdateNewTabButtonState();
}

void CompanionSidePanelController::OnCompanionSidePanelClosed() {
  open_in_new_tab_url_ = GURL();
}

bool CompanionSidePanelController::IsCompanionShowing() {
  SidePanelRegistry* registry = SidePanelRegistry::GetDeprecated(web_contents_);
  if (!registry) {
    return false;
  }

  return registry->active_entry().has_value() &&
         registry->active_entry().value()->key() ==
             SidePanelEntry::Key(SidePanelEntry::Id::kSearchCompanion);
}

void CompanionSidePanelController::SetCompanionAsActiveEntry(
    content::WebContents* contents) {
  // It is not guaranteed that the WebContents already has an entry for CSC,
  // so we need to explicelty create one.
  companion::CompanionTabHelper::FromWebContents(contents)
      ->CreateAndRegisterEntry();

  SidePanelRegistry* new_tab_registry =
      SidePanelRegistry::GetDeprecated(contents);
  if (!new_tab_registry) {
    return;
  }
  SidePanelEntry* entry = new_tab_registry->GetEntryForKey(
      SidePanelEntry::Key(SidePanelEntry::Id::kSearchCompanion));
  if (!entry) {
    return;
  }
  new_tab_registry->SetActiveEntry(entry);
}

content::WebContents*
CompanionSidePanelController::GetCompanionWebContentsForTesting() {
  return web_contents();
}

void CompanionSidePanelController::OnEntryShown(SidePanelEntry* entry) {
  // Lens entry is possible if image search is disabled on CSC.
  if (is_lens_view_showing_) {
    base::RecordAction(
        base::UserMetricsAction("LensUnifiedSidePanel.LensEntryShown"));
  }
}

void CompanionSidePanelController::OnEntryHidden(SidePanelEntry* entry) {
  panel_container_view_ = nullptr;
  // Lens entry is possible if image search is disabled on CSC.
  if (is_lens_view_showing_) {
    base::RecordAction(
        base::UserMetricsAction("LensUnifiedSidePanel.LensEntryHidden"));
  }
}

void CompanionSidePanelController::AddObserver() {
  auto* entry = SidePanelRegistry::GetDeprecated(web_contents_)
                    ->GetEntryForKey(SidePanelEntry::Key(
                        SidePanelEntry::Id::kSearchCompanion));
  entry->AddObserver(this);
}

void CompanionSidePanelController::RemoveObserver() {
  auto* entry = SidePanelRegistry::GetDeprecated(web_contents_)
                    ->GetEntryForKey(SidePanelEntry::Key(
                        SidePanelEntry::Id::kSearchCompanion));
  entry->RemoveObserver(this);
}

std::unique_ptr<views::View>
CompanionSidePanelController::CreateCompanionWebView() {
  auto panel_container_view = std::make_unique<views::FlexLayoutView>();
  panel_container_view_ = panel_container_view.get();

  // Set layout properties for the parent container view.
  panel_container_view->SetOrientation(views::LayoutOrientation::kVertical);
  panel_container_view->SetMainAxisAlignment(views::LayoutAlignment::kStart);
  panel_container_view->SetCrossAxisAlignment(views::LayoutAlignment::kStretch);

  // If a view was already create to be shown.
  if (future_content_view_) {
    panel_container_view->AddChildView(std::move(future_content_view_));
    return panel_container_view;
  }

  // Allow child view to expand indefinitely.
  auto* companion_web_view = panel_container_view->AddChildView(
      std::make_unique<CompanionSidePanelWebView>(
          Profile::FromBrowserContext(web_contents_->GetBrowserContext())));
  companion_web_view->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded));

  // Observe on the webcontents for opening links in new tab.
  Observe(companion_web_view->GetWebContents());
  return panel_container_view;
}

GURL CompanionSidePanelController::GetOpenInNewTabUrl() {
  // The start time needs to be updated when the url is fetched for opening
  // to properly log the loading latency in Lens.
  open_in_new_tab_url_ =
      lens::AppendOrReplaceStartTimeIfLensRequest(open_in_new_tab_url_);
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
  if (!IsSiteTrusted(
          source_render_frame_host->GetLastCommittedOrigin().GetURL())) {
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
  // Do not open search URLs as we will open them in the companion instead.
  bool should_open_url =
      !(google_util::IsGoogleSearchUrl(url) && url.path_piece() == "/search");
  params.disposition = open_in_current_tab
                           ? WindowOpenDisposition::CURRENT_TAB
                           : WindowOpenDisposition::NEW_FOREGROUND_TAB;

  Browser* browser = chrome::FindBrowserWithTab(web_contents_);
  if (!browser) {
    return;
  }

  // Open the url in the desired tab.
  content::WebContents* tab_web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  if (should_open_url) {
    tab_web_contents =
        browser->OpenURL(params, /*navigation_handle_callback=*/{});

    if (open_in_current_tab && tab_web_contents) {
      // Add metrics to record the open trigger for the companion page as a link
      // click from side panel. Note, the user can click on links even before
      // the metrics is consumed, e.g. a double click. Either way, just
      // overwrite the metrics if it already exists.
      auto* tab_helper =
          companion::CompanionTabHelper::FromWebContents(tab_web_contents);
      tab_helper->SetMostRecentSidePanelOpenTrigger(
          SidePanelOpenTrigger::kOpenedInNewTabFromSidePanel);
    }

    // If a new tab was opened, open companion side panel in it.
    if (!open_in_current_tab && tab_web_contents) {
      browser->GetFeatures().side_panel_ui()->Show(
          SidePanelEntry::Id::kSearchCompanion,
          SidePanelOpenTrigger::kOpenedInNewTabFromSidePanel);
    }
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
    auto link_open_action =
        open_in_current_tab
            ? side_panel::mojom::LinkOpenMetadata::LinkOpenAction::kClobbered
            : side_panel::mojom::LinkOpenMetadata::LinkOpenAction::kNewTab;
    auto metadata = side_panel::mojom::LinkOpenMetadata::New(
        (should_open_url
             ? link_open_action
             : side_panel::mojom::LinkOpenMetadata::LinkOpenAction::kIgnored),
        is_entry_point_default_pinned);
    companion_helper->AddCompanionFinishedLoadingCallback(
        base::BindOnce(&CompanionSidePanelController::NotifyLinkClick,
                       weak_ptr_factory_.GetWeakPtr(), url, std::move(metadata),
                       tab_web_contents));
  }
}

void CompanionSidePanelController::FrameSizeChanged(
    content::RenderFrameHost* render_frame_host,
    const gfx::Size& frame_size) {
  // We need to wait for the WebContents to have bounds before issuing the Lens
  // request. This method gets notified once the WebContents has bounds, so we
  // can issue the Lens request.
  if (render_frame_host && !render_frame_host->GetParent()) {
    auto* tab_helper =
        companion::CompanionTabHelper::FromWebContents(web_contents_);
    std::unique_ptr<side_panel::mojom::ImageQuery> image_query =
        tab_helper->GetImageQuery();
    if (!image_query) {
      return;
    }
    tab_helper->GetCompanionPageHandler()->OnImageQuery(*image_query);
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
  if (has_companion_loaded_) {
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
  has_companion_loaded_ = true;
  for (auto& callback : companion_loaded_callbacks_) {
    std::move(callback).Run();
  }
  companion_loaded_callbacks_.clear();
}

void CompanionSidePanelController::OpenContextualLensView(
    const content::OpenURLParams& params) {
  CHECK(web_contents_);
  Browser* browser = chrome::FindBrowserWithTab(web_contents_);
  auto* registry = SidePanelRegistry::GetDeprecated(web_contents_);
  if (!browser || !registry) {
    return;
  }
  if (panel_container_view_) {
    auto lens_view = CreateContextualLensView(params);
    panel_container_view_->RemoveAllChildViews();
    panel_container_view_->AddChildView(std::move(lens_view));
  } else {
    // This means the panel is not open yet.
    future_content_view_ = CreateContextualLensView(params);
  }
  is_lens_view_showing_ = true;
}

std::unique_ptr<views::View>
CompanionSidePanelController::CreateContextualLensView(
    const content::OpenURLParams& params) {
#if BUILDFLAG(ENABLE_LENS_DESKTOP_GOOGLE_BRANDED_FEATURES)
  Browser* browser = chrome::FindBrowserWithTab(web_contents_);
  if (!browser) {
    return nullptr;
  }
  auto side_panel_view = std::make_unique<lens::LensUnifiedSidePanelView>(
      BrowserView::GetBrowserViewForBrowser(browser),
      base::BindRepeating(
          &CompanionSidePanelController::UpdateNewTabButtonState,
          base::Unretained(this)));
  side_panel_view->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded));
  side_panel_view->OpenUrl(params);
  lens_side_panel_view_ = side_panel_view->GetWeakPtr();
  return side_panel_view;
#else
  return nullptr;
#endif
}

GURL CompanionSidePanelController::GetLensOpenInNewTabButtonURL() {
#if BUILDFLAG(ENABLE_LENS_DESKTOP_GOOGLE_BRANDED_FEATURES)
  return lens_side_panel_view_ ? lens_side_panel_view_->GetOpenInNewTabURL()
                               : GURL();
#else
  return GURL();
#endif
}

void CompanionSidePanelController::UpdateNewTabButtonState() {
#if BUILDFLAG(ENABLE_LENS_DESKTOP_GOOGLE_BRANDED_FEATURES)
  Browser* browser = chrome::FindBrowserWithTab(web_contents_);
  if (!browser) {
    return;
  }
  auto* coordinator = browser->GetFeatures().side_panel_coordinator();
  if (!coordinator) {
    return;
  }
  coordinator->UpdateNewTabButtonState();
#endif
}

content::WebContents*
CompanionSidePanelController::GetLensViewWebContentsForTesting() {
#if BUILDFLAG(ENABLE_LENS_DESKTOP_GOOGLE_BRANDED_FEATURES)
  CHECK(lens_side_panel_view_);
  return lens_side_panel_view_ ? lens_side_panel_view_->GetWebContents()
                               : nullptr;
#else
  return nullptr;
#endif
}

bool CompanionSidePanelController::IsLensLaunchButtonEnabledForTesting() {
#if BUILDFLAG(ENABLE_LENS_DESKTOP_GOOGLE_BRANDED_FEATURES)
  CHECK(lens_side_panel_view_);
  return lens_side_panel_view_->IsLaunchButtonEnabledForTesting();  // IN-TEST
#else
  return false;
#endif
}

bool CompanionSidePanelController::OpenLensResultsInNewTabForTesting() {
#if BUILDFLAG(ENABLE_LENS_DESKTOP_GOOGLE_BRANDED_FEATURES)
  if (!lens_side_panel_view_) {
    return false;
  }
  lens_side_panel_view_->LoadResultsInNewTab();
  return true;
#else
  return false;
#endif
}

}  // namespace companion
