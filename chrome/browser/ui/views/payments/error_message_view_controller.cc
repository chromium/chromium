// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/error_message_view_controller.h"

#include <memory>

#include "chrome/browser/ui/views/payments/payment_request_views_util.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace {

class PaymentsErrorLabel : public views::Label {
  METADATA_HEADER(PaymentsErrorLabel, views::Label)

 public:
  PaymentsErrorLabel()
      : Label(l10n_util::GetStringUTF16(IDS_PAYMENTS_ERROR_MESSAGE)) {
    SetMultiLine(true);
    SetHorizontalAlignment(gfx::ALIGN_LEFT);
  }
  ~PaymentsErrorLabel() override = default;

  // views::Label:
  void OnThemeChanged() override {
    Label::OnThemeChanged();
    SetEnabledColor(GetColorProvider()->GetColor(ui::kColorAlertHighSeverity));
  }
};

BEGIN_METADATA(PaymentsErrorLabel)
END_METADATA

}  // namespace

namespace payments {

ErrorMessageViewController::ErrorMessageViewController(
    base::WeakPtr<PaymentRequestSpec> spec,
    base::WeakPtr<PaymentRequestState> state,
    base::WeakPtr<PaymentRequestDialogView> dialog)
    : PaymentRequestSheetController(spec, state, dialog) {}

ErrorMessageViewController::~ErrorMessageViewController() {}

std::u16string ErrorMessageViewController::GetPrimaryButtonLabel() {
  return l10n_util::GetStringUTF16(IDS_CLOSE);
}

PaymentRequestSheetController::ButtonCallback
ErrorMessageViewController::GetPrimaryButtonCallback() {
  return base::BindRepeating(&ErrorMessageViewController::CloseButtonPressed,
                             base::Unretained(this));
}

int ErrorMessageViewController::GetPrimaryButtonId() {
  return static_cast<int>(DialogViewID::CANCEL_BUTTON);
}

bool ErrorMessageViewController::GetPrimaryButtonEnabled() {
  return true;
}

bool ErrorMessageViewController::ShouldShowHeaderBackArrow() {
  return false;
}

bool ErrorMessageViewController::ShouldShowSecondaryButton() {
  return false;
}

std::u16string ErrorMessageViewController::GetSheetTitle() {
  return l10n_util::GetStringUTF16(IDS_PAYMENTS_ERROR_MESSAGE_DIALOG_TITLE);
}

void ErrorMessageViewController::FillContentView(views::View* content_view) {
  auto layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets::VH(0, kPaymentRequestRowHorizontalInsets), 0);
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kStart);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);
  content_view->SetLayoutManager(std::move(layout));
  content_view->AddChildView(std::make_unique<PaymentsErrorLabel>());
}

bool ErrorMessageViewController::GetSheetId(DialogViewID* sheet_id) {
  *sheet_id = DialogViewID::ERROR_SHEET;
  return true;
}

bool ErrorMessageViewController::ShouldAccelerateEnterKey() {
  return true;
}

bool ErrorMessageViewController::CanContentViewBeScrollable() {
  // The error message is a single line of text that doesn't need a scroll view.
  return false;
}

base::WeakPtr<PaymentRequestSheetController>
ErrorMessageViewController::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace payments
