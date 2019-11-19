// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/error_message_view_controller.h"

#include <memory>

#include "chrome/browser/ui/views/payments/payment_request_views_util.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace payments {

ErrorMessageViewController::ErrorMessageViewController(
    PaymentRequestSpec* spec,
    PaymentRequestState* state,
    PaymentRequestDialogView* dialog)
    : PaymentRequestSheetController(spec, state, dialog) {}

ErrorMessageViewController::~ErrorMessageViewController() {}

std::unique_ptr<views::Button>
ErrorMessageViewController::CreatePrimaryButton() {
  std::unique_ptr<views::Button> button(
      views::MdTextButton::CreateSecondaryUiBlueButton(
          this, l10n_util::GetStringUTF16(IDS_CLOSE)));
  button->set_tag(static_cast<int>(PaymentRequestCommonTags::CLOSE_BUTTON_TAG));
  button->SetID(static_cast<int>(DialogViewID::CANCEL_BUTTON));
  return button;
}

bool ErrorMessageViewController::ShouldShowHeaderBackArrow() {
  return false;
}

bool ErrorMessageViewController::ShouldShowSecondaryButton() {
  return false;
}

base::string16 ErrorMessageViewController::GetSheetTitle() {
  return l10n_util::GetStringUTF16(IDS_PAYMENTS_ERROR_MESSAGE_DIALOG_TITLE);
}

void ErrorMessageViewController::FillContentView(views::View* content_view) {
  auto layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets(0, kPaymentRequestRowHorizontalInsets), 0);
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kStart);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);
  content_view->SetLayoutManager(std::move(layout));

  std::unique_ptr<views::Label> label = std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_PAYMENTS_ERROR_MESSAGE));
  label->SetEnabledColor(label->GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_AlertSeverityHigh));
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  content_view->AddChildView(label.release());
}

}  // namespace payments
