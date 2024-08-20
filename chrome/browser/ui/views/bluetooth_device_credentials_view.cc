// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bluetooth_device_credentials_view.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ui/bluetooth/bluetooth_dialogs.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/vector_icons/vector_icons.h"
#include "device/bluetooth/strings/grit/bluetooth_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/color/color_id.h"
#include "ui/compositor/layer.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"

using content::BluetoothDelegate;

void ShowBluetoothDeviceCredentialsDialog(
    content::WebContents* web_contents,
    const std::u16string& device_identifier,
    BluetoothDelegate::PairPromptCallback close_callback) {
  // This dialog owns itself. DialogDelegateView will delete |dialog| instance.
  auto* dialog = new BluetoothDeviceCredentialsView(device_identifier,
                                                    std::move(close_callback));
  constrained_window::ShowWebModalDialogViews(dialog, web_contents);
}

namespace {

bool IsInputTextValid(const std::u16string& text) {
  const size_t num_digits = base::ranges::count_if(
      text, [](char16_t ch) { return base::IsAsciiDigit(ch); });
  // This dialog is currently only used to prompt for Bluetooth PINs which
  // are always six digit numeric values as per the spec. This function could
  // do a better job of validating input, but should also be accompanied by
  // a better UI to help the user understand why the "OK" button is disabled
  // when a seemingly valid PIN, which doesn't conform to the spec., has been
  // input.
  return num_digits > 0;
}

}  // namespace

BluetoothDeviceCredentialsView::BluetoothDeviceCredentialsView(
    const std::u16string& device_identifier,
    BluetoothDelegate::PairPromptCallback close_callback)
    : close_callback_(std::move(close_callback)) {
  SetModalType(ui::mojom::ModalType::kChild);
  set_margins(ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
      views::DialogContentType::kText, views::DialogContentType::kText));
  SetAcceptCallback(
      base::BindOnce(&BluetoothDeviceCredentialsView::OnDialogAccepted,
                     base::Unretained(this)));
  auto canceled = [](BluetoothDeviceCredentialsView* dialog) {
    std::move(dialog->close_callback_)
        .Run(BluetoothDelegate::PairPromptResult(
            BluetoothDelegate::PairPromptStatus::kCancelled));
  };
  SetCancelCallback(base::BindOnce(canceled, base::Unretained(this)));
  SetCloseCallback(base::BindOnce(canceled, base::Unretained(this)));
  InitControls(device_identifier);
}

BluetoothDeviceCredentialsView::~BluetoothDeviceCredentialsView() = default;

void BluetoothDeviceCredentialsView::InitControls(
    const std::u16string& device_identifier) {
  //
  // Create the following layout:
  //
  // ┌───────────────┬─────────────────────────────────────────────┐
  // │               │ Device passkey                              │
  // │ ┌───────────┐ │                                             │
  // │ │           │ │ Please enter the passkey for <device name>: │
  // │ │ Bluetooth │ │ ┌────────────────────────────────────────┐  │
  // │ │    icon   │ │ │                                        │  │
  // │ │           │ │ └────────────────────────────────────────┘  │
  // │ └───────────┘ │                       ┌──────┐  ┌────────┐  │
  // │               │                       │  OK  │  │ Cancel │  │
  // │               │                       └──────┘  └────────┘  │
  // └───────────────┴─────────────────────────────────────────────┘
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

  constexpr int kIconSize = 48;  // width and height.
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

  views::Label* passkey_prompt_label_ptr = nullptr;
  {
    auto passkey_prompt_label =
        std::make_unique<views::Label>(l10n_util::GetStringFUTF16(
            IDS_BLUETOOTH_DEVICE_CREDENTIALS_LABEL, device_identifier));
    passkey_prompt_label_ptr = passkey_prompt_label.get();
    passkey_prompt_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    passkey_prompt_label->SetMultiLine(true);
    contents_wrapper->AddChildView(std::move(passkey_prompt_label));
  }

  {
    constexpr int kDefaultTextfieldNumChars = 8;
    constexpr int kMinimumTextfieldNumChars = 6;

    passkey_text_ =
        contents_wrapper->AddChildView(std::make_unique<views::Textfield>());
    passkey_text_->set_controller(this);
    passkey_text_->SetDefaultWidthInChars(kDefaultTextfieldNumChars);
    passkey_text_->SetMinimumWidthInChars(kMinimumTextfieldNumChars);
    passkey_text_->SetTextInputType(ui::TEXT_INPUT_TYPE_TEXT);
    passkey_text_->GetViewAccessibility().SetName(*passkey_prompt_label_ptr);
    // TODO(cmumford): Windows Narrator says "no item in view".
  }

  contents_wrapper_ = AddChildView(std::move(contents_wrapper));
}

views::View* BluetoothDeviceCredentialsView::GetInitiallyFocusedView() {
  return passkey_text_;
}

gfx::Size BluetoothDeviceCredentialsView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  constexpr int kDialogWidth = 360;
  int height =
      GetLayoutManager()->GetPreferredHeightForWidth(this, kDialogWidth);
  return gfx::Size(kDialogWidth, height);
}

bool BluetoothDeviceCredentialsView::IsDialogButtonEnabled(
    ui::mojom::DialogButton button) const {
  if (button != ui::mojom::DialogButton::kOk) {
    return true;  // Only "OK" button is sensitized - all others are enabled.
  }

  return IsInputTextValid(passkey_text_->GetText());
}

std::u16string BluetoothDeviceCredentialsView::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_BLUETOOTH_DEVICE_CREDENTIALS_TITLE);
}

void BluetoothDeviceCredentialsView::OnDialogAccepted() {
  DCHECK(IsDialogButtonEnabled(ui::mojom::DialogButton::kOk));

  std::u16string trimmed_input;
  base::TrimWhitespace(passkey_text_->GetText(), base::TRIM_ALL,
                       &trimmed_input);

  BluetoothDelegate::PairPromptResult result;
  result.result_code = BluetoothDelegate::PairPromptStatus::kSuccess;
  result.pin = base::UTF16ToUTF8(trimmed_input);
  std::move(close_callback_).Run(result);
}

void BluetoothDeviceCredentialsView::ContentsChanged(
    views::Textfield* sender,
    const std::u16string& new_contents) {
  DCHECK_EQ(sender, passkey_text_);
  SetButtonEnabled(ui::mojom::DialogButton::kOk,
                   IsInputTextValid(new_contents));
  DialogModelChanged();
}

BEGIN_METADATA(BluetoothDeviceCredentialsView)
END_METADATA
