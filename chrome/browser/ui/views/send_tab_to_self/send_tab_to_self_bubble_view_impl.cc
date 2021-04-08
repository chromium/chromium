// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_bubble_view_impl.h"

#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_bubble_controller.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_bubble_device_button.h"
#include "chrome/grit/generated_resources.h"
#include "components/send_tab_to_self/target_device_info.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

namespace send_tab_to_self {

namespace {

// The valid device button height.
constexpr int kDeviceButtonHeight = 56;
// Maximum number of buttons that are shown without scroll. If the device
// number is larger than kMaximumButtons, the bubble content will be
// scrollable.
constexpr int kMaximumButtons = 5;

// Used to group the device buttons together, which makes moving between them
// with arrow keys possible.
constexpr int kDeviceButtonGroup = 0;

}  // namespace

SendTabToSelfBubbleViewImpl::SendTabToSelfBubbleViewImpl(
    views::View* anchor_view,
    content::WebContents* web_contents,
    SendTabToSelfBubbleController* controller)
    : LocationBarBubbleDelegateView(anchor_view, web_contents),
      controller_(controller) {
  SetButtons(ui::DIALOG_BUTTON_NONE);
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));
  DCHECK(controller);
}

SendTabToSelfBubbleViewImpl::~SendTabToSelfBubbleViewImpl() {}

void SendTabToSelfBubbleViewImpl::Hide() {
  if (controller_) {
    controller_->OnBubbleClosed();
    controller_ = nullptr;
  }
  CloseBubble();
}

bool SendTabToSelfBubbleViewImpl::ShouldShowCloseButton() const {
  return true;
}

std::u16string SendTabToSelfBubbleViewImpl::GetWindowTitle() const {
  return controller_->GetWindowTitle();
}

void SendTabToSelfBubbleViewImpl::WindowClosing() {
  if (controller_) {
    controller_->OnBubbleClosed();
    controller_ = nullptr;
  }
}

void SendTabToSelfBubbleViewImpl::OnPaint(gfx::Canvas* canvas) {
  views::BubbleDialogDelegateView::OnPaint(canvas);
}

void SendTabToSelfBubbleViewImpl::Show(DisplayReason reason) {
  ShowForReason(reason);
}

void SendTabToSelfBubbleViewImpl::DeviceButtonPressed(
    SendTabToSelfBubbleDeviceButton* device_button) {
  if (!controller_)
    return;

  controller_->OnDeviceSelected(device_button->device_name(),
                                device_button->device_guid());
  Hide();
}

const views::View* SendTabToSelfBubbleViewImpl::GetButtonContainerForTesting()
    const {
  return scroll_view_->contents();
}

void SendTabToSelfBubbleViewImpl::Init() {
  auto* provider = ChromeLayoutProvider::Get();
  set_margins(
      gfx::Insets(provider->GetDistanceMetric(
                      views::DISTANCE_DIALOG_CONTENT_MARGIN_TOP_CONTROL),
                  0,
                  provider->GetDistanceMetric(
                      views::DISTANCE_DIALOG_CONTENT_MARGIN_BOTTOM_CONTROL),
                  0));
  SetLayoutManager(std::make_unique<views::FillLayout>());

  CreateScrollView();

  PopulateScrollView(controller_->GetValidDevices());
}

void SendTabToSelfBubbleViewImpl::CreateScrollView() {
  scroll_view_ = AddChildView(std::make_unique<views::ScrollView>());
  scroll_view_->ClipHeightTo(0, kDeviceButtonHeight * kMaximumButtons);
}

void SendTabToSelfBubbleViewImpl::PopulateScrollView(
    const std::vector<TargetDeviceInfo>& devices) {
  auto* device_list_view =
      scroll_view_->SetContents(std::make_unique<views::View>());
  device_list_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  for (const auto& device : devices) {
    auto* view = device_list_view->AddChildView(
        std::make_unique<SendTabToSelfBubbleDeviceButton>(this, device));
    view->SetGroup(kDeviceButtonGroup);
  }

  if (!device_list_view->children().empty())
    SetInitiallyFocusedView(device_list_view->children()[0]);

  MaybeSizeToContents();
  Layout();
}

void SendTabToSelfBubbleViewImpl::MaybeSizeToContents() {
  // The widget may be null if this is called while the dialog is opening.
  if (GetWidget())
    SizeToContents();
}

}  // namespace send_tab_to_self
