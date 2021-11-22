// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/read_later_side_panel_web_view.h"

#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/browser/extensions/api/bookmark_manager_private/bookmark_manager_private_api.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/user_education/feature_promo_controller_views.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "ui/views/controls/menu/menu_runner.h"

ReadLaterSidePanelWebView::ReadLaterSidePanelWebView(
    Browser* browser,
    base::RepeatingClosure close_cb)
    : browser_(browser),
      close_cb_(std::move(close_cb)),
      contents_wrapper_(std::make_unique<BubbleContentsWrapperT<ReadLaterUI>>(
          GURL(chrome::kChromeUIReadLaterURL),
          browser->profile(),
          IDS_READ_LATER_TITLE,
          /*webui_resizes_host=*/false,
          /*esc_closes_ui=*/false)) {
  SetVisible(false);
  contents_wrapper_->SetHost(weak_factory_.GetWeakPtr());
  contents_wrapper_->ReloadWebContents();
  SetWebContents(contents_wrapper_->web_contents());
  set_allow_accelerators(true);

  if (base::FeatureList::IsEnabled(features::kSidePanelDragAndDrop)) {
    extensions::BookmarkManagerPrivateDragEventRouter::CreateForWebContents(
        contents_wrapper_->web_contents());
  }

  browser_->tab_strip_model()->AddObserver(this);
}

void ReadLaterSidePanelWebView::SetVisible(bool visible) {
  views::WebView::SetVisible(visible);
  base::RecordAction(
      base::UserMetricsAction(visible ? "SidePanel.Show" : "SidePanel.Hide"));
  if (visible) {
    // Record usage for side panel promo.
    feature_engagement::TrackerFactory::GetForBrowserContext(
        browser_->profile())
        ->NotifyEvent("side_panel_shown");

    // Close IPH for side panel if shown.
    FeaturePromoControllerViews* const feature_promo_controller =
        BrowserView::GetBrowserViewForBrowser(browser_)
            ->feature_promo_controller();
    feature_promo_controller->CloseBubble(
        feature_engagement::kIPHReadingListInSidePanelFeature);
  }
}

ReadLaterSidePanelWebView::~ReadLaterSidePanelWebView() = default;

void ReadLaterSidePanelWebView::ViewHierarchyChanged(
    const views::ViewHierarchyChangedDetails& details) {
  WebView::ViewHierarchyChanged(details);
  // Ensure the WebContents is in a visible state after being added to the
  // side panel so the correct lifecycle hooks are triggered.
  if (details.is_add && details.child == this)
    contents_wrapper_->web_contents()->WasShown();
}

void ReadLaterSidePanelWebView::ShowUI() {
  SetVisible(true);
  RequestFocus();
  UpdateActiveURL(browser_->tab_strip_model()->GetActiveWebContents());
}

void ReadLaterSidePanelWebView::CloseUI() {
  close_cb_.Run();
}

void ReadLaterSidePanelWebView::ShowCustomContextMenu(
    gfx::Point point,
    std::unique_ptr<ui::MenuModel> menu_model) {
  ConvertPointToScreen(this, &point);
  context_menu_model_ = std::move(menu_model);
  context_menu_runner_ = std::make_unique<views::MenuRunner>(
      context_menu_model_.get(),
      views::MenuRunner::HAS_MNEMONICS | views::MenuRunner::CONTEXT_MENU);
  context_menu_runner_->RunMenuAt(
      GetWidget(), nullptr, gfx::Rect(point, gfx::Size()),
      views::MenuAnchorPosition::kTopLeft, ui::MENU_SOURCE_MOUSE,
      contents_wrapper_->web_contents()->GetContentNativeView());
}

void ReadLaterSidePanelWebView::HideCustomContextMenu() {
  if (context_menu_runner_)
    context_menu_runner_->Cancel();
}

bool ReadLaterSidePanelWebView::HandleKeyboardEvent(
    content::WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  return unhandled_keyboard_event_handler_.HandleKeyboardEvent(
      event, GetFocusManager());
}

void ReadLaterSidePanelWebView::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (GetVisible() && selection.active_tab_changed())
    UpdateActiveURL(tab_strip_model->GetActiveWebContents());
}

void ReadLaterSidePanelWebView::TabChangedAt(content::WebContents* contents,
                                             int index,
                                             TabChangeType change_type) {
  if (GetVisible() && index == browser_->tab_strip_model()->active_index() &&
      change_type == TabChangeType::kAll) {
    UpdateActiveURL(browser_->tab_strip_model()->GetWebContentsAt(index));
  }
}

void ReadLaterSidePanelWebView::UpdateActiveURL(
    content::WebContents* contents) {
  auto* controller = contents_wrapper_->GetWebUIController();
  if (!controller || !contents)
    return;

  controller->GetAs<ReadLaterUI>()->SetActiveTabURL(
      chrome::GetURLToBookmark(contents));
}
