// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_search/unified_side_search_controller.h"

#include "base/functional/bind.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/side_search/side_search_utils.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/browser/ui/views/side_search/side_search_views_utils.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/search_engines/template_url_service.h"
#include "components/url_formatter/elide_url.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/navigation_handle.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/view_class_properties.h"

namespace {
class SideSearchWebView : public views::WebView {
 public:
  using WebView::WebView;

  ~SideSearchWebView() override {
    if (!web_contents())
      return;
    auto* side_contents_helper =
        SideSearchSideContentsHelper::FromWebContents(web_contents());
    if (!side_contents_helper)
      return;

    auto* tab_web_contents = side_contents_helper->GetTabWebContents();
    if (!tab_web_contents)
      return;

    auto* helper =
        SideSearchTabContentsHelper::FromWebContents(tab_web_contents);
    if (helper)
      helper->ClearSidePanelContents();
  }
};
}  // namespace

GURL UnifiedSideSearchController::GetOpenInNewTabURL() const {
  auto* active_contents = GetBrowserView()->GetActiveWebContents();
  DCHECK(active_contents);
  auto* helper = SideSearchTabContentsHelper::FromWebContents(active_contents);
  DCHECK(helper);
  const auto* template_url_service =
      TemplateURLServiceFactory::GetForProfile(GetProfile());
  return template_url_service->RemoveSideSearchParamFromURL(
      helper->last_search_url().value());
}

UnifiedSideSearchController::UnifiedSideSearchController(
    content::WebContents* web_contents)
    : content::WebContentsUserData<UnifiedSideSearchController>(*web_contents) {
  Observe(web_contents);
}

UnifiedSideSearchController::~UnifiedSideSearchController() {
  Observe(nullptr);
}

bool UnifiedSideSearchController::HandleKeyboardEvent(
    content::WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  auto* browser_view = GetBrowserView();
  return browser_view ? unhandled_keyboard_event_handler_.HandleKeyboardEvent(
                            event, browser_view->GetFocusManager())
                      : false;
}

content::WebContents* UnifiedSideSearchController::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params,
    base::OnceCallback<void(content::NavigationHandle&)>
        navigation_handle_callback) {
  auto* browser_view = GetBrowserView();
  return browser_view ? browser_view->browser()->OpenURL(
                            params, std::move(navigation_handle_callback))
                      : nullptr;
}

void UnifiedSideSearchController::SidePanelAvailabilityChanged(
    bool should_close) {
  if (should_close) {
    auto* registry = SidePanelRegistry::GetDeprecated(web_contents());
    if (registry && registry->GetEntryForKey(
                        SidePanelEntry::Key(SidePanelEntry::Id::kSideSearch))) {
      registry->Deregister(
          SidePanelEntry::Key(SidePanelEntry::Id::kSideSearch));
    }
  }
  UpdateSidePanel();
}

void UnifiedSideSearchController::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument() ||
      !navigation_handle->HasCommitted()) {
    return;
  }

  UpdateSidePanel();

  if (ShouldAutomaticallyTriggerAfterNavigation(navigation_handle)) {
    auto* tracker =
        feature_engagement::TrackerFactory::GetForBrowserContext(GetProfile());
    auto* side_panel_ui = GetSidePanelUI();
    auto* tab_contents_helper =
        SideSearchTabContentsHelper::FromWebContents(web_contents());

    if (!side_panel_ui || !tracker || !tab_contents_helper ||
        !tracker->ShouldTriggerHelpUI(
            feature_engagement::kIPHSideSearchAutoTriggeringFeature)) {
      return;
    }

    side_panel_ui->Show(
        SidePanelEntry::Id::kSideSearch,
        SidePanelUtil::SidePanelOpenTrigger::kIPHSideSearchAutoTrigger);
    tab_contents_helper->SetAutoTriggered(true);

    // Note that `Dismiss()` in this case does not dismiss the UI. It's telling
    // the FE backend that the promo is done so that other promos can run. The
    // side panel showing should not block other promos from displaying.
    tracker->Dismissed(feature_engagement::kIPHSideSearchAutoTriggeringFeature);
    tracker->NotifyEvent(feature_engagement::events::kSideSearchAutoTriggered);
  }
}

void UnifiedSideSearchController::OnEntryShown(SidePanelEntry* entry) {
  UpdateSidePanel();
  auto* active_contents = GetBrowserView()->GetActiveWebContents();
  if (active_contents) {
    auto* helper =
        SideSearchTabContentsHelper::FromWebContents(active_contents);
    if (helper)
      helper->MaybeRecordDurationSidePanelAvailableToFirstOpen();
  }
}

void UnifiedSideSearchController::OnEntryHidden(SidePanelEntry* entry) {
  UpdateSidePanel();
}

base::WeakPtr<UnifiedSideSearchController>
UnifiedSideSearchController::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

std::unique_ptr<views::View> UnifiedSideSearchController::GetSideSearchView() {
  auto* browser_view = GetBrowserView();
  DCHECK(browser_view);
  auto side_search_view =
      std::make_unique<SideSearchWebView>(browser_view->GetProfile());
  side_search_view->SetProperty(views::kElementIdentifierKey,
                                kSideSearchWebViewElementId);
  side_search_view->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded));
  side_search_view->SetBackground(
      views::CreateThemedSolidBackground(kColorToolbar));
  auto* tab_contents_helper =
      SideSearchTabContentsHelper::FromWebContents(web_contents());
  DCHECK(tab_contents_helper);
  side_search_view->SetWebContents(tab_contents_helper->GetSidePanelContents());
  return std::move(side_search_view);
}

ui::ImageModel UnifiedSideSearchController::GetSideSearchIcon() {
  const int icon_size = ChromeLayoutProvider::Get()->GetDistanceMetric(
      ChromeDistanceMetric::DISTANCE_SIDE_PANEL_HEADER_VECTOR_ICON_SIZE);
  auto* browser = chrome::FindBrowserWithTab(web_contents());
  auto icon_image =
      browser ? DefaultSearchIconSource::GetOrCreateForBrowser(browser)
                    ->GetSizedIconImage(icon_size)
              : ui::ImageModel();
  return icon_image.IsEmpty()
             ? ui::ImageModel::FromVectorIcon(vector_icons::kSearchIcon,
                                              ui::kColorIcon, icon_size)
             : std::move(icon_image);
}

std::u16string UnifiedSideSearchController::GetSideSearchName() const {
  auto* tab_contents_helper =
      SideSearchTabContentsHelper::FromWebContents(web_contents());
  if (!tab_contents_helper)
    return std::u16string();

  auto last_search_url = tab_contents_helper->last_search_url();
  return last_search_url
             ? url_formatter::
                   FormatUrlForDisplayOmitSchemePathAndTrivialSubdomains(
                       last_search_url.value())
             : std::u16string();
}

// To make SidePanelOpenTrigger::kContextMenuSearchOption records correctly,
// this function can only be called via tapping on menu search option.
void UnifiedSideSearchController::OpenSidePanel() {
  UpdateSidePanel();
  auto* side_panel_ui = GetSidePanelUI();
  if (side_panel_ui) {
    side_panel_ui->Show(
        SidePanelEntry::Id::kSideSearch,
        SidePanelUtil::SidePanelOpenTrigger::kContextMenuSearchOption);
  }
}

void UnifiedSideSearchController::CloseSidePanel() {
  auto* side_panel_ui = GetSidePanelUI();
  if (side_panel_ui) {
    side_panel_ui->Close();
  }
}

BrowserView* UnifiedSideSearchController::GetBrowserView() const {
  auto* browser = chrome::FindBrowserWithTab(web_contents());
  return browser ? BrowserView::GetBrowserViewForBrowser(browser) : nullptr;
}

Profile* UnifiedSideSearchController::GetProfile() const {
  return Profile::FromBrowserContext(web_contents()->GetBrowserContext());
}

SidePanelUI* UnifiedSideSearchController::GetSidePanelUI() {
  auto* browser = chrome::FindBrowserWithTab(web_contents());
  return browser ? browser->GetFeatures().side_panel_ui() : nullptr;
}

void UnifiedSideSearchController::UpdateSidePanel() {
  auto* tab_contents_helper =
      SideSearchTabContentsHelper::FromWebContents(web_contents());

  // Early return if the tab helper for the active tab is not defined
  // (crbug.com/1307908).
  if (!tab_contents_helper)
    return;

  UpdateSidePanelRegistry(
      tab_contents_helper->CanShowSidePanelForCommittedNavigation());

  auto* browser_view = GetBrowserView();
  // GetBrowserView() can return nullptr when the WebContents
  // is detached and about to move into another browser.
  if (browser_view) {
    browser_view->UpdatePageActionIcon(PageActionIconType::kSideSearch);
    browser_view->InvalidateLayout();
  }
}

void UnifiedSideSearchController::UpdateSidePanelRegistry(bool is_available) {
  auto* registry = SidePanelRegistry::GetDeprecated(web_contents());
  if (!registry)
    return;
  auto* current_entry = registry->GetEntryForKey(
      SidePanelEntry::Key(SidePanelEntry::Id::kSideSearch));
  if (!current_entry && is_available) {
    auto entry = std::make_unique<SidePanelEntry>(
        SidePanelEntry::Id::kSideSearch,
        base::BindRepeating(&UnifiedSideSearchController::GetSideSearchView,
                            base::Unretained(this)),
        base::BindRepeating(&UnifiedSideSearchController::GetOpenInNewTabURL,
                            base::Unretained(this)));
    entry->AddObserver(this);
    registry->Register(std::move(entry));
    RecordSideSearchAvailabilityChanged(
        SideSearchAvailabilityChangeType::kBecomeAvailable);
  }

  if (current_entry && !is_available) {
    current_entry->RemoveObserver(this);
    registry->Deregister(SidePanelEntry::Key(SidePanelEntry::Id::kSideSearch));
    RecordSideSearchAvailabilityChanged(
        SideSearchAvailabilityChangeType::kBecomeUnavailable);
  }
}

bool UnifiedSideSearchController::ShouldAutomaticallyTriggerAfterNavigation(
    content::NavigationHandle* navigation_handle) {
  // Only trigger the panel automatically if the current tab is the browser's
  // active tab (it may not necessarily be the active tab if navigation commit
  // happens after the user switches tabs).
  auto* browser_view = GetBrowserView();
  if (!browser_view || browser_view->GetActiveWebContents() != web_contents())
    return false;

  // If the side search side panel is already open we do not need to
  // automatically retrigger the panel.

  Browser* browser = chrome::FindBrowserWithTab(web_contents());
  if (side_search::IsSideSearchToggleOpen(browser)) {
    return false;
  }

  auto* tab_contents_helper =
      SideSearchTabContentsHelper::FromWebContents(web_contents());
  if (!tab_contents_helper)
    return false;

  const GURL& previously_committed_url =
      navigation_handle->GetPreviousPrimaryMainFrameURL();
  const bool is_renderer_initiated = navigation_handle->IsRendererInitiated();

  // How many times a user has to return to a given SRP before we automatically
  // trigger the side search side panel for that SRP on a subsequent navigation.
  constexpr int kAutoTriggeringReturnCount = 2;

  // Trigger the side panel only if we've returned to the same SRP n times and
  // this is the first navigation after navigating away from the Google SRP. We
  // also check to ensure the navigation is renderer initiated to avoid showing
  // the side panel if the user navigates the tab via the omnibox / bookmarks
  // etc.
  return is_renderer_initiated &&
         tab_contents_helper->returned_to_previous_srp_count() ==
             kAutoTriggeringReturnCount &&
         previously_committed_url == tab_contents_helper->last_search_url();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(UnifiedSideSearchController);
