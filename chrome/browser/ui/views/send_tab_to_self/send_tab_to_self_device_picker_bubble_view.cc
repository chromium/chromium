// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_device_picker_bubble_view.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/send_tab_to_self/manage_account_devices_link_view.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_bubble_controller.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_bubble_device_button.h"
#include "chrome/browser/ui/views/sharing_hub/sharing_hub_bubble_util.h"
#include "chrome/grit/generated_resources.h"
#include "components/send_tab_to_self/features.h"
#include "components/send_tab_to_self/target_device_info.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/ui_base_types.h"
#include "ui/color/color_id.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"

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

SendTabToSelfDevicePickerBubbleView::SendTabToSelfDevicePickerBubbleView(
    views::View* anchor_view,
    content::WebContents* web_contents)
    : SendTabToSelfBubbleView(anchor_view, web_contents),
      controller_(SendTabToSelfBubbleController::CreateOrGetFromWebContents(
                      web_contents)
                      ->AsWeakPtr()) {
  DCHECK(controller_);
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  set_fixed_width(ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));
}

SendTabToSelfDevicePickerBubbleView::~SendTabToSelfDevicePickerBubbleView() =
    default;

void SendTabToSelfDevicePickerBubbleView::Hide() {
  if (controller_) {
    controller_->OnBubbleClosed();
    controller_ = nullptr;
  }
  CloseBubble();
}

bool SendTabToSelfDevicePickerBubbleView::ShouldShowCloseButton() const {
  return true;
}

std::u16string SendTabToSelfDevicePickerBubbleView::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_SEND_TAB_TO_SELF);
}

void SendTabToSelfDevicePickerBubbleView::WindowClosing() {
  if (controller_) {
    controller_->OnBubbleClosed();
    controller_ = nullptr;
  }
}

void SendTabToSelfDevicePickerBubbleView::BackButtonPressed() {
  if (controller_) {
    controller_->OnBackButtonPressed();
    Hide();
  }
}

void SendTabToSelfDevicePickerBubbleView::DeviceButtonPressed(
    SendTabToSelfBubbleDeviceButton* device_button) {
  if (!controller_)
    return;

  controller_->OnDeviceSelected(device_button->device_guid());

  GetViewAccessibility().AnnounceText(l10n_util::GetStringFUTF16(
      IDS_SEND_TAB_TO_SELF_SENDING_ANNOUNCE,
      base::UTF8ToUTF16(device_button->device_name())));

  Hide();
}

const views::View*
SendTabToSelfDevicePickerBubbleView::GetButtonContainerForTesting() const {
  return scroll_view_->contents();
}

void SendTabToSelfDevicePickerBubbleView::Init() {
  auto* provider = ChromeLayoutProvider::Get();
  const int top_margin = provider->GetDistanceMetric(
      views::DISTANCE_DIALOG_CONTENT_MARGIN_TOP_TEXT);
  set_margins(gfx::Insets::TLBR(top_margin, 0, 0, 0));

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  CreateHintTextLabel();
  CreateDevicesScrollView();

  AddChildView(std::make_unique<views::Separator>());
  views::View* footer = AddChildView(
      BuildManageAccountDevicesLinkView(/*show_link=*/true, controller_));
  footer->SetBackground(views::CreateThemedSolidBackground(
      ui::kColorMenuItemBackgroundHighlighted));
}

void SendTabToSelfDevicePickerBubbleView::AddedToWidget() {
  if (!controller_->show_back_button())
    return;

  // Adding a title view will replace the default title.
  GetBubbleFrameView()->SetTitleView(
      std::make_unique<sharing_hub::TitleWithBackButtonView>(
          base::BindRepeating(
              &SendTabToSelfDevicePickerBubbleView::BackButtonPressed,
              base::Unretained(this)),
          GetWindowTitle()));
}

void SendTabToSelfDevicePickerBubbleView::CreateHintTextLabel() {
  auto* provider = ChromeLayoutProvider::Get();
  auto* description = AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(
          IDS_TOOLBAR_BUTTON_SEND_TAB_TO_SELF_BUTTON_HINT_TEXT),
      views::style::CONTEXT_LABEL, views::style::STYLE_SECONDARY));
  description->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(0,
                        provider->GetDistanceMetric(
                            views::DISTANCE_BUTTON_HORIZONTAL_PADDING),
                        provider->GetDistanceMetric(
                            views::DISTANCE_UNRELATED_CONTROL_VERTICAL),
                        provider->GetDistanceMetric(
                            views::DISTANCE_BUTTON_HORIZONTAL_PADDING)));
  description->SetMultiLine(true);
  description->SetHorizontalAlignment(gfx::ALIGN_LEFT);
}

void SendTabToSelfDevicePickerBubbleView::CreateDevicesScrollView() {
  scroll_view_ = AddChildView(std::make_unique<views::ScrollView>());
  scroll_view_->ClipHeightTo(0, kDeviceButtonHeight * kMaximumButtons);

  auto* device_list_view =
      scroll_view_->SetContents(std::make_unique<views::View>());
  device_list_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  for (const TargetDeviceInfo& device : controller_->GetValidDevices()) {
    auto* view = device_list_view->AddChildView(
        std::make_unique<SendTabToSelfBubbleDeviceButton>(this, device));
    view->SetGroup(kDeviceButtonGroup);
  }

  if (!device_list_view->children().empty())
    SetInitiallyFocusedView(device_list_view->children()[0]);
}

BEGIN_METADATA(SendTabToSelfDevicePickerBubbleView)
END_METADATA

}  // namespace send_tab_to_self
