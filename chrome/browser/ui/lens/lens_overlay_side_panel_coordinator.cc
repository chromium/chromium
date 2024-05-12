// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_overlay_side_panel_coordinator.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/lens/lens_overlay_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_url_builder.h"
#include "chrome/browser/ui/lens/lens_untrusted_ui.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/views/side_panel/side_panel_content_proxy.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_util.h"
#include "chrome/browser/ui/views/side_panel/side_panel_web_ui_view.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/google/core/common/google_util.h"
#include "components/lens/lens_features.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/referrer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/page_transition_types.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view_class_properties.h"

using SidePanelWebUIViewT_LensUntrustedUI =
    SidePanelWebUIViewT<lens::LensUntrustedUI>;
BEGIN_TEMPLATE_METADATA(SidePanelWebUIViewT_LensUntrustedUI,
                        SidePanelWebUIViewT)
END_METADATA

namespace lens {

namespace {
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

}  // namespace

LensOverlaySidePanelCoordinator::LensOverlaySidePanelCoordinator(
    Browser* browser,
    LensOverlayController* lens_overlay_controller,
    SidePanelUI* side_panel_ui,
    content::WebContents* web_contents)
    : tab_browser_(browser),
      lens_overlay_controller_(lens_overlay_controller),
      side_panel_ui_(side_panel_ui),
      tab_web_contents_(web_contents->GetWeakPtr()) {}

LensOverlaySidePanelCoordinator::~LensOverlaySidePanelCoordinator() {
  DeregisterEntry();
}

// static
actions::ActionItem::InvokeActionCallback
LensOverlaySidePanelCoordinator::CreateSidePanelActionCallback(
    Browser* browser) {
  return base::BindRepeating(
      [](Browser* browser, actions::ActionItem* item,
         actions::ActionInvocationContext context) {
        LensOverlayController* controller =
            LensOverlayController::GetController(
                browser->tab_strip_model()->GetActiveWebContents());
        DCHECK(controller);

        // Toggle the Lens overlay. There's no need to show or hide the side
        // panel as the overlay controller will handle that.
        if (controller->IsOverlayShowing()) {
          controller->CloseUIAsync(
              LensOverlayController::DismissalSource::kToolbar);
        } else {
          controller->ShowUI(LensOverlayController::InvocationSource::kToolbar);
        }
      },
      browser);
}

void LensOverlaySidePanelCoordinator::RegisterEntryAndShow() {
  RegisterEntry();
  side_panel_ui_->Show(SidePanelEntry::Id::kLensOverlayResults);
  lens_overlay_controller_->NotifyResultsPanelOpened();
}

void LensOverlaySidePanelCoordinator::OnEntryHidden(SidePanelEntry* entry) {
  // Only deregister the entry if the overlay is showing. This prevents the
  // side panel entry closing while the overlay is open on a backgrounded tab.
  if (lens_overlay_controller_->IsOverlayShowing()) {
    DeregisterEntry();
  }
}

content::WebContents*
LensOverlaySidePanelCoordinator::GetSidePanelWebContents() {
  return side_panel_web_view_->GetWebContents();
}

bool LensOverlaySidePanelCoordinator::IsEntryShowing() {
  return side_panel_ui_->IsSidePanelEntryShowing(
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
  Browser* browser = chrome::FindBrowserWithTab(GetTabWebContents());
  if (!browser) {
    return;
  }
  browser->OpenURL(params, /*navigation_handle_callback=*/{});
}

void LensOverlaySidePanelCoordinator::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
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
  if (!lens::IsValidSearchResultsUrl(navigation_handle->GetURL())) {
    auto params =
        content::OpenURLParams::FromNavigationHandle(navigation_handle);
    params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
    Browser* browser = chrome::FindBrowserWithTab(GetTabWebContents());
    if (!browser) {
      return;
    }
    navigation_handle->SetSilentlyIgnoreErrors();
    browser->OpenURL(params, /*navigation_handle_callback=*/{});
    return;
  }

  // If we expect to load this URL in the side panel, show the loading
  // page.
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

void LensOverlaySidePanelCoordinator::RegisterEntry() {
  auto* registry = SidePanelRegistry::Get(GetTabWebContents());
  CHECK(registry);

  // If the entry is already registered, don't register it again.
  if (!registry->GetEntryForKey(
          SidePanelEntry::Key(SidePanelEntry::Id::kLensOverlayResults))) {
    // TODO(b/328295358): Change title and icon when available.
    auto entry = std::make_unique<SidePanelEntry>(
        SidePanelEntry::Id::kLensOverlayResults,
        l10n_util::GetStringUTF16(IDS_SIDE_PANEL_COMPANION_TITLE),
        ui::ImageModel::FromVectorIcon(vector_icons::kSearchIcon,
                                       ui::kColorIcon,
                                       /*icon_size=*/16),
        base::BindRepeating(
            &LensOverlaySidePanelCoordinator::CreateLensOverlayResultsView,
            base::Unretained(this)),
        base::BindRepeating(
            &LensOverlaySidePanelCoordinator::GetOpenInNewTabUrl,
            base::Unretained(this)));
    registry->Register(std::move(entry));

    // Observe the side panel entry.
    auto* registered_entry = registry->GetEntryForKey(
        SidePanelEntry::Key(SidePanelEntry::Id::kLensOverlayResults));
    registered_entry->AddObserver(this);
  }
}

void LensOverlaySidePanelCoordinator::DeregisterEntry() {
  auto* registry = SidePanelRegistry::Get(GetTabWebContents());
  CHECK(registry);
  // If the side panel web view was created, then we need to release the
  // associated searchbox handler and remove the glue to the overlay controller
  // if it is present.
  if (side_panel_web_view_) {
    lens_overlay_controller_->ResetSearchboxHandler();
    lens_overlay_controller_->RemoveGlueForWebView(side_panel_web_view_);
    side_panel_web_view_ = nullptr;
  }

  // Remove the side panel entry observer if it is present.
  auto* registered_entry = registry->GetEntryForKey(
      SidePanelEntry::Key(SidePanelEntry::Id::kLensOverlayResults));
  if (registered_entry) {
    registered_entry->RemoveObserver(this);
  }

  // Notifies the Lens overlay to handle the entry deregistering.
  lens_overlay_controller_->OnSidePanelEntryDeregistered();

  // This is a no-op if the entry does not exist.
  registry->Deregister(
      SidePanelEntry::Key(SidePanelEntry::Id::kLensOverlayResults));
}

std::unique_ptr<views::View>
LensOverlaySidePanelCoordinator::CreateLensOverlayResultsView() {
  // TODO(b/328295358): Change task manager string ID in view creation when
  // available.
  auto view = std::make_unique<SidePanelWebUIViewT<lens::LensUntrustedUI>>(
      base::RepeatingClosure(), base::RepeatingClosure(),
      std::make_unique<WebUIContentsWrapperT<lens::LensUntrustedUI>>(
          GURL(chrome::kChromeUILensUntrustedSidePanelURL),
          tab_browser_->profile(), IDS_SIDE_PANEL_COMPANION_TITLE,
          /*webui_resizes_host=*/false,
          /*esc_closes_ui=*/false));
  view->SetProperty(views::kElementIdentifierKey,
                    LensOverlayController::kOverlaySidePanelWebViewId);
  side_panel_web_view_ = view.get();
  Observe(GetSidePanelWebContents());
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

content::WebContents* LensOverlaySidePanelCoordinator::GetTabWebContents() {
  content::WebContents* tab_contents = tab_web_contents_.get();
  CHECK(tab_contents);
  return tab_contents;
}

}  // namespace lens
