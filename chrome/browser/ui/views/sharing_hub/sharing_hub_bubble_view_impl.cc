// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sharing_hub/sharing_hub_bubble_view_impl.h"

#include "chrome/browser/ui/sharing_hub/sharing_hub_bubble_controller.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

namespace sharing_hub {

SharingHubBubbleViewImpl::SharingHubBubbleViewImpl(
    views::View* anchor_view,
    content::WebContents* web_contents,
    SharingHubBubbleController* controller)
    : LocationBarBubbleDelegateView(anchor_view, web_contents),
      controller_(controller) {
  SetButtons(ui::DIALOG_BUTTON_NONE);
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));
  DCHECK(controller);
}

SharingHubBubbleViewImpl::~SharingHubBubbleViewImpl() = default;

void SharingHubBubbleViewImpl::Hide() {
  if (controller_) {
    controller_->OnBubbleClosed();
    controller_ = nullptr;
  }
  CloseBubble();
}

bool SharingHubBubbleViewImpl::ShouldShowCloseButton() const {
  return true;
}

std::u16string SharingHubBubbleViewImpl::GetWindowTitle() const {
  return controller_->GetWindowTitle();
}

void SharingHubBubbleViewImpl::WindowClosing() {
  if (controller_) {
    controller_->OnBubbleClosed();
    controller_ = nullptr;
  }
}

void SharingHubBubbleViewImpl::OnPaint(gfx::Canvas* canvas) {
  views::BubbleDialogDelegateView::OnPaint(canvas);
}

void SharingHubBubbleViewImpl::Show(DisplayReason reason) {
  ShowForReason(reason);
}

const views::View* SharingHubBubbleViewImpl::GetButtonContainerForTesting()
    const {
  return scroll_view_->contents();
}

void SharingHubBubbleViewImpl::Init() {
  auto* provider = ChromeLayoutProvider::Get();
  set_margins(
      gfx::Insets(provider->GetDistanceMetric(
                      views::DISTANCE_DIALOG_CONTENT_MARGIN_TOP_CONTROL),
                  0,
                  provider->GetDistanceMetric(
                      views::DISTANCE_DIALOG_CONTENT_MARGIN_BOTTOM_CONTROL),
                  0));
  SetLayoutManager(std::make_unique<views::FillLayout>());

  scroll_view_ = AddChildView(std::make_unique<views::ScrollView>());

  PopulateScrollView();
}

void SharingHubBubbleViewImpl::PopulateScrollView() {
  auto* action_list_view =
      scroll_view_->SetContents(std::make_unique<views::View>());
  action_list_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  MaybeSizeToContents();
  Layout();
}

void SharingHubBubbleViewImpl::MaybeSizeToContents() {
  // The widget may be null if this is called while the dialog is opening.
  if (GetWidget())
    SizeToContents();
}

}  // namespace sharing_hub
