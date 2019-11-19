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

}  // namespace

SendTabToSelfBubbleViewImpl::SendTabToSelfBubbleViewImpl(
    views::View* anchor_view,
    content::WebContents* web_contents,
    SendTabToSelfBubbleController* controller)
    : LocationBarBubbleDelegateView(anchor_view, web_contents),
      web_contents_(web_contents),
      controller_(controller) {
  DialogDelegate::set_buttons(ui::DIALOG_BUTTON_NONE);
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

base::string16 SendTabToSelfBubbleViewImpl::GetWindowTitle() const {
  return controller_->GetWindowTitle();
}

void SendTabToSelfBubbleViewImpl::WindowClosing() {
  if (controller_) {
    controller_->OnBubbleClosed();
    controller_ = nullptr;
  }
}

bool SendTabToSelfBubbleViewImpl::Close() {
  return Cancel();
}

void SendTabToSelfBubbleViewImpl::ButtonPressed(views::Button* sender,
                                                const ui::Event& event) {
  DevicePressed(sender->tag());
}

gfx::Size SendTabToSelfBubbleViewImpl::CalculatePreferredSize() const {
  const int width = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_BUBBLE_PREFERRED_WIDTH);
  return gfx::Size(width, GetHeightForWidth(width));
}

void SendTabToSelfBubbleViewImpl::OnPaint(gfx::Canvas* canvas) {
  views::BubbleDialogDelegateView::OnPaint(canvas);
}

void SendTabToSelfBubbleViewImpl::Show(DisplayReason reason) {
  ShowForReason(reason);
}

const std::vector<SendTabToSelfBubbleDeviceButton*>&
SendTabToSelfBubbleViewImpl::GetDeviceButtonsForTest() {
  return device_buttons_;
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
  scroll_view_ = new views::ScrollView();
  AddChildView(scroll_view_);
  scroll_view_->ClipHeightTo(0, kDeviceButtonHeight * kMaximumButtons);
}

void SendTabToSelfBubbleViewImpl::PopulateScrollView(
    const std::vector<TargetDeviceInfo>& devices) {
  DCHECK(device_buttons_.empty());
  auto device_list_view = std::make_unique<views::View>();
  device_list_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  int tag = 0;
  for (const auto& device : devices) {
    auto device_button = std::make_unique<SendTabToSelfBubbleDeviceButton>(
        this, device,
        /** button_tag */ tag++);
    device_buttons_.push_back(device_button.get());
    device_list_view->AddChildView(std::move(device_button));
  }
  scroll_view_->SetContents(std::move(device_list_view));

  MaybeSizeToContents();
  Layout();
}

void SendTabToSelfBubbleViewImpl::DevicePressed(size_t index) {
  if (!controller_) {
    return;
  }

  DCHECK_LT(index, device_buttons_.size());

  SendTabToSelfBubbleDeviceButton* device_button = device_buttons_[index];
  controller_->OnDeviceSelected(device_button->device_name(),
                                device_button->device_guid());
  Hide();
}

void SendTabToSelfBubbleViewImpl::MaybeSizeToContents() {
  // The widget may be null if this is called while the dialog is opening.
  if (GetWidget()) {
    SizeToContents();
  }
}

}  // namespace send_tab_to_self
