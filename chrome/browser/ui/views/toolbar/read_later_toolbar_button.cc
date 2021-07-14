// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/read_later_toolbar_button.h"

#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/webview/webview.h"

namespace {

class ReadLaterSidePanelWebView : public views::WebView,
                                  public BubbleContentsWrapper::Host {
 public:
  ReadLaterSidePanelWebView(Profile* profile, base::RepeatingClosure close_cb)
      : close_cb_(std::move(close_cb)),
        contents_wrapper_(std::make_unique<BubbleContentsWrapperT<ReadLaterUI>>(
            GURL(chrome::kChromeUIReadLaterURL),
            profile,
            IDS_READ_LATER_TITLE,
            /*enable_extension_apis=*/true,
            /*webui_resizes_host=*/false)) {
    SetVisible(false);
    contents_wrapper_->SetHost(weak_factory_.GetWeakPtr());
    contents_wrapper_->ReloadWebContents();
    SetWebContents(contents_wrapper_->web_contents());
  }

  void SetVisible(bool visible) override {
    views::WebView::SetVisible(visible);
    base::RecordAction(
        base::UserMetricsAction(visible ? "SidePanel.Show" : "SidePanel.Hide"));
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
  void ShowUI() override { SetVisible(true); }
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

 private:
  base::RepeatingClosure close_cb_;
  std::unique_ptr<BubbleContentsWrapperT<ReadLaterUI>> contents_wrapper_;
  std::unique_ptr<views::MenuRunner> context_menu_runner_;
  std::unique_ptr<ui::MenuModel> context_menu_model_;
  base::WeakPtrFactory<ReadLaterSidePanelWebView> weak_factory_{this};
};

}  // namespace

ReadLaterToolbarButton::ReadLaterToolbarButton(Browser* browser)
    : ToolbarButton(base::BindRepeating(&ReadLaterToolbarButton::ButtonPressed,
                                        base::Unretained(this))),
      browser_(browser),
      contents_wrapper_(std::make_unique<BubbleContentsWrapperT<ReadLaterUI>>(
          GURL(chrome::kChromeUIReadLaterURL),
          browser_->profile(),
          IDS_READ_LATER_TITLE,
          true)) {
  contents_wrapper_->ReloadWebContents();

  SetVectorIcons(kSidePanelIcon, kSidePanelTouchIcon);
  SetTooltipText(l10n_util::GetStringUTF16(IDS_TOOLTIP_SIDE_PANEL_SHOW));
  button_controller()->set_notify_action(
      views::ButtonController::NotifyAction::kOnPress);
  GetViewAccessibility().OverrideHasPopup(ax::mojom::HasPopup::kMenu);
}

ReadLaterToolbarButton::~ReadLaterToolbarButton() = default;

void ReadLaterToolbarButton::ButtonPressed() {
  BrowserView* const browser_view =
      BrowserView::GetBrowserViewForBrowser(browser_);
  DCHECK(browser_view->right_aligned_side_panel());

  if (!side_panel_webview_) {
    auto webview = std::make_unique<ReadLaterSidePanelWebView>(
        browser_->profile(),
        base::BindRepeating(&ReadLaterToolbarButton::ButtonPressed,
                            base::Unretained(this)));
    side_panel_webview_ =
        browser_view->right_aligned_side_panel()->AddChildView(
            std::move(webview));
    SetTooltipText(l10n_util::GetStringUTF16(IDS_TOOLTIP_SIDE_PANEL_HIDE));
  } else {
    browser_view->right_aligned_side_panel()->RemoveChildViewT(
        side_panel_webview_);
    side_panel_webview_ = nullptr;
    SetTooltipText(l10n_util::GetStringUTF16(IDS_TOOLTIP_SIDE_PANEL_SHOW));
  }
}
