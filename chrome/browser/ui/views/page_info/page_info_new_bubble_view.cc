// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/page_info_new_bubble_view.h"

#include "chrome/browser/ui/page_info/chrome_page_info_delegate.h"
#include "chrome/browser/ui/page_info/chrome_page_info_ui_delegate.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/page_info/page_info_main_view.h"
#include "chrome/browser/ui/views/page_info/page_info_security_content_view.h"
#include "chrome/browser/ui/views/page_info/page_info_view_factory.h"
#include "chrome/browser/ui/views/page_info/page_switcher_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"

PageInfoNewBubbleView::PageInfoNewBubbleView(
    views::View* anchor_view,
    const gfx::Rect& anchor_rect,
    gfx::NativeView parent_window,
    content::WebContents* associated_web_contents,
    const GURL& url,
    PageInfoClosingCallback closing_callback)
    : PageInfoBubbleViewBase(anchor_view,
                             anchor_rect,
                             parent_window,
                             PageInfoBubbleViewBase::BUBBLE_PAGE_INFO,
                             associated_web_contents),
      closing_callback_(std::move(closing_callback)) {
  DCHECK(closing_callback_);
  DCHECK(web_contents());

  SetShowTitle(false);
  SetShowCloseButton(false);

  ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();

  // In Harmony, the last view is a HoverButton, which overrides the bottom
  // dialog inset in favor of its own. Note the multi-button value is used here
  // assuming that the "Cookies" & "Site settings" buttons will always be shown.
  const int bottom_margin =
      layout_provider->GetDistanceMetric(DISTANCE_CONTENT_LIST_VERTICAL_MULTI);
  const int top_margin =
      layout_provider->GetInsetsMetric(views::INSETS_DIALOG).top();
  set_margins(gfx::Insets(top_margin, 0, bottom_margin, 0));
  ui_delegate_ =
      std::make_unique<ChromePageInfoUiDelegate>(web_contents(), url);
  presenter_ = std::make_unique<PageInfo>(
      std::make_unique<ChromePageInfoDelegate>(web_contents()), web_contents(),
      url);
  view_factory_ = std::make_unique<PageInfoViewFactory>(
      presenter_.get(), ui_delegate_.get(), this);

  SetTitle(presenter_->GetSimpleSiteName());

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  page_container_ = AddChildView(
      std::make_unique<PageSwitcherView>(view_factory_->CreateMainPageView()));

  views::BubbleDialogDelegateView::CreateBubble(this);
}

PageInfoNewBubbleView::~PageInfoNewBubbleView() = default;

void PageInfoNewBubbleView::OpenMainPage() {
  page_container_->SwitchToPage(view_factory_->CreateMainPageView());
}

void PageInfoNewBubbleView::OpenSecurityPage() {
  presenter_->RecordPageInfoAction(
      PageInfo::PageInfoAction::PAGE_INFO_SECURITY_DETAILS_OPENED);
  page_container_->SwitchToPage(view_factory_->CreateSecurityPageView());
}

void PageInfoNewBubbleView::OpenPermissionPage(ContentSettingsType type) {
  presenter_->RecordPageInfoAction(
      PageInfo::PageInfoAction::PAGE_INFO_PERMISSION_DIALOG_OPENED);
  page_container_->SwitchToPage(view_factory_->CreatePermissionPageView(type));
}

void PageInfoNewBubbleView::CloseBubble() {
  GetWidget()->CloseWithReason(
      views::Widget::ClosedReason::kCloseButtonClicked);
}

void PageInfoNewBubbleView::DidChangeVisibleSecurityState() {
  presenter_->UpdateSecurityState();
}

void PageInfoNewBubbleView::OnWidgetDestroying(views::Widget* widget) {
  PageInfoBubbleViewBase::OnWidgetDestroying(widget);

  // This method mostly shouldn't be re-entrant but there are a few cases where
  // it can be (see crbug/966308). In that case, we have already run the closing
  // callback so should not attempt to do it again. As there will always be a
  // |closing_callback_|, this is also used to ensure that the |presenter_| is
  // informed exactly once.
  if (closing_callback_) {
    bool reload_prompt;
    presenter_->OnUIClosing(&reload_prompt);

    std::move(closing_callback_).Run(widget->closed_reason(), reload_prompt);
  }
}

void PageInfoNewBubbleView::WebContentsDestroyed() {
  weak_factory_.InvalidateWeakPtrs();
}

gfx::Size PageInfoNewBubbleView::CalculatePreferredSize() const {
  if (page_container_ == nullptr) {
    return views::View::CalculatePreferredSize();
  }

  int width = PageInfoViewFactory::kMinBubbleWidth;
  if (page_container_) {
    width = std::max(width, page_container_->GetPreferredSize().width());
    width = std::min(width, PageInfoViewFactory::kMaxBubbleWidth);
  }
  return gfx::Size(width, views::View::GetHeightForWidth(width));
}

void PageInfoNewBubbleView::ChildPreferredSizeChanged(views::View* child) {
  Layout();
  SizeToContents();
}
