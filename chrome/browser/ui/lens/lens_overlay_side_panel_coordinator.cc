// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_overlay_side_panel_coordinator.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/lens/lens_overlay_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_side_panel_web_view.h"
#include "chrome/browser/ui/lens/lens_overlay_url_builder.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/public/tab_interface.h"
#include "chrome/browser/ui/views/side_panel/side_panel_content_proxy.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/views/side_panel/side_panel_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "components/google/core/common/google_util.h"
#include "components/lens/lens_features.h"
#include "components/lens/lens_overlay_dismissal_source.h"
#include "components/lens/lens_overlay_invocation_source.h"
#include "components/vector_icons/vector_icons.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/referrer.h"
#include "net/base/network_change_notifier.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/page_transition_types.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view_class_properties.h"

namespace lens {

namespace {

inline constexpr char kChromeSideSearchVersionHeaderName[] =
    "X-Chrome-Side-Search-Version";
inline constexpr char kChromeSideSearchVersionHeaderValue[] = "1";

bool IsSiteTrusted(const GURL& url) {
  if (google_util::IsGoogleDomainUrl(
          url, google_util::ALLOW_SUBDOMAIN,
          google_util::DISALLOW_NON_STANDARD_PORTS)) {
    return true;
  }

  // This is a workaround for local development where the URL may be a
  // non-Google domain / proxy. If the Finch flag for the lens overlay results
  // search URL is not set to a Google domain, make sure the request is coming
  // from the results search URL page.
  if (net::registry_controlled_domains::SameDomainOrHost(
          url, GURL(lens::features::GetLensOverlayResultsSearchURL()),
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES)) {
    return true;
  }

  return false;
}

SidePanelUI* GetSidePanelUI(LensOverlayController* controller) {
  return controller->GetTabInterface()
      ->GetBrowserWindowInterface()
      ->GetFeatures()
      .side_panel_ui();
}

}  // namespace

LensOverlaySidePanelCoordinator::LensOverlaySidePanelCoordinator(
    LensOverlayController* lens_overlay_controller)
    : lens_overlay_controller_(lens_overlay_controller) {}

LensOverlaySidePanelCoordinator::~LensOverlaySidePanelCoordinator() {
  // If the coordinator is destroyed before the web view, clear the reference
  // from the web view.
  if (side_panel_web_view_) {
    side_panel_web_view_->ClearCoordinator();
    side_panel_web_view_ = nullptr;
  }

  auto* registry = lens_overlay_controller_->GetTabInterface()
                       ->GetTabFeatures()
                       ->side_panel_registry();
  CHECK(registry);

  // Remove the side panel entry observer if it is present.
  auto* registered_entry = registry->GetEntryForKey(
      SidePanelEntry::Key(SidePanelEntry::Id::kLensOverlayResults));
  if (registered_entry) {
    registered_entry->RemoveObserver(this);
  }

  // This is a no-op if the entry does not exist.
  registry->Deregister(
      SidePanelEntry::Key(SidePanelEntry::Id::kLensOverlayResults));
}

void LensOverlaySidePanelCoordinator::RegisterEntryAndShow() {
  RegisterEntry();
  GetSidePanelUI(lens_overlay_controller_)
      ->Show(SidePanelEntry::Id::kLensOverlayResults);
  lens_overlay_controller_->NotifyResultsPanelOpened();
}

void LensOverlaySidePanelCoordinator::OnEntryWillHide(
    SidePanelEntry* entry,
    SidePanelEntryHideReason reason) {
  lens_overlay_controller_->OnSidePanelWillHide(reason);
}

void LensOverlaySidePanelCoordinator::OnEntryHidden(SidePanelEntry* entry) {
  // We cannot distinguish between:
  //   (1) A teardown during the middle of the async close process from
  //   LensOverlayController.
  //   (2) The user clicked the 'x' button while the overlay is showing.
  //   (3) The side panel naturally went away after a tab switch.
  // Forward to LensOverlayController to have it disambiguate.
  lens_overlay_controller_->OnSidePanelHidden();
}

void LensOverlaySidePanelCoordinator::WebViewClosing() {
  // This is called from the destructor of the WebView. Synchronously clear all
  // state associated with the WebView.
  if (side_panel_web_view_) {
    lens_overlay_controller_->ResetSidePanelSearchboxHandler();
    lens_overlay_controller_->RemoveGlueForWebView(side_panel_web_view_);
    side_panel_web_view_ = nullptr;
  }
}

content::WebContents*
LensOverlaySidePanelCoordinator::GetSidePanelWebContents() {
  if (side_panel_web_view_) {
    return side_panel_web_view_->GetWebContents();
  }
  return nullptr;
}

bool LensOverlaySidePanelCoordinator::IsEntryShowing() {
  return GetSidePanelUI(lens_overlay_controller_)
      ->IsSidePanelEntryShowing(
          SidePanelEntry::Key(SidePanelEntry::Id::kLensOverlayResults));
}

// This method is called when the WebContents wants to open a link in a new
// tab (e.g. an anchor tag with target="_blank"). This delegate does not
// override AddNewContents(), so the WebContents is not actually created.
// Instead it forwards the parameters to the real browser. The navigation
// throttle is not sufficient to handle this because it only handles navigations
// within the same web contents.
void LensOverlaySidePanelCoordinator::DidOpenRequestedURL(
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

  // We can't open a new tab while the observer is running because it might
  // destroy this WebContents. Post as task instead.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&LensOverlaySidePanelCoordinator::OpenURLInBrowser,
                     weak_ptr_factory_.GetWeakPtr(), std::move(params)));
}

void LensOverlaySidePanelCoordinator::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  // Focus the web contents immediately, so that hotkey presses (i.e. escape)
  // are handled.
  GetSidePanelWebContents()->Focus();
  // We only care about the navigation if it is the results frame, is HTTPS,
  // renderer initiated and NOT a same document navigation.
  if (!navigation_handle->IsRendererInitiated() ||
      !navigation_handle->GetURL().SchemeIsHTTPOrHTTPS() ||
      navigation_handle->IsSameDocument() ||
      navigation_handle->IsInPrimaryMainFrame() ||
      !navigation_handle->GetParentFrame() ||
      !navigation_handle->GetParentFrame()->IsInPrimaryMainFrame()) {
    return;
  }

  // Any navigation that the results iframe attempts to a different domain
  // will fail. Since the navigation throttle may not be able to intercept
  // certain navigations before they result in an error page, we should make
  // sure these error pages don't commit and instead open these URLs in a new
  // tab.
  if (!lens::IsValidSearchResultsUrl(navigation_handle->GetURL()) &&
      lens::GetSearchResultsUrlFromRedirectUrl(navigation_handle->GetURL())
          .is_empty()) {
    navigation_handle->SetSilentlyIgnoreErrors();
    lens_overlay_controller_->GetTabInterface()
        ->GetBrowserWindowInterface()
        ->OpenGURL(navigation_handle->GetURL(),
                   WindowOpenDisposition::NEW_FOREGROUND_TAB);
    return;
  }

  // If we expect to load this URL in the side panel, show the loading
  // page and any feature-specific request headers.
  navigation_handle->SetRequestHeader(kChromeSideSearchVersionHeaderName,
                                      kChromeSideSearchVersionHeaderValue);
  lens_overlay_controller_->SetSidePanelShowErrorPage(
      net::NetworkChangeNotifier::IsOffline());
  lens_overlay_controller_->SetSidePanelIsLoadingResults(true);
}

void LensOverlaySidePanelCoordinator::DOMContentLoaded(
    content::RenderFrameHost* render_frame_host) {
  // We only care about loads that happen one level down in the side panel.
  if (!render_frame_host->GetParent() ||
      render_frame_host->GetParent()->GetParent() ||
      !render_frame_host->GetLastCommittedURL().SchemeIsHTTPOrHTTPS()) {
    return;
  }

  lens_overlay_controller_->SetSidePanelIsLoadingResults(false);
}

web_modal::WebContentsModalDialogHost*
LensOverlaySidePanelCoordinator::GetWebContentsModalDialogHost() {
  return lens_overlay_controller_->GetTabInterface()
      ->GetBrowserWindowInterface()
      ->GetWebContentsModalDialogHostForWindow();
}

void LensOverlaySidePanelCoordinator::OpenURLInBrowser(
    const content::OpenURLParams& params) {
  lens_overlay_controller_->GetTabInterface()
      ->GetBrowserWindowInterface()
      ->OpenURL(params, /*navigation_handle_callback=*/{});
}

void LensOverlaySidePanelCoordinator::RegisterEntry() {
  auto* registry = lens_overlay_controller_->GetTabInterface()
                       ->GetTabFeatures()
                       ->side_panel_registry();
  CHECK(registry);

  // If the entry is already registered, don't register it again.
  if (!registry->GetEntryForKey(
          SidePanelEntry::Key(SidePanelEntry::Id::kLensOverlayResults))) {
    // TODO(b/328295358): Change title and icon when available.
    auto entry = std::make_unique<SidePanelEntry>(
        SidePanelEntry::Id::kLensOverlayResults,
        base::BindRepeating(
            &LensOverlaySidePanelCoordinator::CreateLensOverlayResultsView,
            base::Unretained(this)),
        base::BindRepeating(
            &LensOverlaySidePanelCoordinator::GetOpenInNewTabUrl,
            base::Unretained(this)),
        GetMoreInfoCallback());
    entry->SetProperty(kShouldShowTitleInSidePanelHeaderKey, false);
    registry->Register(std::move(entry));

    // Observe the side panel entry.
    auto* registered_entry = registry->GetEntryForKey(
        SidePanelEntry::Key(SidePanelEntry::Id::kLensOverlayResults));
    registered_entry->AddObserver(this);
  }
}

std::unique_ptr<views::View>
LensOverlaySidePanelCoordinator::CreateLensOverlayResultsView() {
  // TODO(b/328295358): Change task manager string ID in view creation when
  // available.
  auto view = std::make_unique<LensOverlaySidePanelWebView>(
      lens_overlay_controller_->GetTabInterface()
          ->GetContents()
          ->GetBrowserContext(),
      this);
  view->SetProperty(views::kElementIdentifierKey,
                    LensOverlayController::kOverlaySidePanelWebViewId);
  side_panel_web_view_ = view.get();
  Observe(GetSidePanelWebContents());

  // Register the modal dialog manager for this side panel web contents so
  // browser dialogs can open when requested by the side panel WebUI.
  web_modal::WebContentsModalDialogManager::CreateForWebContents(
      GetSidePanelWebContents());
  web_modal::WebContentsModalDialogManager::FromWebContents(
      GetSidePanelWebContents())
      ->SetDelegate(this);

  // Important safety note: creating the SidePanelWebUIViewT can result in
  // synchronous construction of the WebUIController. Until
  // "CreateGlueForWebView" is called below, the WebUIController will not be
  // able to access to LensOverlayController.
  lens_overlay_controller_->CreateGlueForWebView(view.get());
  view->SetVisible(true);
  SidePanelUtil::GetSidePanelContentProxy(view.get())->SetAvailable(true);
  return view;
}

GURL LensOverlaySidePanelCoordinator::GetOpenInNewTabUrl() {
  return GURL();
}

base::RepeatingCallback<std::unique_ptr<ui::MenuModel>()>
LensOverlaySidePanelCoordinator::GetMoreInfoCallback() {
  if (lens::features::IsLensOverlaySearchBubbleEnabled()) {
    return base::BindRepeating(
        &LensOverlaySidePanelCoordinator::GetMoreInfoMenuModel,
        base::Unretained(this));
  }
  return base::NullCallbackAs<std::unique_ptr<ui::MenuModel>()>();
}

std::unique_ptr<ui::MenuModel>
LensOverlaySidePanelCoordinator::GetMoreInfoMenuModel() {
  auto menu_model = std::make_unique<ui::SimpleMenuModel>(this);
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  menu_model->AddItemWithIcon(
      COMMAND_MY_ACTIVITY,
      l10n_util::GetStringUTF16(IDS_LENS_OVERLAY_MY_ACTIVITY),
      ui::ImageModel::FromVectorIcon(vector_icons::kGoogleGLogoMonochromeIcon,
                                     ui::kColorMenuIcon,
                                     ui::SimpleMenuModel::kDefaultIconSize));
#else
  menu_model->AddItem(COMMAND_MY_ACTIVITY,
                      l10n_util::GetStringUTF16(IDS_LENS_OVERLAY_MY_ACTIVITY));
#endif
  menu_model->AddItemWithIcon(
      COMMAND_LEARN_MORE,
      l10n_util::GetStringUTF16(IDS_LENS_OVERLAY_LEARN_MORE),
      ui::ImageModel::FromVectorIcon(vector_icons::kInfoOutlineIcon,
                                     ui::kColorMenuIcon,
                                     ui::SimpleMenuModel::kDefaultIconSize));
  menu_model->AddItemWithIcon(
      COMMAND_SEND_FEEDBACK, l10n_util::GetStringUTF16(IDS_LENS_SEND_FEEDBACK),
      ui::ImageModel::FromVectorIcon(kSubmitFeedbackIcon, ui::kColorMenuIcon,
                                     ui::SimpleMenuModel::kDefaultIconSize));
  return menu_model;
}

void LensOverlaySidePanelCoordinator::ExecuteCommand(int command_id,
                                                     int event_flags) {
  switch (command_id) {
    case COMMAND_MY_ACTIVITY: {
      lens_overlay_controller_->ActivityRequestedByEvent(event_flags);
      break;
    }
    case COMMAND_LEARN_MORE: {
      lens_overlay_controller_->InfoRequestedByEvent(event_flags);
      break;
    }
    case COMMAND_SEND_FEEDBACK: {
      lens_overlay_controller_->FeedbackRequestedByEvent(event_flags);
      break;
    }
    default: {
      NOTREACHED() << "Unknown option";
    }
  }
}

}  // namespace lens
