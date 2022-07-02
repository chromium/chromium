// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bluetooth_device_pair_confirm_view.h"

#include <cwctype>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/compositor/layer.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"

using ::content::BluetoothDelegate;

namespace chrome {

void ShowBluetoothDevicePairConfirmDialog(
    content::WebContents* web_contents,
    const std::u16string& device_identifier,
    BluetoothDelegate::PairConfirmCallback close_callback) {
  // This dialog owns itself. DialogDelegateView will delete |dialog| instance.
  auto* dialog = new BluetoothDevicePairConfirmView(device_identifier,
                                                    std::move(close_callback));
  constrained_window::ShowWebModalDialogViews(dialog, web_contents);
}

}  // namespace chrome

BluetoothDevicePairConfirmView::BluetoothDevicePairConfirmView(
    const std::u16string& device_identifier,
    BluetoothDelegate::PairConfirmCallback close_callback)
    : close_callback_(std::move(close_callback)) {
  SetModalType(ui::MODAL_TYPE_CHILD);
  set_margins(ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
      views::DialogContentType::kText, views::DialogContentType::kText));
  SetAcceptCallback(
      base::BindOnce(&BluetoothDevicePairConfirmView::OnDialogAccepted,
                     base::Unretained(this)));
  auto canceled = [](BluetoothDevicePairConfirmView* dialog) {
    std::move(dialog->close_callback_)
        .Run(BluetoothDelegate::DevicePairConfirmPromptResult::kCancelled);
  };
  SetCancelCallback(base::BindOnce(canceled, base::Unretained(this)));
  SetCloseCallback(base::BindOnce(canceled, base::Unretained(this)));
  SetButtonEnabled(ui::DIALOG_BUTTON_OK, true);
  InitControls(device_identifier);
}

BluetoothDevicePairConfirmView::~BluetoothDevicePairConfirmView() = default;

void BluetoothDevicePairConfirmView::InitControls(
    const std::u16string& device_identifier) {
  //
  // Create the following layout:
  //
  // ┌───────────────┬──────────────────────────────────────────────────────────────┐
  // │               │ Pair Confirmation                                            │
  // │ ┌───────────┐ │                                                              │
  // │ │           │ │ Bluetooth device <device name> would like permission to pair.│
  // │ │ Bluetooth │ │                                                              │
  // │ │    icon   │ │                                                              │
  // │ │           │ │                                                              │
  // │ └───────────┘ │                                        ┌──────┐  ┌────────┐  │
  // │               │                                        │  OK  │  │ Cancel │  │
  // │               │                                        └──────┘  └────────┘  │
  // └───────────────┴──────────────────────────────────────────────────────────────┘
  //
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
    auto passkey_prompt_label =
        std::make_unique<views::Label>(l10n_util::GetStringFUTF16(
            IDS_BLUETOOTH_DEVICE_PAIR_CONFIRM_LABEL, device_identifier));

    passkey_prompt_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    passkey_prompt_label->SetMultiLine(true);
    contents_wrapper->AddChildView(std::move(passkey_prompt_label));
  }

  contents_wrapper_ = AddChildView(std::move(contents_wrapper));
}

gfx::Size BluetoothDevicePairConfirmView::CalculatePreferredSize() const {
  constexpr int kDialogWidth = 360;
  int height =
      GetLayoutManager()->GetPreferredHeightForWidth(this, kDialogWidth);
  return gfx::Size(kDialogWidth, height);
}

std::u16string BluetoothDevicePairConfirmView::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_BLUETOOTH_DEVICE_PAIR_CONFIRM_TITLE);
}

void BluetoothDevicePairConfirmView::OnDialogAccepted() {
  std::move(close_callback_)
      .Run(BluetoothDelegate::DevicePairConfirmPromptResult::kSuccess);
}

BEGIN_METADATA(BluetoothDevicePairConfirmView, views::DialogDelegateView)
END_METADATA
