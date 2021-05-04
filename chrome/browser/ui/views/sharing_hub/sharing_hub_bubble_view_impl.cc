// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sharing_hub/sharing_hub_bubble_view_impl.h"

#include "chrome/browser/ui/sharing_hub/sharing_hub_bubble_controller.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "ui/views/controls/scroll_view.h"
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
  SetLayoutManager(std::make_unique<views::FillLayout>());

  CreateScrollView();
}

void SharingHubBubbleViewImpl::CreateScrollView() {
  scroll_view_ = AddChildView(std::make_unique<views::ScrollView>());
}

}  // namespace sharing_hub
