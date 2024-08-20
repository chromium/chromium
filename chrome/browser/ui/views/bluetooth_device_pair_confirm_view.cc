// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bluetooth_device_pair_confirm_view.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/ui/bluetooth/bluetooth_dialogs.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/color/color_id.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"

using ::content::BluetoothDelegate;

void ShowBluetoothDevicePairConfirmDialog(
    content::WebContents* web_contents,
    const std::u16string& device_identifier,
    const std::optional<std::u16string>& pin,
    BluetoothDelegate::PairPromptCallback close_callback) {
  // This dialog owns itself. DialogDelegateView will delete |dialog| instance.
  auto* dialog = new BluetoothDevicePairConfirmView(device_identifier, pin,
                                                    std::move(close_callback));
  constrained_window::ShowWebModalDialogViews(dialog, web_contents);
}

BluetoothDevicePairConfirmView::BluetoothDevicePairConfirmView(
    const std::u16string& device_identifier,
    const std::optional<std::u16string>& pin,
    BluetoothDelegate::PairPromptCallback close_callback)
    : close_callback_(std::move(close_callback)),
      display_pin_(pin.has_value()) {
  SetModalType(ui::mojom::ModalType::kChild);
  set_margins(ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
      views::DialogContentType::kText, views::DialogContentType::kText));
  SetAcceptCallback(
      base::BindOnce(&BluetoothDevicePairConfirmView::OnDialogAccepted,
                     base::Unretained(this)));
  auto canceled = [](BluetoothDevicePairConfirmView* dialog) {
    std::move(dialog->close_callback_)
        .Run(BluetoothDelegate::PairPromptResult(
            BluetoothDelegate::PairPromptStatus::kCancelled));
  };
  SetCancelCallback(base::BindOnce(canceled, base::Unretained(this)));
  SetCloseCallback(base::BindOnce(canceled, base::Unretained(this)));
  SetButtonEnabled(ui::mojom::DialogButton::kOk, true);
  InitControls(device_identifier, pin);
}

BluetoothDevicePairConfirmView::~BluetoothDevicePairConfirmView() = default;

void BluetoothDevicePairConfirmView::InitControls(
    const std::u16string& device_identifier,
    const std::optional<std::u16string>& pin) {
  //
  // Create the following layout:
  //
  // ┌───────────────┬────────────────────────────────────────────────┐
  // │               │ IDS_BLUETOOTH_DEVICE_PAIR_CONFIRM_TITLE        │
  // │ ┌───────────┐ │                                                │
  // │ │           │ │ IDS_BLUETOOTH_DEVICE_PAIR_CONFIRM_LABEL        │
  // │ │ Bluetooth │ │                                                │
  // │ │    icon   │ │                                                │
  // │ │           │ │                                                │
  // │ └───────────┘ │                          ┌──────┐  ┌────────┐  │
  // │               │                          │  OK  │  │ Cancel │  │
  // │               │                          └──────┘  └────────┘  │
  // └───────────────┴────────────────────────────────────────────────┘
  //
  // Or, if a |pin| is specified.
  //
  // ┌───────────────┬────────────────────────────────────────────────┐
  // │               │ IDS_BLUETOOTH_DEVICE_PASSKEY_CONFIRM_TITLE     │
  // │ ┌───────────┐ │                                                │
  // │ │           │ │ IDS_BLUETOOTH_DEVICE_PASSKEY_CONFIRM_LABEL     │
  // │ │ Bluetooth │ │                                                │
  // │ │    icon   │ │                                                │
  // │ │           │ │                                                │
  // │ └───────────┘ │                          ┌──────┐  ┌────────┐  │
  // │               │                          │  OK  │  │ Cancel │  │
  // │               │                          └──────┘  └────────┘  │
  // └───────────────┴────────────────────────────────────────────────┘
  //

  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetCrossAxisAlignment(views::LayoutAlignment::kCenter);

  // The vertical space that must exist on the top and the bottom of the item
  // to ensure the proper spacing is maintained between items when stacking
  // vertically.
  const int vertical_spacing = ChromeLayoutProvider::Get()->GetDistanceMetric(
                                   DISTANCE_CONTROL_LIST_VERTICAL) /
                               2;
  constexpr int horizontal_spacing = 0;

  constexpr int kIconSize = 30;  // width and height.
  auto icon_view = std::make_unique<views::ImageView>();
  icon_view->SetImage(ui::ImageModel::FromVectorIcon(
      vector_icons::kBluetoothIcon, ui::kColorIcon, kIconSize));
  icon_view_ = AddChildView(std::move(icon_view));

  auto contents_wrapper = std::make_unique<views::View>();

  contents_wrapper->SetProperty(
      views::kMarginsKey,
      gfx::Insets::VH(vertical_spacing, horizontal_spacing));

  contents_wrapper->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetMainAxisAlignment(views::LayoutAlignment::kCenter);
  contents_wrapper->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded));

  {
    // Display the pin if provided, so the user can verify if a matching
    // pin is shown on the device. Otherwise, user verification is solely
    // based upon recognition of the device identifier.
    auto prompt_label =
        display_pin_
            ? std::make_unique<views::Label>(l10n_util::GetStringFUTF16(
                  IDS_BLUETOOTH_DEVICE_PASSKEY_CONFIRM_LABEL, pin.value(),
                  device_identifier))
            : std::make_unique<views::Label>(l10n_util::GetStringFUTF16(
                  IDS_BLUETOOTH_DEVICE_PAIR_CONFIRM_LABEL, device_identifier));

    prompt_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    prompt_label->SetVerticalAlignment(gfx::ALIGN_TOP);
    prompt_label->SetMultiLine(true);
    contents_wrapper->AddChildView(std::move(prompt_label));
  }

  contents_wrapper_ = AddChildView(std::move(contents_wrapper));
}

gfx::Size BluetoothDevicePairConfirmView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  constexpr int kDialogWidth = 440;
  int height =
      GetLayoutManager()->GetPreferredHeightForWidth(this, kDialogWidth);
  return gfx::Size(kDialogWidth, height);
}

std::u16string BluetoothDevicePairConfirmView::GetWindowTitle() const {
  return display_pin_ ? l10n_util::GetStringUTF16(
                            IDS_BLUETOOTH_DEVICE_PASSKEY_CONFIRM_TITLE)
                      : l10n_util::GetStringUTF16(
                            IDS_BLUETOOTH_DEVICE_PAIR_CONFIRM_TITLE);
}

void BluetoothDevicePairConfirmView::OnDialogAccepted() {
  BluetoothDelegate::PairPromptResult prompt_result;
  prompt_result.result_code = BluetoothDelegate::PairPromptStatus::kSuccess;
  std::move(close_callback_).Run(prompt_result);
}

BEGIN_METADATA(BluetoothDevicePairConfirmView)
END_METADATA
