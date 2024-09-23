// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_bubble_view_impl.h"

#include <string>

#include "base/metrics/user_metrics.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/accessibility/non_accessible_image_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_content_view.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

namespace {

constexpr int kMaxBubbleWidth = 1000;

}  // namespace

CookieControlsBubbleViewImpl::CookieControlsBubbleViewImpl(
    views::View* anchor_view,
    content::WebContents* web_contents,
    OnCloseBubbleCallback callback)
    : LocationBarBubbleDelegateView(anchor_view, web_contents,true),
      callback_(std::move(callback)) {
  SetShowCloseButton(true);
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  SetProperty(views::kElementIdentifierKey, kCookieControlsBubble);
  SetSubtitleAllowCharacterBreak(true);
}

CookieControlsBubbleViewImpl::~CookieControlsBubbleViewImpl() = default;

void CookieControlsBubbleViewImpl::Init() {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  auto* provider = ChromeLayoutProvider::Get();
  const int vertical_margin =
      provider->GetDistanceMetric(DISTANCE_CONTENT_LIST_VERTICAL_MULTI);
  set_margins(gfx::Insets::VH(vertical_margin, 0));
}

void CookieControlsBubbleViewImpl::InitContentView(
    std::unique_ptr<CookieControlsContentView> view) {
  CHECK(!content_view_);
  content_view_ = AddChildView(std::move(view));
  content_view_->SetProperty(views::kElementIdentifierKey, kContentView);
}

void CookieControlsBubbleViewImpl::InitReloadingView(
    std::unique_ptr<View> view) {
  CHECK(!reloading_view_);
  reloading_view_ = AddChildView(std::move(view));
  reloading_view_->SetProperty(views::kElementIdentifierKey, kReloadingView);
}

void CookieControlsBubbleViewImpl::UpdateTitle(const std::u16string& title) {
  SetTitle(title);
}

void CookieControlsBubbleViewImpl::UpdateSubtitle(
    const std::u16string& subtitle) {
  SetSubtitle(subtitle);
}

void CookieControlsBubbleViewImpl::UpdateFaviconImage(const gfx::Image& image,
                                                      int favicon_view_id) {
  auto* favicon =
      views::AsViewClass<NonAccessibleImageView>(GetViewByID(favicon_view_id));
  CHECK(favicon);
  favicon->SetImage(ui::ImageModel::FromImage(image));
}

void CookieControlsBubbleViewImpl::SwitchToReloadingView() {
  base::RecordAction(
      base::UserMetricsAction("CookieControls.Bubble.ReloadingShown"));
  GetReloadingView()->SetVisible(true);
  GetContentView()->SetVisible(false);
  InvalidateLayout();
}

CookieControlsContentView* CookieControlsBubbleViewImpl::GetContentView() {
  return content_view_;
}

views::View* CookieControlsBubbleViewImpl::GetReloadingView() {
  return reloading_view_;
}

void CookieControlsBubbleViewImpl::CloseWidget() {
  GetWidget()->Close();
}

base::CallbackListSubscription
CookieControlsBubbleViewImpl::RegisterOnUserClosedContentViewCallback(
    base::RepeatingClosureList::CallbackType callback) {
  return on_user_closed_content_view_callback_list_.Add(std::move(callback));
}

gfx::Size CookieControlsBubbleViewImpl::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  auto size =
      LocationBarBubbleDelegateView::CalculatePreferredSize(available_size);

  // Enforce a range of valid widths.
  auto* provider = ChromeLayoutProvider::Get();
  int width =
      std::clamp(size.width(),
                 provider->GetDistanceMetric(
                     views::DistanceMetric::DISTANCE_BUBBLE_PREFERRED_WIDTH),
                 kMaxBubbleWidth);
  return gfx::Size(width, size.height());
}

void CookieControlsBubbleViewImpl::CloseBubble() {
  if (GetWidget()->IsClosed()) {
    return;
  }
  std::move(callback_).Run(this);
  LocationBarBubbleDelegateView::CloseBubble();
}

bool CookieControlsBubbleViewImpl::OnCloseRequested(
    views::Widget::ClosedReason close_reason) {
  // Always respect an unspecified reason, which is usually the controller
  // closing the view.
  if (close_reason == views::Widget::ClosedReason::kUnspecified) {
    return true;
  }

  // Ignore focus loss while the reloading view is visible. The reloading view
  // will automatically close when the page has loaded.
  if (GetReloadingView()->GetVisible()) {
    return close_reason != views::Widget::ClosedReason::kLostFocus;
  }

  on_user_closed_content_view_callback_list_.Notify();
  return false;
}

BEGIN_METADATA(CookieControlsBubbleViewImpl)
END_METADATA
