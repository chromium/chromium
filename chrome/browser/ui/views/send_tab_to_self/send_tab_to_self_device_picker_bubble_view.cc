// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_device_picker_bubble_view.h"

#include "base/functional/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser_finder.h"
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
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"

namespace send_tab_to_self {

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(SendTabToSelfDevicePickerBubbleView,
                                      kSendTabToSelfDevicePickerBubbleId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(SendTabToSelfDevicePickerBubbleView,
                                      kManageDevicesLinkElementId);

namespace {

// The valid device button height.
constexpr int kDeviceButtonHeight = 56;
// The valid device button height for the device selection bubble flow.
constexpr int kDeviceSelectionButtonHeight = 54;
// Maximum number of buttons that are shown without scroll. If the device
// number is larger than kMaximumButtons, the bubble content will be
// scrollable.
constexpr int kMaximumButtons = 5;

// Used to group the device buttons together, which makes moving between them
// with arrow keys possible.
constexpr int kDeviceButtonGroup = 0;

}  // namespace

SendTabToSelfDevicePickerBubbleView::SendTabToSelfDevicePickerBubbleView(
    views::BubbleAnchor anchor,
    content::WebContents* web_contents)
    : SendTabToSelfBubbleView(anchor, web_contents) {
  SetProperty(views::kElementIdentifierKey, kSendTabToSelfDevicePickerBubbleId);
}

// Sends the tab immediately when clicked (legacy instant-send behavior).
void SendTabToSelfDevicePickerBubbleView::DeviceButtonPressed(
    SendTabToSelfBubbleDeviceButton* device_button) {
  CHECK(!base::FeatureList::IsEnabled(kSendTabToSelfShowTargetsInContextMenus));
  if (!controller_) {
    return;
  }

  controller_->OnDeviceSelected(device_button->device_guid(),
                                device_button->device_name());

  GetViewAccessibility().AnnounceText(l10n_util::GetStringFUTF16(
      IDS_SEND_TAB_TO_SELF_SENDING_ANNOUNCE,
      base::UTF8ToUTF16(device_button->device_name())));

  Hide();
}

// Highlights the chosen device with a checkmark in the modernized flow.
void SendTabToSelfDevicePickerBubbleView::SelectTargetDevice(
    SendTabToSelfBubbleDeviceButton* device_button) {
  CHECK(base::FeatureList::IsEnabled(kSendTabToSelfShowTargetsInContextMenus));
  CHECK(device_button);
  // Skip redundant state updates and paint passes if already selected.
  if (selected_button_ == device_button) {
    return;
  }

  // If there was a previously selected device, deselect it first.
  if (selected_button_) {
    selected_button_->SetSelected(false);
  }

  selected_button_ = device_button;
  selected_button_->SetSelected(true);
  SetButtonEnabled(ui::mojom::DialogButton::kOk, true);
}

const views::View*
SendTabToSelfDevicePickerBubbleView::GetButtonContainerForTesting() const {
  return scroll_view_->contents();
}

// Switches between legacy and modernized bubble layouts based on the feature
// flag.
void SendTabToSelfDevicePickerBubbleView::Init() {
  if (base::FeatureList::IsEnabled(kSendTabToSelfShowTargetsInContextMenus)) {
    InitDeviceSelectionBubble();
  } else {
    InitInstantSendBubble();
  }
}

bool SendTabToSelfDevicePickerBubbleView::ShouldShowCloseButton() const {
  // Hide redundant close button in modernized flow (has Cancel button).
  return !base::FeatureList::IsEnabled(kSendTabToSelfShowTargetsInContextMenus);
}

// Initializes the legacy bubble layout (instant-send, description text, avatar
// footer).
void SendTabToSelfDevicePickerBubbleView::InitInstantSendBubble() {
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
  footer->SetBackground(
      views::CreateSolidBackground(ui::kColorMenuItemBackgroundHighlighted));

  // No dialog buttons in the legacy bubble (instant-send on click).
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
}

// Initializes the modernized bubble layout (title, subtitle, inline link,
// selection list, Send/Cancel buttons).
void SendTabToSelfDevicePickerBubbleView::InitDeviceSelectionBubble() {
  auto* provider = ChromeLayoutProvider::Get();

  gfx::Insets dialog_insets = provider->GetInsetsMetric(views::INSETS_DIALOG);
  // Eliminating horizontal margins enables full-width hover states for device
  // buttons.
  set_margins(gfx::Insets::TLBR(
      provider->GetDistanceMetric(
          views::DISTANCE_DIALOG_CONTENT_MARGIN_TOP_TEXT),
      0,
      provider->GetDistanceMetric(views::DISTANCE_UNRELATED_CONTROL_VERTICAL),
      0));

  // Stretch ScrollView to full width of the bubble.
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL)));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);

  SetTitle(l10n_util::GetStringUTF16(IDS_SEND_TAB_TO_SELF_DEVICE_PICKER_TITLE));

  // Subtitle description.
  // Create a container to group the subtitle and link closely.
  auto* header_container = AddChildView(std::make_unique<views::View>());
  auto* header_layout =
      header_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical, gfx::Insets(), 0));
  header_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);
  header_container->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(0, dialog_insets.left(), 0, dialog_insets.right()));

  // Subtitle description.
  auto* subtitle =
      header_container->AddChildView(std::make_unique<views::Label>(
          l10n_util::GetStringUTF16(
              IDS_SEND_TAB_TO_SELF_DEVICE_PICKER_SUBTITLE),
          views::style::CONTEXT_LABEL, views::style::STYLE_CAPTION));
  subtitle->SetEnabledColor(ui::kColorLabelForegroundSecondary);
  subtitle->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  subtitle->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  SetInitiallyFocusedView(subtitle);

  // Manage devices link.
  auto* link = header_container->AddChildView(std::make_unique<views::Link>(
      l10n_util::GetStringUTF16(IDS_SEND_TAB_TO_SELF_MANAGE_DEVICES),
      views::style::CONTEXT_LABEL, views::style::STYLE_CAPTION));
  link->SetCallback(base::BindRepeating(
      &SendTabToSelfBubbleController::OnManageDevicesClicked, controller_));
  link->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  link->SetProperty(views::kElementIdentifierKey, kManageDevicesLinkElementId);

  CreateDevicesScrollView();

  // Configure standard dialog buttons (Send / Cancel)
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kOk) |
             static_cast<int>(ui::mojom::DialogButton::kCancel));
  SetButtonLabel(
      ui::mojom::DialogButton::kOk,
      l10n_util::GetStringUTF16(IDS_SEND_TAB_TO_SELF_SEND_BUTTON_LABEL));
  SetButtonLabel(ui::mojom::DialogButton::kCancel,
                 l10n_util::GetStringUTF16(IDS_APP_CANCEL));
  if (!selected_button_) {
    SetButtonEnabled(ui::mojom::DialogButton::kOk, false);
  }
  SetAcceptCallback(
      base::BindOnce(&SendTabToSelfDevicePickerBubbleView::HandleSendClicked,
                     base::Unretained(this)));
}

// Triggered when the "Send" button is clicked. Sends the tab to the selected
// device.
void SendTabToSelfDevicePickerBubbleView::HandleSendClicked() {
  if (!controller_) {
    return;
  }
  // A device is always selected by default on open, so selected_button_ should
  // never be null when the Send button is clicked.
  CHECK(selected_button_);

  controller_->OnDeviceSelected(selected_button_->device_guid(),
                                selected_button_->device_name());

  GetViewAccessibility().AnnounceText(l10n_util::GetStringFUTF16(
      IDS_SEND_TAB_TO_SELF_SENDING_ANNOUNCE,
      base::UTF8ToUTF16(selected_button_->device_name())));
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
  const int button_height =
      base::FeatureList::IsEnabled(kSendTabToSelfShowTargetsInContextMenus)
          ? kDeviceSelectionButtonHeight
          : kDeviceButtonHeight;
  scroll_view_->ClipHeightTo(0, button_height * kMaximumButtons);

  auto* device_list_view =
      scroll_view_->SetContents(std::make_unique<views::View>());
  auto* list_layout =
      device_list_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));
  // Stretch device buttons to full width of list.
  list_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);
  SendTabToSelfBubbleDeviceButton* first_device = nullptr;
  for (const TargetDeviceInfo& device : controller_->GetValidDevices()) {
    auto* view = device_list_view->AddChildView(
        std::make_unique<SendTabToSelfBubbleDeviceButton>(this, device));
    view->SetGroup(kDeviceButtonGroup);
    if (!first_device) {
      first_device = view;
    }
  }

  if (first_device) {
    if (base::FeatureList::IsEnabled(kSendTabToSelfShowTargetsInContextMenus)) {
      // Auto-select the first device on launch
      SelectTargetDevice(first_device);
    } else {
      SetInitiallyFocusedView(first_device);
    }
  }
}

BEGIN_METADATA(SendTabToSelfDevicePickerBubbleView)
END_METADATA

}  // namespace send_tab_to_self
