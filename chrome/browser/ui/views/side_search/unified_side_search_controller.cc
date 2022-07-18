// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_search/unified_side_search_controller.h"

#include "base/bind.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/side_search/side_search_utils.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "components/url_formatter/elide_url.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/navigation_handle.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/flex_layout_view.h"

UnifiedSideSearchController::UnifiedSideSearchController(
    content::WebContents* web_contents)
    : content::WebContentsUserData<UnifiedSideSearchController>(*web_contents) {
  Observe(web_contents);

  // Update the state of the side panel to catch cases where we switch to a tab
  // where the panel should be hidden (or vise versa).
  UpdateSidePanel();
}

UnifiedSideSearchController::~UnifiedSideSearchController() {
  Observe(nullptr);
}

bool UnifiedSideSearchController::HandleKeyboardEvent(
    content::WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  auto* browser_view = GetBrowserView();
  return browser_view ? unhandled_keyboard_event_handler_.HandleKeyboardEvent(
                            event, browser_view->GetFocusManager())
                      : false;
}

content::WebContents* UnifiedSideSearchController::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params) {
  auto* browser_view = GetBrowserView();
  return browser_view ? browser_view->browser()->OpenURL(params) : nullptr;
}

void UnifiedSideSearchController::SidePanelAvailabilityChanged(
    bool should_close) {
  if (should_close) {
    CloseSidePanel();
  } else {
    UpdateSidePanel();
  }
}

void UnifiedSideSearchController::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument() ||
      !navigation_handle->HasCommitted()) {
    return;
  }

  UpdateSidePanel();
}

void UnifiedSideSearchController::OnEntryShown(SidePanelEntry* entry) {
  UpdateSidePanel();
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
      std::make_unique<views::WebView>(browser_view->GetProfile());
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
  auto* browser = chrome::FindBrowserWithWebContents(web_contents());
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

void UnifiedSideSearchController::OpenSidePanel() {
  auto* browser_view = GetBrowserView();
  if (browser_view) {
    browser_view->side_panel_coordinator()->Show(
        SidePanelEntry::Id::kSideSearch);
    auto* active_contents = browser_view->GetActiveWebContents();
    if (active_contents) {
      auto* helper =
          SideSearchTabContentsHelper::FromWebContents(active_contents);
      if (helper)
        helper->MaybeRecordDurationSidePanelAvailableToFirstOpen();
    }
  }
  UpdateSidePanel();
}

void UnifiedSideSearchController::CloseSidePanel(
    absl::optional<SideSearchCloseActionType> action) {
  auto* browser_view = GetBrowserView();
  if (browser_view) {
    browser_view->side_panel_coordinator()->Close();
  }
  UpdateSidePanel();

  // Clear the side contents for the currently active tab.
  ClearSideContentsCache();
}

BrowserView* UnifiedSideSearchController::GetBrowserView() const {
  auto* browser = chrome::FindBrowserWithWebContents(web_contents());
  return browser ? BrowserView::GetBrowserViewForBrowser(browser) : nullptr;
}

void UnifiedSideSearchController::ClearSideContentsCache() {
  auto* browser_view = GetBrowserView();
  if (!browser_view)
    return;
  DCHECK(!browser_view->side_panel_coordinator()->IsSidePanelShowing());
  auto* registry = SidePanelRegistry::Get(web_contents());
  if (!registry)
    return;
  auto* current_entry =
      registry->GetEntryForId(SidePanelEntry::Id::kSideSearch);
  if (current_entry)
    current_entry->ClearCachedView();

  auto* helper = SideSearchTabContentsHelper::FromWebContents(web_contents());
  if (helper)
    helper->ClearSidePanelContents();
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
  auto* registry = SidePanelRegistry::Get(web_contents());
  if (!registry)
    return;
  auto* current_entry =
      registry->GetEntryForId(SidePanelEntry::Id::kSideSearch);
  if (!current_entry && is_available) {
    auto entry = std::make_unique<SidePanelEntry>(
        SidePanelEntry::Id::kSideSearch, GetSideSearchName(),
        GetSideSearchIcon(),
        base::BindRepeating(&UnifiedSideSearchController::GetSideSearchView,
                            base::Unretained(this)));
    entry->AddObserver(this);
    registry->Register(std::move(entry));
    RecordSideSearchAvailabilityChanged(
        SideSearchAvailabilityChangeType::kBecomeAvailable);
  }

  if (current_entry && !is_available) {
    current_entry->RemoveObserver(this);
    registry->Deregister(SidePanelEntry::Id::kSideSearch);
    RecordSideSearchAvailabilityChanged(
        SideSearchAvailabilityChangeType::kBecomeUnavailable);
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(UnifiedSideSearchController);
