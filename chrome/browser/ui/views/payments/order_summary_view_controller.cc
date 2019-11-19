// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/order_summary_view_controller.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "chrome/browser/ui/views/payments/payment_request_views_util.h"
#include "components/payments/core/currency_formatter.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/font.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/view.h"

namespace payments {

namespace {

// Creates a view for a line item to be displayed in the Order Summary Sheet.
// |label| is the text in the left-aligned label and |amount| is the text of the
// right-aliged label in the row. The |amount| and |label| texts are emphasized
// if |emphasize| is true, which is only the case for the last row containing
// the total of the order. |amount_label_id| is specified to recall the view
// later, e.g. in tests.
std::unique_ptr<views::View> CreateLineItemView(const base::string16& label,
                                                const base::string16& currency,
                                                const base::string16& amount,
                                                bool emphasize,
                                                DialogViewID currency_label_id,
                                                DialogViewID amount_label_id) {
  std::unique_ptr<views::View> row = std::make_unique<views::View>();

  // The vertical spacing for these rows is slightly different than the spacing
  // spacing for clickable rows, so don't use kPaymentRequestRowVerticalInsets.
  constexpr int kRowVerticalInset = 4;
  const gfx::Insets row_insets(
      kRowVerticalInset, payments::kPaymentRequestRowHorizontalInsets,
      kRowVerticalInset, payments::kPaymentRequestRowHorizontalInsets);
  row->SetBorder(payments::CreatePaymentRequestRowBorder(
      row->GetNativeTheme()->GetSystemColor(
          ui::NativeTheme::kColorId_SeparatorColor),
      row_insets));

  views::GridLayout* layout =
      row->SetLayoutManager(std::make_unique<views::GridLayout>());

  views::ColumnSet* columns = layout->AddColumnSet(0);
  // The first column has resize_percent = 1 so that it stretches all the way
  // across the row up to the amount label. This way the first label elides as
  // required.
  columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER, 1.0,
                     views::GridLayout::USE_PREF, 0, 0);
  columns->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER,
                     views::GridLayout::kFixedSize, views::GridLayout::FIXED,
                     kAmountSectionWidth, kAmountSectionWidth);

  layout->StartRow(views::GridLayout::kFixedSize, 0);
  std::unique_ptr<views::Label> label_text;
  std::unique_ptr<views::Label> currency_text;
  std::unique_ptr<views::Label> amount_text;
  if (emphasize) {
    label_text = CreateMediumLabel(label);
    currency_text = CreateMediumLabel(currency);
    amount_text = CreateMediumLabel(amount);
  } else {
    label_text = std::make_unique<views::Label>(label);
    currency_text = CreateHintLabel(currency);
    amount_text = std::make_unique<views::Label>(amount);
  }
  // Strings from the website may not match the locale of the device, so align
  // them according to the language of the text. This will result, for example,
  // in "he" labels being right-aligned in a browser that's using "en" locale.
  label_text->SetHorizontalAlignment(gfx::ALIGN_TO_HEAD);
  amount_text->SetID(static_cast<int>(amount_label_id));
  amount_text->SetMultiLine(true);
  // The amount is formatted by the browser (and not provided by the website) so
  // it can be aligned to left.
  amount_text->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  amount_text->SetAllowCharacterBreak(true);

  std::unique_ptr<views::View> amount_wrapper = std::make_unique<views::View>();
  views::GridLayout* wrapper_layout =
      amount_wrapper->SetLayoutManager(std::make_unique<views::GridLayout>());
  views::ColumnSet* wrapper_columns = wrapper_layout->AddColumnSet(0);
  wrapper_columns->AddColumn(
      views::GridLayout::LEADING, views::GridLayout::CENTER,
      views::GridLayout::kFixedSize, views::GridLayout::USE_PREF, 0, 0);
  wrapper_columns->AddColumn(views::GridLayout::TRAILING,
                             views::GridLayout::CENTER, 1.0,
                             views::GridLayout::USE_PREF, 0, 0);

  wrapper_layout->StartRow(views::GridLayout::kFixedSize, 0);
  currency_text->SetID(static_cast<int>(currency_label_id));
  wrapper_layout->AddView(std::move(currency_text));
  wrapper_layout->AddView(std::move(amount_text));

  layout->AddView(std::move(label_text));
  layout->AddView(std::move(amount_wrapper));

  return row;
}

}  // namespace

OrderSummaryViewController::OrderSummaryViewController(
    PaymentRequestSpec* spec,
    PaymentRequestState* state,
    PaymentRequestDialogView* dialog)
    : PaymentRequestSheetController(spec, state, dialog), pay_button_(nullptr) {
  spec->AddObserver(this);
  state->AddObserver(this);
}

OrderSummaryViewController::~OrderSummaryViewController() {
  spec()->RemoveObserver(this);
  state()->RemoveObserver(this);
}

void OrderSummaryViewController::OnSpecUpdated() {
  UpdateContentView();
}

void OrderSummaryViewController::OnSelectedInformationChanged() {
  UpdatePayButtonState(state()->is_ready_to_pay());
}

std::unique_ptr<views::Button>
OrderSummaryViewController::CreatePrimaryButton() {
  std::unique_ptr<views::Button> button(
      views::MdTextButton::CreateSecondaryUiBlueButton(
          this, l10n_util::GetStringUTF16(IDS_PAYMENTS_PAY_BUTTON)));
  button->set_tag(static_cast<int>(PaymentRequestCommonTags::PAY_BUTTON_TAG));
  button->SetID(static_cast<int>(DialogViewID::PAY_BUTTON));
  pay_button_ = button.get();
  UpdatePayButtonState(state()->is_ready_to_pay());
  return button;
}

bool OrderSummaryViewController::ShouldShowSecondaryButton() {
  return false;
}

base::string16 OrderSummaryViewController::GetSheetTitle() {
  return l10n_util::GetStringUTF16(IDS_PAYMENTS_ORDER_SUMMARY_LABEL);
}

void OrderSummaryViewController::FillContentView(views::View* content_view) {
  auto layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical);
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kStart);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);
  content_view->SetLayoutManager(std::move(layout));

  bool is_mixed_currency = spec()->IsMixedCurrency();
  // Set the ID for the first few line items labels, for testing.
  const std::vector<DialogViewID> line_items{
      DialogViewID::ORDER_SUMMARY_LINE_ITEM_1,
      DialogViewID::ORDER_SUMMARY_LINE_ITEM_2,
      DialogViewID::ORDER_SUMMARY_LINE_ITEM_3};
  const auto& display_items = spec()->GetDisplayItems(state()->selected_app());
  for (size_t i = 0; i < display_items.size(); i++) {
    DialogViewID view_id =
        i < line_items.size() ? line_items[i] : DialogViewID::VIEW_ID_NONE;
    base::string16 currency = base::UTF8ToUTF16("");
    if (is_mixed_currency) {
      currency = base::UTF8ToUTF16((*display_items[i])->amount->currency);
    }

    content_view->AddChildView(
        CreateLineItemView(
            base::UTF8ToUTF16((*display_items[i])->label), currency,
            spec()->GetFormattedCurrencyAmount((*display_items[i])->amount),
            false, DialogViewID::VIEW_ID_NONE, view_id)
            .release());
  }

  base::string16 total_label_value = l10n_util::GetStringFUTF16(
      IDS_PAYMENT_REQUEST_ORDER_SUMMARY_SHEET_TOTAL_FORMAT,
      base::UTF8ToUTF16(
          spec()->GetTotal(state()->selected_app())->amount->currency),
      spec()->GetFormattedCurrencyAmount(
          spec()->GetTotal(state()->selected_app())->amount));

  content_view->AddChildView(
      CreateLineItemView(
          base::UTF8ToUTF16(spec()->GetTotal(state()->selected_app())->label),
          base::UTF8ToUTF16(
              spec()->GetTotal(state()->selected_app())->amount->currency),
          spec()->GetFormattedCurrencyAmount(
              spec()->GetTotal(state()->selected_app())->amount),
          true, DialogViewID::ORDER_SUMMARY_TOTAL_CURRENCY_LABEL,
          DialogViewID::ORDER_SUMMARY_TOTAL_AMOUNT_LABEL)
          .release());
}

void OrderSummaryViewController::UpdatePayButtonState(bool enabled) {
  pay_button_->SetEnabled(enabled);
}

}  // namespace payments
