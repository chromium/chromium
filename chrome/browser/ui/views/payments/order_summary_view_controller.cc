// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/order_summary_view_controller.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "chrome/browser/ui/views/payments/payment_request_views_util.h"
#include "components/payments/core/currency_formatter.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/font.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/table_layout.h"
#include "ui/views/view.h"

namespace payments {

namespace {

// The vertical spacing for these rows is slightly different than the
// spacing spacing for clickable rows, so don't use
// kPaymentRequestRowVerticalInsets.
constexpr int kLineItemRowVerticalInset = 4;

class LineItemRow : public views::View {
  METADATA_HEADER(LineItemRow, views::View)

 public:
  LineItemRow()
      : row_insets_(
            gfx::Insets::TLBR(kLineItemRowVerticalInset,
                              payments::kPaymentRequestRowHorizontalInsets,
                              kLineItemRowVerticalInset,
                              payments::kPaymentRequestRowHorizontalInsets)) {
    // The border color will be set to the theme color in OnThemeChanged, but we
    // need to initialize the view with an empty border so that the correct
    // bounds are computed.
    SetBorder(views::CreateEmptyBorder(row_insets_));
  }

  // views::View:
  void OnThemeChanged() override {
    View::OnThemeChanged();
    SetBorder(payments::CreatePaymentRequestRowBorder(
        GetColorProvider()->GetColor(ui::kColorSeparator), row_insets_));
  }

 private:
  gfx::Insets row_insets_;
};

BEGIN_METADATA(LineItemRow)
END_METADATA

// Creates a view for a line item to be displayed in the Order Summary Sheet.
// |label| is the text in the left-aligned label and |amount| is the text of the
// right-aliged label in the row. The |amount| and |label| texts are emphasized
// if |emphasize| is true, which is only the case for the last row containing
// the total of the order. |amount_label_id| is specified to recall the view
// later, e.g. in tests.
std::unique_ptr<views::View> CreateLineItemView(const std::u16string& label,
                                                const std::u16string& currency,
                                                const std::u16string& amount,
                                                bool emphasize,
                                                DialogViewID currency_label_id,
                                                DialogViewID amount_label_id) {
  std::unique_ptr<views::View> row = std::make_unique<LineItemRow>();
  views::TableLayout* const layout =
      row->SetLayoutManager(std::make_unique<views::TableLayout>());

  // The first column has resize_percent = 1 so that it stretches all the way
  // across the row up to the amount label. This way the first label elides as
  // required.
  layout->AddColumn(views::LayoutAlignment::kStart,
                    views::LayoutAlignment::kCenter, 1.0,
                    views::TableLayout::ColumnSize::kUsePreferred, 0, 0);
  layout->AddColumn(views::LayoutAlignment::kCenter,
                    views::LayoutAlignment::kCenter,
                    views::TableLayout::kFixedSize,
                    views::TableLayout::ColumnSize::kUsePreferred,
                    kAmountSectionWidth, kAmountSectionWidth);
  layout->AddColumn(views::LayoutAlignment::kEnd,
                    views::LayoutAlignment::kCenter,
                    views::TableLayout::kFixedSize,
                    views::TableLayout::ColumnSize::kUsePreferred, 0, 0);

  layout->AddRows(1, views::TableLayout::kFixedSize);
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

  currency_text->SetID(static_cast<int>(currency_label_id));

  row->AddChildView(std::move(label_text));
  row->AddChildView(std::move(currency_text));
  row->AddChildView(std::move(amount_text));

  return row;
}

}  // namespace

OrderSummaryViewController::OrderSummaryViewController(
    base::WeakPtr<PaymentRequestSpec> spec,
    base::WeakPtr<PaymentRequestState> state,
    base::WeakPtr<PaymentRequestDialogView> dialog)
    : PaymentRequestSheetController(spec, state, dialog) {
  DCHECK(spec);
  DCHECK(state);
  spec->AddObserver(this);
  state->AddObserver(this);
}

OrderSummaryViewController::~OrderSummaryViewController() {
  if (spec())
    spec()->RemoveObserver(this);

  state()->RemoveObserver(this);
}

void OrderSummaryViewController::OnSpecUpdated() {
  UpdateContentView();
}

void OrderSummaryViewController::OnSelectedInformationChanged() {
  primary_button()->SetEnabled(GetPrimaryButtonEnabled());
}

bool OrderSummaryViewController::ShouldShowSecondaryButton() {
  return false;
}

std::u16string OrderSummaryViewController::GetSheetTitle() {
  return l10n_util::GetStringUTF16(IDS_PAYMENTS_ORDER_SUMMARY_LABEL);
}

void OrderSummaryViewController::FillContentView(views::View* content_view) {
  if (!spec())
    return;

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
    std::u16string currency = u"";
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

  std::u16string total_label_value = l10n_util::GetStringFUTF16(
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

bool OrderSummaryViewController::GetSheetId(DialogViewID* sheet_id) {
  *sheet_id = DialogViewID::ORDER_SUMMARY_SHEET;
  return true;
}

bool OrderSummaryViewController::ShouldAccelerateEnterKey() {
  return true;
}

base::WeakPtr<PaymentRequestSheetController>
OrderSummaryViewController::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace payments
