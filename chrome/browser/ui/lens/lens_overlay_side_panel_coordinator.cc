// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_overlay_side_panel_coordinator.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/companion/text_finder/text_finder_manager.h"
#include "chrome/browser/companion/text_finder/text_highlighter_manager.h"
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
#include "components/lens/lens_overlay_metrics.h"
#include "components/lens/lens_overlay_side_panel_menu_option.h"
#include "components/lens/lens_overlay_side_panel_result.h"
#include "components/shared_highlighting/core/common/fragment_directives_utils.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/referrer.h"
#include "net/base/network_change_notifier.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/page_transition_types.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view_class_properties.h"

#if BUILDFLAG(ENABLE_PDF)
#include "chrome/browser/pdf/pdf_extension_util.h"
#include "components/pdf/browser/pdf_document_helper.h"
#endif  // BUILDFLAG(ENABLE_PDF)

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

bool LensOverlaySidePanelCoordinator::MaybeHandleTextDirectives(
    const GURL& nav_url) {
  if (ShouldHandleTextDirectives(nav_url)) {
    const GURL& page_url = lens_overlay_controller_->GetTabInterface()
                               ->GetContents()
                               ->GetLastCommittedURL();
    // Need to check if the page URL matches the navigation URL again. This is
    // because in the case of the navigation URL being a search URL with a text
    // fragment, it should open in a new tab instead of the side panel. This
    // also adds an additional check to make sure the text query parameters
    // match.
    if (lens::IsValidSearchResultsUrl(nav_url)) {
      auto page_url_text_query = lens::GetTextQueryParameterValue(page_url);
      auto nav_url_text_query = lens::GetTextQueryParameterValue(nav_url);
      if (page_url.host() != nav_url.host() ||
          page_url.path() != nav_url.path() ||
          page_url_text_query != nav_url_text_query) {
        lens_overlay_controller_->GetTabInterface()
            ->GetBrowserWindowInterface()
            ->OpenGURL(nav_url, WindowOpenDisposition::NEW_FOREGROUND_TAB);
        return true;
      }
    }

    // Nav url should have a text fragment.
    auto text_fragments =
        shared_highlighting::ExtractTextFragments(nav_url.ref());

    // Create and attach a `TextFinderManager` to the primary page.
    content::Page& page = lens_overlay_controller_->GetTabInterface()
                              ->GetContents()
                              ->GetPrimaryPage();
    companion::TextFinderManager* text_finder_manager =
        companion::TextFinderManager::GetOrCreateForPage(page);
    text_finder_manager->CreateTextFinders(
        text_fragments,
        base::BindOnce(
            &LensOverlaySidePanelCoordinator::OnTextFinderLookupComplete,
            weak_ptr_factory_.GetWeakPtr(), nav_url));
    return true;
  }
  return false;
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

  const GURL& nav_url = navigation_handle->GetURL();

  // We only care about the navigation if it is the results frame, is HTTPS,
  // renderer initiated and NOT a same document navigation.
  if (!navigation_handle->IsRendererInitiated() ||
      !nav_url.SchemeIsHTTPOrHTTPS() || navigation_handle->IsSameDocument() ||
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
  if (!lens::IsValidSearchResultsUrl(nav_url) &&
      lens::GetSearchResultsUrlFromRedirectUrl(nav_url).is_empty()) {
    navigation_handle->SetSilentlyIgnoreErrors();

#if BUILDFLAG(ENABLE_PDF)
    content::WebContents* web_contents =
        lens_overlay_controller_->GetTabInterface()->GetContents();

    // If a PDFDocumentHelper is found attached to the current web contents,
    // that means that the PDF viewer is currently loaded in it.
    if (ShouldHandlePDFViewportChange(nav_url)) {
      auto* pdf_helper =
          pdf::PDFDocumentHelper::MaybeGetForWebContents(web_contents);
      if (pdf_helper) {
        pdf_extension_util::DispatchShouldUpdateViewportEvent(
            web_contents->GetPrimaryMainFrame(), nav_url);
        return;
      }
    }
#endif  // BUILDFLAG(ENABLE_PDF)

    // If the contextual search box is enabled, cross-origin navigations could
    // be a citation that should be rendered as text highlights in the current
    // tab.
    if (MaybeHandleTextDirectives(nav_url)) {
      return;
    }

    lens_overlay_controller_->GetTabInterface()
        ->GetBrowserWindowInterface()
        ->OpenGURL(nav_url, WindowOpenDisposition::NEW_FOREGROUND_TAB);
    return;
  }

  // If the search URL should be opened in a new tab, open it here.
  if (ShouldOpenSearchURLInNewTab(nav_url)) {
    lens_overlay_controller_->GetTabInterface()
        ->GetBrowserWindowInterface()
        ->OpenGURL(nav_url, WindowOpenDisposition::NEW_FOREGROUND_TAB);
    return;
  }

  // If the query has text directives, return early to allow the navigation
  // throttle to handle the request. Have to check `ShouldHandleTextDirectives`
  // separately in case the navigation happens to be a citation on a valid
  // search result that would typically be loaded in the side panel. In this
  // case, the navigation throttle will cancel the navigation and call
  // `MaybeHandleTextDirectives` directly.
  if (ShouldHandleTextDirectives(nav_url)) {
    return;
  }

  // If we expect to load this URL in the side panel, show the loading
  // page and any feature-specific request headers.
  navigation_handle->SetRequestHeader(kChromeSideSearchVersionHeaderName,
                                      kChromeSideSearchVersionHeaderValue);
  lens_overlay_controller_->SetSidePanelIsOffline(
      net::NetworkChangeNotifier::IsOffline());
  lens_overlay_controller_->SetSidePanelNewTabUrl(GURL());
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

  lens_overlay_controller_->SetSidePanelNewTabUrl(
      render_frame_host->GetLastCommittedURL());
  lens_overlay_controller_->SetSidePanelIsLoadingResults(false);
}

web_modal::WebContentsModalDialogHost*
LensOverlaySidePanelCoordinator::GetWebContentsModalDialogHost() {
  return lens_overlay_controller_->GetTabInterface()
      ->GetBrowserWindowInterface()
      ->GetWebContentsModalDialogHostForWindow();
}

bool LensOverlaySidePanelCoordinator::ShouldHandleTextDirectives(
    const GURL& nav_url) {
  // Only handle text directives if the feature is enabled and the overlay is
  // not covering the current tab.
  if (!lens::features::HandleSidePanelTextDirectivesEnabled() ||
      lens_overlay_controller_->IsOverlayShowing()) {
    return false;
  }

  const GURL& page_url = lens_overlay_controller_->GetTabInterface()
                             ->GetContents()
                             ->GetLastCommittedURL();
  // Only handle text directives when the page URL and the URL being navigated
  // to have the same host and path, or if the URL being navigated to is result
  // search URL with a text fragment then it needs custom handling to open in a
  // new tab rather than in the side panel. This ignores the ref and query
  // attributes.
  if ((page_url.host() != nav_url.host() ||
       page_url.path() != nav_url.path()) &&
      !lens::IsValidSearchResultsUrl(nav_url)) {
    return false;
  }

  auto text_fragments =
      shared_highlighting::ExtractTextFragments(nav_url.ref());
  // If the url that is being navigated to does not have a text directive, then
  // it cannot be handled.
  return !text_fragments.empty();
}

bool LensOverlaySidePanelCoordinator::ShouldHandlePDFViewportChange(
    const GURL& nav_url) {
  // Only handle text directives if the feature is enabled and the overlay is
  // not covering the current tab.
  if (!lens::features::HandleSidePanelTextDirectivesEnabled() ||
      lens_overlay_controller_->IsOverlayShowing()) {
    return false;
  }

  const GURL& page_url = lens_overlay_controller_->GetTabInterface()
                             ->GetContents()
                             ->GetLastCommittedURL();
  // Handle the PDF hash change if the URL being navigated to is the same as the
  // URL loaded in the main tab. The URL being navigated to should also contain
  // a fragment with viewport parameters that will be parsed in the extension.
  return !nav_url.ref().empty() && page_url.host() == nav_url.host() &&
         page_url.path() == nav_url.path() &&
         page_url.query() == nav_url.query();
}

void LensOverlaySidePanelCoordinator::OnTextFinderLookupComplete(
    const GURL& nav_url,
    const std::vector<std::pair<std::string, bool>>& lookup_results) {
  const GURL& page_url = lens_overlay_controller_->GetTabInterface()
                             ->GetContents()
                             ->GetLastCommittedURL();
  if (lookup_results.empty()) {
    if (URLsMatchWithoutTextFragment(page_url, nav_url)) {
      lens_overlay_controller_->ShowToastInSidePanel(l10n_util::GetStringUTF8(
          IDS_LENS_OVERLAY_TOAST_PAGE_CONTENT_NOT_FOUND_MESSAGE));
      return;
    }

    lens_overlay_controller_->GetTabInterface()
        ->GetBrowserWindowInterface()
        ->OpenGURL(nav_url, WindowOpenDisposition::NEW_FOREGROUND_TAB);
    return;
  }

  std::vector<std::string> text_directives;
  for (auto pair : lookup_results) {
    // If any of the text fragments are not found, then open in a new tab.
    if (!pair.second) {
      if (URLsMatchWithoutTextFragment(page_url, nav_url)) {
        lens_overlay_controller_->ShowToastInSidePanel(l10n_util::GetStringUTF8(
            IDS_LENS_OVERLAY_TOAST_PAGE_CONTENT_NOT_FOUND_MESSAGE));
        return;
      }

      lens_overlay_controller_->GetTabInterface()
          ->GetBrowserWindowInterface()
          ->OpenGURL(nav_url, WindowOpenDisposition::NEW_FOREGROUND_TAB);
      return;
    }
    text_directives.push_back(pair.first);
  }

  // Delete any existing `TextHighlighterManager` on the page. Without this, any
  // text highlights after the first to be rendered on the page will not render.
  auto& page = lens_overlay_controller_->GetTabInterface()
                   ->GetContents()
                   ->GetPrimaryPage();
  if (companion::TextHighlighterManager::GetForPage(page)) {
    companion::TextHighlighterManager::DeleteForPage(page);
  }

  // If every text fragment was found, then create a text highlighter manager to
  // render the text highlights.
  companion::TextHighlighterManager* text_highlighter_manager =
      companion::TextHighlighterManager::GetOrCreateForPage(page);
  text_highlighter_manager->CreateTextHighlightersAndRemoveExisting(
      text_directives);
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
LensOverlaySidePanelCoordinator::CreateLensOverlayResultsView(
    SidePanelEntryScope& scope) {
  // TODO(b/328295358): Change task manager string ID in view creation when
  // available.
  auto view = std::make_unique<LensOverlaySidePanelWebView>(
      lens_overlay_controller_->GetTabInterface()
          ->GetContents()
          ->GetBrowserContext(),
      this, scope);
  view->SetProperty(views::kElementIdentifierKey,
                    LensOverlayController::kOverlaySidePanelWebViewId);
  side_panel_web_view_ = view.get();
  Observe(GetSidePanelWebContents());

  view->SetVisible(true);
  SidePanelUtil::GetSidePanelContentProxy(view.get())->SetAvailable(true);
  return view;
}

GURL LensOverlaySidePanelCoordinator::GetOpenInNewTabUrl() {
  if (lens::features::IsLensOverlaySidePanelOpenInNewTabEnabled()) {
    return lens_overlay_controller_->GetSidePanelNewTabUrl();
  } else {
    return GURL();
  }
}

base::RepeatingCallback<std::unique_ptr<ui::MenuModel>()>
LensOverlaySidePanelCoordinator::GetMoreInfoCallback() {
  if (lens::features::IsLensOverlayContextualSearchboxEnabled()) {
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
      ui::ImageModel::FromVectorIcon(vector_icons::kFeedbackIcon,
                                     ui::kColorMenuIcon,
                                     ui::SimpleMenuModel::kDefaultIconSize));
  return menu_model;
}

void LensOverlaySidePanelCoordinator::ExecuteCommand(int command_id,
                                                     int event_flags) {
  switch (command_id) {
    case COMMAND_MY_ACTIVITY: {
      lens::RecordSidePanelMenuOptionSelected(
          lens::LensOverlaySidePanelMenuOption::kMyActivity);
      lens_overlay_controller_->ActivityRequestedByEvent(event_flags);
      break;
    }
    case COMMAND_LEARN_MORE: {
      lens::RecordSidePanelMenuOptionSelected(
          lens::LensOverlaySidePanelMenuOption::kLearnMore);
      lens_overlay_controller_->InfoRequestedByEvent(event_flags);
      break;
    }
    case COMMAND_SEND_FEEDBACK: {
      lens::RecordSidePanelMenuOptionSelected(
          lens::LensOverlaySidePanelMenuOption::kSendFeedback);
      lens_overlay_controller_->FeedbackRequestedByEvent(event_flags);
      break;
    }
    default: {
      lens::RecordSidePanelMenuOptionSelected(
          lens::LensOverlaySidePanelMenuOption::kUnknown);
      NOTREACHED() << "Unknown option";
    }
  }
}

}  // namespace lens
