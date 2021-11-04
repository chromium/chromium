// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/read_later_toolbar_button.h"

#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/scoped_observation.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/extensions/api/bookmark_manager_private/bookmark_manager_private_api.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/read_later/reading_list_model_factory.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/chrome_view_class_properties.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel.h"
#include "chrome/browser/ui/views/user_education/feature_promo_controller_views.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/controls/dot_indicator.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/webview/webview.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/browser/ui/views/lens/lens_side_panel_controller.h"
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

namespace {

class ReadLaterSidePanelWebView : public views::WebView,
                                  public TabStripModelObserver,
                                  public BubbleContentsWrapper::Host {
 public:
  ReadLaterSidePanelWebView(Browser* browser, base::RepeatingClosure close_cb)
      : browser_(browser),
        close_cb_(std::move(close_cb)),
        contents_wrapper_(std::make_unique<BubbleContentsWrapperT<ReadLaterUI>>(
            GURL(chrome::kChromeUIReadLaterURL),
            browser->profile(),
            IDS_READ_LATER_TITLE,
            /*enable_extension_apis=*/true,
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

  void SetVisible(bool visible) override {
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

  void ViewHierarchyChanged(
      const views::ViewHierarchyChangedDetails& details) override {
    WebView::ViewHierarchyChanged(details);
    // Ensure the WebContents is in a visible state after being added to the
    // side panel so the correct lifecycle hooks are triggered.
    if (details.is_add && details.child == this)
      contents_wrapper_->web_contents()->WasShown();
  }

  // BubbleContentsWrapper::Host:
  void ShowUI() override {
    SetVisible(true);
    RequestFocus();
    UpdateActiveURL(browser_->tab_strip_model()->GetActiveWebContents());
  }
  void CloseUI() override { close_cb_.Run(); }
  void ShowCustomContextMenu(
      gfx::Point point,
      std::unique_ptr<ui::MenuModel> menu_model) override {
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
  void HideCustomContextMenu() override {
    if (context_menu_runner_)
      context_menu_runner_->Cancel();
  }
  bool HandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) override {
    return unhandled_keyboard_event_handler_.HandleKeyboardEvent(
        event, GetFocusManager());
  }

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override {
    if (GetVisible() && selection.active_tab_changed())
      UpdateActiveURL(tab_strip_model->GetActiveWebContents());
  }

  void TabChangedAt(content::WebContents* contents,
                    int index,
                    TabChangeType change_type) override {
    if (GetVisible() && index == browser_->tab_strip_model()->active_index() &&
        change_type == TabChangeType::kAll) {
      UpdateActiveURL(browser_->tab_strip_model()->GetWebContentsAt(index));
    }
  }

 private:
  void UpdateActiveURL(content::WebContents* contents) {
    auto* controller = contents_wrapper_->GetWebUIController();
    if (!controller || !contents)
      return;

    controller->GetAs<ReadLaterUI>()->SetActiveTabURL(
        chrome::GetURLToBookmark(contents));
  }

  Browser* const browser_;
  base::RepeatingClosure close_cb_;
  std::unique_ptr<BubbleContentsWrapperT<ReadLaterUI>> contents_wrapper_;
  std::unique_ptr<views::MenuRunner> context_menu_runner_;
  std::unique_ptr<ui::MenuModel> context_menu_model_;
  // A handler to handle unhandled keyboard messages coming back from the
  // renderer process.
  views::UnhandledKeyboardEventHandler unhandled_keyboard_event_handler_;
  base::WeakPtrFactory<ReadLaterSidePanelWebView> weak_factory_{this};
};

}  // namespace

ReadLaterToolbarButton::ReadLaterToolbarButton(Browser* browser)
    : ToolbarButton(base::BindRepeating(&ReadLaterToolbarButton::ButtonPressed,
                                        base::Unretained(this))),
      browser_(browser),
      dot_indicator_(views::DotIndicator::Install(image())),
      dot_bounds_updater_(
          std::make_unique<DotBoundsUpdater>(dot_indicator_, image())),
      reading_list_model_(
          ReadingListModelFactory::GetForBrowserContext(browser_->profile())) {
  SetVectorIcons(kSidePanelIcon, kSidePanelTouchIcon);
  SetTooltipText(l10n_util::GetStringUTF16(IDS_TOOLTIP_SIDE_PANEL_SHOW));
  button_controller()->set_notify_action(
      views::ButtonController::NotifyAction::kOnPress);
  GetViewAccessibility().OverrideHasPopup(ax::mojom::HasPopup::kMenu);

  if (reading_list_model_)
    reading_list_model_scoped_observation_.Observe(reading_list_model_);
}

ReadLaterToolbarButton::~ReadLaterToolbarButton() = default;

ReadLaterToolbarButton::DotBoundsUpdater::DotBoundsUpdater(
    views::DotIndicator* dot_indicator,
    views::ImageView* image)
    : dot_indicator_(dot_indicator), image_(image) {
  observation_.Observe(image);
}

ReadLaterToolbarButton::DotBoundsUpdater::~DotBoundsUpdater() = default;

void ReadLaterToolbarButton::DotBoundsUpdater::OnViewBoundsChanged(
    View* observed_view) {
  gfx::Rect dot_rect(8, 8);
  if (ui::TouchUiController::Get()->touch_ui()) {
    dot_rect = ScaleToEnclosingRect(
        dot_rect, float{kDefaultTouchableIconSize} / kDefaultIconSize);
  }
  dot_rect.set_origin(image_->GetImageBounds().bottom_right() -
                      dot_rect.bottom_right().OffsetFromOrigin());
  dot_indicator_->SetBoundsRect(dot_rect);
}

void ReadLaterToolbarButton::ReadingListModelLoaded(
    const ReadingListModel* model) {
  if (model->unseen_size())
    dot_indicator_->Show();
}

void ReadLaterToolbarButton::ReadingListModelBeingDeleted(
    const ReadingListModel* model) {
  DCHECK(model == reading_list_model_);
  DCHECK(reading_list_model_scoped_observation_.IsObservingSource(
      reading_list_model_));
  reading_list_model_scoped_observation_.Reset();
}

void ReadLaterToolbarButton::ReadingListDidApplyChanges(
    ReadingListModel* model) {
  if (!side_panel_webview_ && reading_list_model_->unseen_size() > 0) {
    dot_indicator_->Show();
  } else {
    dot_indicator_->Hide();
  }
}

void ReadLaterToolbarButton::ButtonPressed() {
  BrowserView* const browser_view =
      BrowserView::GetBrowserViewForBrowser(browser_);
  DCHECK(browser_view->right_aligned_side_panel());

  if (!side_panel_webview_) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    // Hide the Lens side panel if it's showing instead.
    lens::LensSidePanelController* const lens_side_panel_controller =
        browser_view->lens_side_panel_controller();
    if (lens_side_panel_controller && lens_side_panel_controller->IsShowing()) {
      lens_side_panel_controller->Close();
      return;
    }
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

    // Using base::Unretained(this) is safe here because the side panel (and the
    // web view as its child) will be destroyed before the toolbar which will
    // destroy the ReadLaterToolbarButton.
    auto webview = std::make_unique<ReadLaterSidePanelWebView>(
        browser_, base::BindRepeating(&ReadLaterToolbarButton::ButtonPressed,
                                      base::Unretained(this)));
    side_panel_webview_ =
        browser_view->right_aligned_side_panel()->AddChildView(
            std::move(webview));
    SetTooltipText(l10n_util::GetStringUTF16(IDS_TOOLTIP_SIDE_PANEL_HIDE));
    reading_list_model_->MarkAllSeen();
    dot_indicator_->Hide();
  } else {
    HideSidePanel();
  }
}

void ReadLaterToolbarButton::HideSidePanel() {
  BrowserView* const browser_view =
      BrowserView::GetBrowserViewForBrowser(browser_);
  DCHECK(browser_view->right_aligned_side_panel());
  if (side_panel_webview_) {
    browser_view->right_aligned_side_panel()->RemoveChildViewT(
        side_panel_webview_);
    side_panel_webview_ = nullptr;
    SetTooltipText(l10n_util::GetStringUTF16(IDS_TOOLTIP_SIDE_PANEL_SHOW));
  }
}

bool ReadLaterToolbarButton::ShouldShowInkdropAfterIphInteraction() {
  return false;
}
