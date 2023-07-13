// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_bubble_view.h"

#include <string>
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/accessibility/non_accessible_image_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_content_view.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(CookieControlsBubbleView,
                                      kCookieControlsBubble);

CookieControlsBubbleView::CookieControlsBubbleView(
    views::View* anchor_view,
    content::WebContents* web_contents,
    OnCloseBubbleCallback callback)
    : LocationBarBubbleDelegateView(anchor_view, web_contents),
      callback_(std::move(callback)) {
  SetShowCloseButton(true);
  SetButtons(ui::DIALOG_BUTTON_NONE);
  SetProperty(views::kElementIdentifierKey, kCookieControlsBubble);
}

CookieControlsBubbleView::~CookieControlsBubbleView() = default;

void CookieControlsBubbleView::Init() {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  auto* provider = ChromeLayoutProvider::Get();
  const int vertical_margin =
      provider->GetDistanceMetric(DISTANCE_CONTENT_LIST_VERTICAL_MULTI);
  set_margins(gfx::Insets::VH(vertical_margin, 0));
  set_fixed_width(provider->GetDistanceMetric(
      views::DistanceMetric::DISTANCE_BUBBLE_PREFERRED_WIDTH));
}

void CookieControlsBubbleView::InitContentView(
    std::unique_ptr<CookieControlsContentView> view) {
  CHECK(!content_view_);
  content_view_ = AddChildView(std::move(view));
}

void CookieControlsBubbleView::InitReloadingView(std::unique_ptr<View> view) {
  CHECK(!reloading_view_);
  reloading_view_ = AddChildView(std::move(view));
}

void CookieControlsBubbleView::UpdateTitle(const std::u16string& title) {
  SetTitle(title);
  SizeToContents();
}

void CookieControlsBubbleView::UpdateSubtitle(const std::u16string& subtitle) {
  SetSubtitle(subtitle);
}

void CookieControlsBubbleView::UpdateFaviconImage(const gfx::Image& image,
                                                  int favicon_view_id) {
  auto* favicon =
      views::AsViewClass<NonAccessibleImageView>(GetViewByID(favicon_view_id));
  CHECK(favicon);
  favicon->SetImage(ui::ImageModel::FromImage(image));
}

void CookieControlsBubbleView::ShowContentView() {
  reloading_view()->SetVisible(false);
  content_view()->SetVisible(true);
}

void CookieControlsBubbleView::ShowReloadingView() {
  reloading_view()->SetVisible(true);
  content_view()->SetVisible(false);
}

void CookieControlsBubbleView::ChildPreferredSizeChanged(views::View* child) {
  SizeToContents();
}

void CookieControlsBubbleView::CloseBubble() {
  if (GetWidget()->IsClosed()) {
    return;
  }
  std::move(callback_).Run(this);
  LocationBarBubbleDelegateView::CloseBubble();
}
