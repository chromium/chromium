// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tab_search/tab_search_bubble_view.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "chrome/browser/ui/webui/tab_search/tab_search_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

namespace {

// The min / max size available to the TabSearchBubbleView.
// These are arbitrary sizes that match those set by ExtensionPopup.
// TODO(tluk): Determine the correct size constraints for the
// TabSearchBubbleView.
constexpr gfx::Size kMinSize(25, 25);
constexpr gfx::Size kMaxSize(800, 600);

class TabSearchWebView : public views::WebView {
 public:
  TabSearchWebView(content::BrowserContext* browser_context,
                   TabSearchBubbleView* parent)
      : WebView(browser_context), parent_(parent) {}

  ~TabSearchWebView() override = default;

  // views::WebView:
  void PreferredSizeChanged() override {
    WebView::PreferredSizeChanged();
    parent_->OnWebViewSizeChanged();
  }

  // content::WebContentsDelegate:
  bool HandleContextMenu(content::RenderFrameHost* render_frame_host,
                         const content::ContextMenuParams& params) override {
    // Ignores context menu.
    return true;
  }

 private:
  TabSearchBubbleView* parent_;
};

}  // namespace

// static.
views::Widget* TabSearchBubbleView::CreateTabSearchBubble(
    content::BrowserContext* browser_context,
    views::View* anchor_view) {
  return BubbleDialogDelegateView::CreateBubble(
      std::make_unique<TabSearchBubbleView>(browser_context, anchor_view));
}

TabSearchBubbleView::TabSearchBubbleView(
    content::BrowserContext* browser_context,
    views::View* anchor_view)
    : BubbleDialogDelegateView(anchor_view, views::BubbleBorder::TOP_RIGHT),
      web_view_(AddChildView(
          std::make_unique<TabSearchWebView>(browser_context, this))) {
  SetButtons(ui::DIALOG_BUTTON_NONE);
  set_margins(gfx::Insets());

  SetLayoutManager(std::make_unique<views::FillLayout>());
  // Required for intercepting extension function calls when the page is loaded
  // in a bubble (not a full tab, thus tab helpers are not registered
  // automatically).
  extensions::ChromeExtensionWebContentsObserver::CreateForWebContents(
      web_view_->GetWebContents());
  web_view_->EnableSizingFromWebContents(kMinSize, kMaxSize);
  web_view_->LoadInitialURL(GURL(chrome::kChromeUITabSearchURL));

  TabSearchUI* const tab_search_ui = static_cast<TabSearchUI*>(
      web_view_->GetWebContents()->GetWebUI()->GetController());
  // Depends on the TabSearchUI object being constructed synchronously when the
  // navigation is started in LoadInitialURL().
  tab_search_ui->SetEmbedder(this);
}

TabSearchBubbleView::~TabSearchBubbleView() {
  if (timer_.has_value()) {
    UmaHistogramMediumTimes("Tabs.TabSearch.WindowDisplayedDuration2",
                            timer_->Elapsed());
  }
}

gfx::Size TabSearchBubbleView::CalculatePreferredSize() const {
  // Constrain the size to popup min/max.
  gfx::Size preferred_size = views::View::CalculatePreferredSize();
  preferred_size.SetToMax(kMinSize);
  preferred_size.SetToMin(kMaxSize);
  return preferred_size;
}

void TabSearchBubbleView::AddedToWidget() {
  BubbleDialogDelegateView::AddedToWidget();
  observed_bubble_widget_.Add(GetWidget());
  web_view_->holder()->SetCornerRadii(gfx::RoundedCornersF(GetCornerRadius()));
}

void TabSearchBubbleView::ShowBubble() {
  DCHECK(GetWidget());
  GetWidget()->Show();
  web_view_->GetWebContents()->Focus();
  timer_ = base::ElapsedTimer();
}

void TabSearchBubbleView::CloseBubble() {
  DCHECK(GetWidget());
  GetWidget()->CloseWithReason(views::Widget::ClosedReason::kEscKeyPressed);
}

void TabSearchBubbleView::OnWidgetClosing(views::Widget* widget) {
  if (widget == GetWidget()) {
    TabSearchUI* const tab_search_ui = static_cast<TabSearchUI*>(
        web_view_->GetWebContents()->GetWebUI()->GetController());
    tab_search_ui->SetEmbedder(nullptr);
  }
}

void TabSearchBubbleView::OnWebViewSizeChanged() {
  SizeToContents();
}
