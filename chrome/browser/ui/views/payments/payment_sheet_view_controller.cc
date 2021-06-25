// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/payment_sheet_view_controller.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/i18n/message_formatter.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "chrome/browser/ui/views/payments/payment_request_row_view.h"
#include "chrome/browser/ui/views/payments/payment_request_views_util.h"
#include "chrome/common/url_constants.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/payments/content/payment_app.h"
#include "components/payments/content/payment_request_spec.h"
#include "components/payments/content/payment_request_state.h"
#include "components/payments/core/currency_formatter.h"
#include "components/payments/core/payment_prefs.h"
#include "components/payments/core/strings_util.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/font.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/range/range.h"
#include "ui/gfx/text_elider.h"
#include "ui/gfx/text_utils.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/color_tracking_icon_view.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/metadata/metadata_impl_macros.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view.h"

namespace payments {
namespace {

// A class that ensures proper elision of labels in the form
// "[preview] and N more" where preview might be elided to allow "and N more" to
// be always visible.
class PreviewEliderLabel : public views::Label {
 public:
  METADATA_HEADER(PreviewEliderLabel);

  // Creates a PreviewEliderLabel where |preview_text| might be elided,
  // |format_string| is the string with format argument numbers in ICU syntax
  // and |n| is the "N more" item count.
  PreviewEliderLabel(const std::u16string& preview_text,
                     const std::u16string& format_string,
                     int n,
                     int text_style)
      : views::Label(std::u16string(), views::style::CONTEXT_LABEL, text_style),
        preview_text_(preview_text),
        format_string_(format_string),
        n_(n) {}
  PreviewEliderLabel(const PreviewEliderLabel&) = delete;
  PreviewEliderLabel& operator=(const PreviewEliderLabel&) = delete;
  ~PreviewEliderLabel() override = default;

  // Formats |preview_text_|, |format_string_|, and |n_| into a string that fits
  // inside of |pixel_width|, eliding |preview_text_| as required.
  std::u16string CreateElidedString(int pixel_width) {
    for (int preview_length = preview_text_.size(); preview_length > 0;
         --preview_length) {
      std::u16string elided_preview;
      gfx::ElideRectangleString(preview_text_, 1, preview_length,
                                /*strict=*/false, &elided_preview);
      std::u16string elided_string =
          base::i18n::MessageFormatter::FormatWithNumberedArgs(
              format_string_, "", elided_preview, n_);
      if (gfx::GetStringWidth(elided_string, font_list()) <= pixel_width)
        return elided_string;
    }

    // TODO(crbug.com/714776): Display something meaningful if the preview can't
    // be elided enough for the string to fit.
    return std::u16string();
  }

 private:
  // views::View:
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override {
    SetText(CreateElidedString(width()));
    views::Label::OnBoundsChanged(previous_bounds);
  }

  std::u16string preview_text_;
  std::u16string format_string_;
  int n_;
};

BEGIN_METADATA(PreviewEliderLabel, views::Label)
END_METADATA

std::unique_ptr<PaymentRequestRowView> CreatePaymentSheetRow(
    views::Button::PressedCallback callback,
    const std::u16string& section_name,
    const std::u16string& accessible_content,
    std::unique_ptr<views::View> content_view,
    std::unique_ptr<views::View> extra_content_view,
    std::unique_ptr<views::View> trailing_button,
    bool clickable,
    bool extra_trailing_inset,
    views::GridLayout::Alignment vertical_alignment =
        views::GridLayout::LEADING) {
  const int trailing_inset = extra_trailing_inset
                                 ? kPaymentRequestRowHorizontalInsets +
                                       kPaymentRequestRowExtraRightInset
                                 : kPaymentRequestRowHorizontalInsets;
  const gfx::Insets row_insets(
      kPaymentRequestRowVerticalInsets, kPaymentRequestRowHorizontalInsets,
      kPaymentRequestRowVerticalInsets, trailing_inset);
  std::unique_ptr<PaymentRequestRowView> row =
      std::make_unique<PaymentRequestRowView>(std::move(callback), clickable,
                                              row_insets);
  views::GridLayout* layout =
      row->SetLayoutManager(std::make_unique<views::GridLayout>());

  views::ColumnSet* columns = layout->AddColumnSet(0);
  // A column for the section name.
  constexpr int kNameColumnWidth = 112;
  columns->AddColumn(views::GridLayout::LEADING, vertical_alignment,
                     views::GridLayout::kFixedSize,
                     views::GridLayout::ColumnSize::kFixed, kNameColumnWidth,
                     0);

  constexpr int kPaddingAfterName = 32;
  columns->AddPaddingColumn(views::GridLayout::kFixedSize, kPaddingAfterName);

  // A column for the content.
  columns->AddColumn(views::GridLayout::FILL, vertical_alignment, 1.0,
                     views::GridLayout::ColumnSize::kUsePreferred, 0, 0);
  // A column for the extra content.
  columns->AddColumn(views::GridLayout::TRAILING, views::GridLayout::CENTER,
                     views::GridLayout::kFixedSize,
                     views::GridLayout::ColumnSize::kUsePreferred, 0, 0);

  constexpr int kPaddingColumnsWidth = 25;
  columns->AddPaddingColumn(views::GridLayout::kFixedSize,
                            kPaddingColumnsWidth);
  // A column for the trailing_button.
  columns->AddColumn(views::GridLayout::TRAILING, views::GridLayout::CENTER,
                     views::GridLayout::kFixedSize,
                     views::GridLayout::ColumnSize::kUsePreferred, 0, 0);

  layout->StartRow(views::GridLayout::kFixedSize, 0);
  std::unique_ptr<views::Label> name_label = CreateMediumLabel(section_name);
  name_label->SetMultiLine(true);
  name_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  layout->AddView(std::move(name_label));

  if (content_view) {
    content_view->SetCanProcessEventsWithinSubtree(false);
    layout->AddView(std::move(content_view));
  } else {
    layout->SkipColumns(1);
  }

  if (extra_content_view) {
    extra_content_view->SetCanProcessEventsWithinSubtree(false);
    layout->AddView(std::move(extra_content_view));
  } else {
    layout->SkipColumns(1);
  }

  layout->AddView(std::move(trailing_button));

  row->SetAccessibleName(
      l10n_util::GetStringFUTF16(IDS_PAYMENTS_ROW_ACCESSIBLE_NAME_FORMAT,
                                 section_name, accessible_content));

  return row;
}

std::unique_ptr<views::View> CreateInlineCurrencyAmountItem(
    const std::u16string& currency,
    const std::u16string& amount,
    bool hint_color,
    bool bold) {
  std::unique_ptr<views::View> item_amount_line =
      std::make_unique<views::View>();
  views::GridLayout* item_amount_layout =
      item_amount_line->SetLayoutManager(std::make_unique<views::GridLayout>());
  views::ColumnSet* item_amount_columns = item_amount_layout->AddColumnSet(0);
  item_amount_columns->AddColumn(
      views::GridLayout::LEADING, views::GridLayout::LEADING,
      views::GridLayout::kFixedSize,
      views::GridLayout::ColumnSize::kUsePreferred, 0, 0);
  item_amount_columns->AddColumn(
      views::GridLayout::TRAILING, views::GridLayout::LEADING, 1.0,
      views::GridLayout::ColumnSize::kUsePreferred, 0, 0);

  DCHECK(!bold || !hint_color);
  std::unique_ptr<views::Label> currency_label;
  if (bold)
    currency_label = CreateBoldLabel(currency);
  else if (hint_color)
    currency_label = CreateHintLabel(currency);
  else
    currency_label = std::make_unique<views::Label>(currency);

  std::unique_ptr<views::Label> amount_label =
      bold ? CreateBoldLabel(amount) : std::make_unique<views::Label>(amount);
  amount_label->SetMultiLine(true);
  amount_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  amount_label->SetAllowCharacterBreak(true);

  item_amount_layout->StartRow(views::GridLayout::kFixedSize, 0);
  item_amount_layout->AddView(std::move(currency_label));
  item_amount_layout->AddView(std::move(amount_label));

  return item_amount_line;
}

// A class used to build Payment Sheet Rows. Construct an instance of it, chain
// calls to argument-setting functions, then call one of the CreateWith*
// functions to create the row view.
class PaymentSheetRowBuilder {
 public:
  PaymentSheetRowBuilder(PaymentSheetViewController* controller,
                         const std::u16string& section_name)
      : controller_(controller), section_name_(section_name) {}

  PaymentSheetRowBuilder& Closure(base::RepeatingClosure closure) {
    closure_ = std::move(closure);
    return *this;
  }

  PaymentSheetRowBuilder& Id(DialogViewID id) {
    id_ = static_cast<int>(id);
    return *this;
  }

  PaymentSheetRowBuilder& AccessibleContent(
      const std::u16string& accessible_content) {
    accessible_content_ = accessible_content;
    return *this;
  }

  // Creates a clickable row to be displayed in the Payment Sheet. It contains
  // a section name and some content, followed by a chevron as a clickability
  // affordance. Both, either, or none of |content_view| and
  // |extra_content_view| may be present, the difference between the two being
  // that content is pinned to the left and extra_content is pinned to the
  // right. The row also displays a light gray horizontal ruler on its lower
  // boundary. The name column has a fixed width equal to |name_column_width|.
  // +----------------------------+
  // | Name | Content | Extra | > |
  // +~~~~~~~~~~~~~~~~~~~~~~~~~~~~+ <-- ruler
  std::unique_ptr<PaymentRequestRowView> CreateWithChevron(
      std::unique_ptr<views::View> content_view,
      std::unique_ptr<views::View> extra_content_view) {
    auto chevron = std::make_unique<views::ColorTrackingIconView>(
        views::kSubmenuArrowIcon,
        gfx::GetDefaultSizeOfVectorIcon(views::kSubmenuArrowIcon));
    chevron->SetCanProcessEventsWithinSubtree(false);
    std::unique_ptr<PaymentRequestRowView> section = CreatePaymentSheetRow(
        GetPressedCallback(), section_name_, accessible_content_,
        std::move(content_view), std::move(extra_content_view),
        std::move(chevron), /*clickable=*/true, /*extra_trailing_inset=*/true);
    section->SetID(id_);
    return section;
  }

  // Creates a row with a button in place of the chevron and |truncated_content|
  // between |section_name| and the button.
  // +------------------------------------------+
  // | Name | truncated_content | button_string |
  // +~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~+
  std::unique_ptr<PaymentRequestRowView> CreateWithButton(
      const std::u16string& truncated_content,
      const std::u16string& button_string,
      bool button_enabled) {
    return CreateWithButton(CreateHintLabel(truncated_content, gfx::ALIGN_LEFT),
                            button_string, button_enabled);
  }

  // Creates a row with a button in place of the chevron with the string between
  // |section_name| and the button built as "|preview|... and |n| more".
  // |format_string| is used to assemble the truncated preview and the rest of
  // the content string.
  // +----------------------------------------------+
  // | Name | preview... and N more | button_string |
  // +~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~+
  std::unique_ptr<PaymentRequestRowView> CreateWithButton(
      const std::u16string& preview_text,
      const std::u16string& format_string,
      int n,
      const std::u16string& button_string,
      bool button_enabled) {
    DCHECK(accessible_content_.empty());
    std::unique_ptr<PreviewEliderLabel> content_view =
        std::make_unique<PreviewEliderLabel>(preview_text, format_string, n,
                                             views::style::STYLE_HINT);
    content_view->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    return CreateWithButton(std::move(content_view), button_string,
                            button_enabled);
  }

 private:
  // Creates a row with a button in place of the chevron.
  // +------------------------------------------+
  // | Name | content_view      | button_string |
  // +~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~+
  std::unique_ptr<PaymentRequestRowView> CreateWithButton(
      std::unique_ptr<views::View> content_view,
      const std::u16string& button_string,
      bool button_enabled) {
    auto button = std::make_unique<views::MdTextButton>(GetPressedCallback(),
                                                        button_string);
    button->SetProminent(true);
    button->SetID(id_);
    button->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
    button->SetEnabled(button_enabled);
    return CreatePaymentSheetRow(
        views::Button::PressedCallback(), section_name_, accessible_content_,
        std::move(content_view), nullptr, std::move(button),
        /*clickable=*/false,
        /*extra_trailing_inset=*/false, views::GridLayout::CENTER);
  }

  views::Button::PressedCallback GetPressedCallback() const {
    return base::BindRepeating(&PaymentSheetViewController::ButtonPressed,
                               base::Unretained(controller_), closure_);
  }

  PaymentSheetViewController* const controller_;
  std::u16string section_name_;
  std::u16string accessible_content_;
  base::RepeatingClosure closure_;
  int id_;
  DISALLOW_COPY_AND_ASSIGN(PaymentSheetRowBuilder);
};

}  // namespace

PaymentSheetViewController::PaymentSheetViewController(
    base::WeakPtr<PaymentRequestSpec> spec,
    base::WeakPtr<PaymentRequestState> state,
    base::WeakPtr<PaymentRequestDialogView> dialog)
    : PaymentRequestSheetController(spec, state, dialog) {
  DCHECK(spec);
  DCHECK(state);
  spec->AddObserver(this);
  state->AddObserver(this);
}

PaymentSheetViewController::~PaymentSheetViewController() {
  if (spec())
    spec()->RemoveObserver(this);

  state()->RemoveObserver(this);
}

void PaymentSheetViewController::OnSpecUpdated() {
  UpdateContentView();
}

void PaymentSheetViewController::OnSelectedInformationChanged() {
  primary_button()->SetText(GetPrimaryButtonLabel());
  primary_button()->SetEnabled(GetPrimaryButtonEnabled());
  UpdateContentView();
}

void PaymentSheetViewController::ButtonPressed(base::RepeatingClosure closure) {
  if (!dialog()->IsInteractive() || !spec())
    return;

  std::move(closure).Run();

  if (!spec()->retry_error_message().empty()) {
    spec()->reset_retry_error_message();
    UpdateContentView();
  }
}

std::u16string PaymentSheetViewController::GetSecondaryButtonLabel() {
  return l10n_util::GetStringUTF16(IDS_CANCEL);
}

bool PaymentSheetViewController::ShouldShowHeaderBackArrow() {
  return false;
}

std::u16string PaymentSheetViewController::GetSheetTitle() {
  return l10n_util::GetStringUTF16(IDS_PAYMENTS_TITLE);
}

void PaymentSheetViewController::FillContentView(views::View* content_view) {
  if (!spec())
    return;

  views::GridLayout* layout =
      content_view->SetLayoutManager(std::make_unique<views::GridLayout>());
  views::ColumnSet* columns = layout->AddColumnSet(0);
  columns->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER, 1.0,
                     views::GridLayout::ColumnSize::kUsePreferred, 0, 0);

  if (!spec()->retry_error_message().empty()) {
    std::unique_ptr<views::View> warning_view =
        CreateWarningView(spec()->retry_error_message(), true /* show_icon */);
    layout->StartRow(views::GridLayout::kFixedSize, 0);
    layout->AddView(std::move(warning_view));
  }

  // The shipping address and contact info rows are optional.
  std::unique_ptr<PaymentRequestRowView> summary_row =
      CreatePaymentSheetSummaryRow();
  if (!summary_row)
    return;

  PaymentRequestRowView* previous_row = summary_row.get();
  layout->StartRow(views::GridLayout::kFixedSize, 0);
  layout->AddView(std::move(summary_row));

  if (state()->ShouldShowShippingSection()) {
    std::unique_ptr<PaymentRequestRowView> shipping_row = CreateShippingRow();
    if (!shipping_row)
      return;

    shipping_row->set_previous_row(previous_row->AsWeakPtr());
    previous_row = shipping_row.get();
    layout->StartRow(views::GridLayout::kFixedSize, 0);
    layout->AddView(std::move(shipping_row));
    // It's possible for requestShipping to be true and for there to be no
    // shipping options yet (they will come in updateWith).
    // TODO(crbug.com/707353): Put a better placeholder row, instead of no row.
    std::unique_ptr<PaymentRequestRowView> shipping_option_row =
        CreateShippingOptionRow();
    if (shipping_option_row) {
      shipping_option_row->set_previous_row(previous_row->AsWeakPtr());
      previous_row = shipping_option_row.get();
      layout->StartRow(views::GridLayout::kFixedSize, 0);
      layout->AddView(std::move(shipping_option_row));
    }
  }
  std::unique_ptr<PaymentRequestRowView> payment_method_row =
      CreatePaymentMethodRow();
  payment_method_row->set_previous_row(previous_row->AsWeakPtr());
  previous_row = payment_method_row.get();
  layout->StartRow(views::GridLayout::kFixedSize, 0);
  layout->AddView(std::move(payment_method_row));
  if (state()->ShouldShowContactSection()) {
    std::unique_ptr<PaymentRequestRowView> contact_info_row =
        CreateContactInfoRow();
    contact_info_row->set_previous_row(previous_row->AsWeakPtr());
    previous_row = contact_info_row.get();
    layout->StartRow(views::GridLayout::kFixedSize, 0);
    layout->AddView(std::move(contact_info_row));
  }
  layout->StartRow(views::GridLayout::kFixedSize, 0);
  layout->AddView(CreateDataSourceRow());
}

// Adds the product logo to the footer.
// +---------------------------------------------------------+
// | (•) chrome                               | PAY | CANCEL |
// +---------------------------------------------------------+
std::unique_ptr<views::View>
PaymentSheetViewController::CreateExtraFooterView() {
  return CreateProductLogoFooterView();
}

// Creates the Order Summary row, which contains an "Order Summary" label,
// an inline list of display items, a Total Amount label, and a Chevron. Returns
// nullptr if WeakPtr<PaymentRequestSpec> has become null.
// +----------------------------------------------+
// | Order Summary   Item 1            $ 1.34     |
// |                 Item 2            $ 2.00   > |
// |                 2 more items...              |
// |                 Total         USD $12.34     |
// +----------------------------------------------+
std::unique_ptr<PaymentRequestRowView>
PaymentSheetViewController::CreatePaymentSheetSummaryRow() {
  if (!spec())
    return nullptr;

  std::unique_ptr<views::View> inline_summary = std::make_unique<views::View>();
  views::GridLayout* layout =
      inline_summary->SetLayoutManager(std::make_unique<views::GridLayout>());
  views::ColumnSet* columns = layout->AddColumnSet(0);
  columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::LEADING,
                     1.0, views::GridLayout::ColumnSize::kUsePreferred, 0, 0);
  constexpr int kItemSummaryPriceFixedWidth = 96;
  columns->AddColumn(views::GridLayout::FILL, views::GridLayout::LEADING,
                     views::GridLayout::kFixedSize,
                     views::GridLayout::ColumnSize::kFixed,
                     kItemSummaryPriceFixedWidth, kItemSummaryPriceFixedWidth);

  const std::vector<const mojom::PaymentItemPtr*>& items =
      spec()->GetDisplayItems(state()->selected_app());

  bool is_mixed_currency = spec()->IsMixedCurrency();
  // The inline items section contains the first 2 display items of the
  // request's details, followed by a label indicating "N more items..." if
  // there are more than 2 items in the details. The total label and amount
  // always follow.
  constexpr size_t kMaxNumberOfItemsShown = 2;
  // Don't show a line reading "1 more" because the item itself can be shown in
  // the same space.
  size_t displayed_items = items.size() <= kMaxNumberOfItemsShown + 1
                               ? items.size()
                               : kMaxNumberOfItemsShown;
  for (size_t i = 0; i < items.size() && i < displayed_items; ++i) {
    layout->StartRow(views::GridLayout::kFixedSize, 0);
    std::unique_ptr<views::Label> summary =
        std::make_unique<views::Label>(base::UTF8ToUTF16((*items[i])->label));
    summary->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    layout->AddView(std::move(summary));

    layout->AddView(CreateInlineCurrencyAmountItem(
        is_mixed_currency ? base::UTF8ToUTF16(spec()->GetFormattedCurrencyCode(
                                (*items[i])->amount))
                          : std::u16string(),
        spec()->GetFormattedCurrencyAmount((*items[i])->amount), true, false));
  }

  size_t hidden_item_count = items.size() - displayed_items;
  if (hidden_item_count > 0) {
    layout->StartRow(views::GridLayout::kFixedSize, 0);
    std::unique_ptr<views::Label> label =
        CreateHintLabel(l10n_util::GetPluralStringFUTF16(
            IDS_PAYMENT_REQUEST_ORDER_SUMMARY_MORE_ITEMS, hidden_item_count));
    layout->AddView(std::move(label));
    if (is_mixed_currency) {
      std::unique_ptr<views::Label> multiple_currency_label =
          CreateHintLabel(l10n_util::GetStringUTF16(
              IDS_PAYMENT_REQUEST_ORDER_SUMMARY_MULTIPLE_CURRENCY_INDICATOR));
      layout->AddView(std::move(multiple_currency_label));
    }
  }

  layout->StartRow(views::GridLayout::kFixedSize, 0);
  PaymentApp* selected_app = state()->selected_app();
  const mojom::PaymentItemPtr& total = spec()->GetTotal(selected_app);
  std::u16string total_label_text = base::UTF8ToUTF16(total->label);
  std::unique_ptr<views::Label> total_label = CreateBoldLabel(total_label_text);
  layout->AddView(std::move(total_label));

  std::u16string total_currency_code =
      base::UTF8ToUTF16(spec()->GetFormattedCurrencyCode(
          spec()->GetTotal(state()->selected_app())->amount));
  std::u16string total_amount = spec()->GetFormattedCurrencyAmount(
      spec()->GetTotal(state()->selected_app())->amount);
  layout->AddView(CreateInlineCurrencyAmountItem(total_currency_code,
                                                 total_amount, false, true));

  PaymentSheetRowBuilder builder(
      this, l10n_util::GetStringUTF16(IDS_PAYMENTS_ORDER_SUMMARY_LABEL));
  builder
      .Closure(base::BindRepeating(&PaymentRequestDialogView::ShowOrderSummary,
                                   dialog()))
      .Id(DialogViewID::PAYMENT_SHEET_SUMMARY_SECTION)
      .AccessibleContent(l10n_util::GetStringFUTF16(
          IDS_PAYMENTS_ORDER_SUMMARY_ACCESSIBLE_LABEL,
          l10n_util::GetStringFUTF16(
              IDS_PAYMENT_REQUEST_ORDER_SUMMARY_SECTION_TOTAL_FORMAT,
              total_label_text, total_currency_code, total_amount)));

  return builder.CreateWithChevron(std::move(inline_summary), nullptr);
}

std::unique_ptr<views::View>
PaymentSheetViewController::CreateShippingSectionContent(
    std::u16string* accessible_content) {
  DCHECK(accessible_content);
  autofill::AutofillProfile* profile = state()->selected_shipping_profile();
  if (!profile)
    return std::make_unique<views::Label>(std::u16string());

  return GetShippingAddressLabelWithMissingInfo(
      AddressStyleType::SUMMARY, state()->GetApplicationLocale(), *profile,
      *(state()->profile_comparator()), accessible_content);
}

// Creates the Shipping row, which contains a "Shipping address" label, the
// user's selected shipping address, and a chevron. Returns null if the
// WeakPtr<PaymentRequestSpec> has become null.
// +----------------------------------------------+
// | Shipping Address   Barack Obama              |
// |                    1600 Pennsylvania Ave.  > |
// |                    1800MYPOTUS               |
// +----------------------------------------------+
std::unique_ptr<PaymentRequestRowView>
PaymentSheetViewController::CreateShippingRow() {
  if (!spec())
    return nullptr;

  std::unique_ptr<views::Button> section;
  PaymentSheetRowBuilder builder(
      this, GetShippingAddressSectionString(spec()->shipping_type()));
  builder
      .Id(state()->selected_shipping_profile()
              ? DialogViewID::PAYMENT_SHEET_SHIPPING_ADDRESS_SECTION
              : DialogViewID::PAYMENT_SHEET_SHIPPING_ADDRESS_SECTION_BUTTON)
      .Closure(state()->shipping_profiles().empty()
                   ? base::BindRepeating(
                         &PaymentSheetViewController::AddShippingButtonPressed,
                         base::Unretained(this))
                   : base::BindRepeating(
                         &PaymentRequestDialogView::ShowShippingProfileSheet,
                         dialog()));
  if (state()->selected_shipping_profile()) {
    std::u16string accessible_content;
    std::unique_ptr<views::View> content =
        CreateShippingSectionContent(&accessible_content);
    return builder.AccessibleContent(accessible_content)
        .CreateWithChevron(std::move(content), nullptr);
  }
  if (state()->shipping_profiles().empty()) {
    return builder.CreateWithButton(std::u16string(),
                                    l10n_util::GetStringUTF16(IDS_ADD),
                                    /*button_enabled=*/true);
  }
  const std::u16string label = GetShippingAddressLabelFromAutofillProfile(
      *state()->shipping_profiles()[0], state()->GetApplicationLocale());
  if (state()->shipping_profiles().size() == 1) {
    return builder.CreateWithButton(label,
                                    l10n_util::GetStringUTF16(IDS_CHOOSE),
                                    /*button_enabled=*/true);
  }
  std::u16string format = l10n_util::GetPluralStringFUTF16(
      IDS_PAYMENT_REQUEST_SHIPPING_ADDRESSES_PREVIEW,
      state()->shipping_profiles().size() - 1);
  return builder.CreateWithButton(label, format,
                                  state()->shipping_profiles().size() - 1,
                                  l10n_util::GetStringUTF16(IDS_CHOOSE),
                                  /*button_enabled=*/true);
}

// Creates the Payment Method row, which contains a "Payment" label, the user's
// masked Credit Card details, the icon for the selected card, and a chevron.
// If no option is selected or none is available, the Chevron and icon are
// replaced with a button
// +----------------------------------------------+
// | Payment         Visa ****0000                |
// |                 John Smith        | VISA | > |
// |                                              |
// +----------------------------------------------+
std::unique_ptr<PaymentRequestRowView>
PaymentSheetViewController::CreatePaymentMethodRow() {
  PaymentApp* selected_app = state()->selected_app();

  PaymentSheetRowBuilder builder(
      this, l10n_util::GetStringUTF16(
                IDS_PAYMENT_REQUEST_PAYMENT_METHOD_SECTION_NAME));
  builder
      .Id(selected_app
              ? DialogViewID::PAYMENT_SHEET_PAYMENT_METHOD_SECTION
              : DialogViewID::PAYMENT_SHEET_PAYMENT_METHOD_SECTION_BUTTON)
      .Closure(
          state()->available_apps().empty()
              ? base::BindRepeating(
                    &PaymentSheetViewController::AddPaymentMethodButtonPressed,
                    base::Unretained(this))
              : base::BindRepeating(
                    &PaymentRequestDialogView::ShowPaymentMethodSheet,
                    dialog()));
  if (selected_app) {
    auto content_view = std::make_unique<views::View>();

    views::GridLayout* layout =
        content_view->SetLayoutManager(std::make_unique<views::GridLayout>());
    views::ColumnSet* columns = layout->AddColumnSet(0);
    columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                       1.0, views::GridLayout::ColumnSize::kUsePreferred, 0, 0);

    layout->StartRow(views::GridLayout::kFixedSize, 0);
    layout->AddView(std::make_unique<views::Label>(selected_app->GetLabel()))
        ->SetHorizontalAlignment(gfx::ALIGN_LEFT);

    layout->StartRow(views::GridLayout::kFixedSize, 0);
    layout->AddView(std::make_unique<views::Label>(selected_app->GetSublabel()))
        ->SetHorizontalAlignment(gfx::ALIGN_LEFT);

    std::unique_ptr<views::ImageView> icon_view = CreateAppIconView(
        selected_app->icon_resource_id(), selected_app->icon_bitmap(),
        selected_app->GetLabel());

    return builder.AccessibleContent(selected_app->GetLabel())
        .CreateWithChevron(std::move(content_view), std::move(icon_view));
  }
  if (state()->available_apps().empty()) {
    return builder.CreateWithButton(std::u16string(),
                                    l10n_util::GetStringUTF16(IDS_ADD),
                                    /*button_enabled=*/true);
  }
  const std::u16string label = state()->available_apps()[0]->GetLabel();
  if (state()->available_apps().size() == 1) {
    return builder.CreateWithButton(label,
                                    l10n_util::GetStringUTF16(IDS_CHOOSE),
                                    /*button_enabled=*/true);
  }
  std::u16string format = l10n_util::GetPluralStringFUTF16(
      IDS_PAYMENT_REQUEST_PAYMENT_METHODS_PREVIEW,
      state()->available_apps().size() - 1);
  return builder.CreateWithButton(label, format,
                                  state()->available_apps().size() - 1,
                                  l10n_util::GetStringUTF16(IDS_CHOOSE),
                                  /*button_enabled=*/true);
}

std::unique_ptr<views::View>
PaymentSheetViewController::CreateContactInfoSectionContent(
    std::u16string* accessible_content) {
  autofill::AutofillProfile* profile = state()->selected_contact_profile();
  *accessible_content = std::u16string();
  return profile && spec()
             ? payments::GetContactInfoLabel(
                   AddressStyleType::SUMMARY, state()->GetApplicationLocale(),
                   *profile, spec()->request_payer_name(),
                   spec()->request_payer_email(), spec()->request_payer_phone(),
                   *(state()->profile_comparator()), accessible_content)
             : std::make_unique<views::Label>(std::u16string());
}

// Creates the Contact Info row, which contains a "Contact info" label; the
// name, email address, and/or phone number; and a chevron.
// +----------------------------------------------+
// | Contact info       Barack Obama              |
// |                    1800MYPOTUS             > |
// |                    potus@whitehouse.gov      |
// +----------------------------------------------+
std::unique_ptr<PaymentRequestRowView>
PaymentSheetViewController::CreateContactInfoRow() {
  PaymentSheetRowBuilder builder(
      this,
      l10n_util::GetStringUTF16(IDS_PAYMENT_REQUEST_CONTACT_INFO_SECTION_NAME));
  builder
      .Id(state()->selected_contact_profile()
              ? DialogViewID::PAYMENT_SHEET_CONTACT_INFO_SECTION
              : DialogViewID::PAYMENT_SHEET_CONTACT_INFO_SECTION_BUTTON)
      .Closure(
          state()->contact_profiles().empty()
              ? base::BindRepeating(
                    &PaymentSheetViewController::AddContactInfoButtonPressed,
                    base::Unretained(this))
              : base::BindRepeating(
                    &PaymentRequestDialogView::ShowContactProfileSheet,
                    dialog()));
  if (state()->selected_contact_profile()) {
    std::u16string accessible_content;
    std::unique_ptr<views::View> content =
        CreateContactInfoSectionContent(&accessible_content);
    return builder.AccessibleContent(accessible_content)
        .CreateWithChevron(std::move(content), nullptr);
  }
  if (state()->contact_profiles().empty()) {
    return builder.CreateWithButton(std::u16string(),
                                    l10n_util::GetStringUTF16(IDS_ADD),
                                    /*button_enabled=*/true);
  }
  static constexpr autofill::ServerFieldType kLabelFields[] = {
      autofill::NAME_FULL, autofill::PHONE_HOME_WHOLE_NUMBER,
      autofill::EMAIL_ADDRESS};
  const std::u16string preview =
      state()->contact_profiles()[0]->ConstructInferredLabel(
          kLabelFields, base::size(kLabelFields), base::size(kLabelFields),
          state()->GetApplicationLocale());
  if (state()->contact_profiles().size() == 1) {
    return builder.CreateWithButton(preview,
                                    l10n_util::GetStringUTF16(IDS_CHOOSE),
                                    /*button_enabled=*/true);
  }
  std::u16string format =
      l10n_util::GetPluralStringFUTF16(IDS_PAYMENT_REQUEST_CONTACTS_PREVIEW,
                                       state()->contact_profiles().size() - 1);
  return builder.CreateWithButton(
      preview, format, state()->contact_profiles().size() - 1,
      l10n_util::GetStringUTF16(IDS_CHOOSE), /*button_enabled=*/true);
}

std::unique_ptr<PaymentRequestRowView>
PaymentSheetViewController::CreateShippingOptionRow() {
  // The Shipping Options row has many different ways of being displayed
  // depending on the state of the dialog and Payment Request.
  // 1. There is a selected shipping address. The website updated the shipping
  //    options.
  //    1.1 There are no available shipping options: don't display the row.
  //    1.2 There are options and one is selected: display the row with the
  //        selection's label and a chevron.
  //    1.3 There are options and none is selected: display a row with a
  //        choose button and the string "|preview of first option| and N more"
  // 2. There is no selected shipping address: do not display the row.
  if (!spec() || spec()->GetShippingOptions().empty() ||
      !state()->selected_shipping_profile()) {
    // 1.1 No shipping options, do not display the row.  (or)
    // 2. There is no selected address: do not show the shipping option section.
    return nullptr;
  }

  // The shipping option section displays the currently selected option if there
  // is one. It's not possible to select an option without selecting an address
  // first.
  PaymentSheetRowBuilder builder(
      this, GetShippingOptionSectionString(spec()->shipping_type()));
  builder.Closure(base::BindRepeating(
      &PaymentRequestDialogView::ShowShippingOptionSheet, dialog()));

  mojom::PaymentShippingOption* selected_option =
      spec()->selected_shipping_option();
  if (selected_option) {
    // 1.2 Show the selected shipping option.
    std::u16string accessible_content;
    std::unique_ptr<views::View> option_row_content = CreateShippingOptionLabel(
        selected_option,
        spec()->GetFormattedCurrencyAmount(selected_option->amount),
        /*emphasize_label=*/false, &accessible_content);
    return builder.Id(DialogViewID::PAYMENT_SHEET_SHIPPING_OPTION_SECTION)
        .AccessibleContent(accessible_content)
        .CreateWithChevron(std::move(option_row_content), nullptr);
  }
  // 1.3 There are options, none are selected: show the enabled Choose
  // button.
  const auto& shipping_options = spec()->GetShippingOptions();
  return builder.Id(DialogViewID::PAYMENT_SHEET_SHIPPING_OPTION_SECTION_BUTTON)
      .CreateWithButton(base::UTF8ToUTF16(shipping_options[0]->label),
                        l10n_util::GetPluralStringFUTF16(
                            IDS_PAYMENT_REQUEST_SHIPPING_OPTIONS_PREVIEW,
                            shipping_options.size() - 1),
                        shipping_options.size() - 1,
                        l10n_util::GetStringUTF16(IDS_CHOOSE),
                        /*button_enabled=*/true);
}

std::unique_ptr<views::View> PaymentSheetViewController::CreateDataSourceRow() {
  std::unique_ptr<views::View> content_view = std::make_unique<views::View>();
  auto layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets(0, kPaymentRequestRowHorizontalInsets));
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kStart);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);
  content_view->SetLayoutManager(std::move(layout));

  std::u16string data_source;
  // If no transaction has been completed so far, choose which string to display
  // as a function of the profile's signed in state. Otherwise, always show the
  // same string.
  bool first_transaction_completed =
      dialog()->GetProfile()->GetPrefs()->GetBoolean(
          payments::kPaymentsFirstTransactionCompleted);
  if (first_transaction_completed) {
    data_source =
        l10n_util::GetStringUTF16(IDS_PAYMENTS_CARD_AND_ADDRESS_SETTINGS);
  } else {
    std::string user_email = state()->GetAuthenticatedEmail();
    if (!user_email.empty()) {
      // Insert the user's email into the format string.
      data_source = l10n_util::GetStringFUTF16(
          IDS_PAYMENTS_CARD_AND_ADDRESS_SETTINGS_SIGNED_IN,
          base::UTF8ToUTF16(user_email));
    } else {
      data_source = l10n_util::GetStringUTF16(
          IDS_PAYMENTS_CARD_AND_ADDRESS_SETTINGS_SIGNED_OUT);
    }
  }

  // The translated string will surround the actual "Settings" substring with
  // BEGIN_LINK and END_LINK. Find the beginning of the link range and the
  // length of the "settings" part, then remove the BEGIN_LINK and END_LINK
  // parts and linkify "settings".
  // TODO(pkasting): Remove these BEGIN/END_LINK tags and use a substitution for
  // "Settings", allowing this code to use the offset-returning versions of the
  // l10n getters.
  std::u16string begin_tag = u"BEGIN_LINK";
  std::u16string end_tag = u"END_LINK";
  size_t link_begin = data_source.find(begin_tag);
  DCHECK(link_begin != std::u16string::npos);

  size_t link_end = data_source.find(end_tag);
  DCHECK(link_end != std::u16string::npos);

  size_t link_length = link_end - link_begin - begin_tag.size();
  data_source.erase(link_end, end_tag.size());
  data_source.erase(link_begin, begin_tag.size());

  auto data_source_label = std::make_unique<views::StyledLabel>();
  data_source_label->SetText(data_source);

  data_source_label->SetBorder(views::CreateEmptyBorder(22, 0, 0, 0));
  data_source_label->SetID(static_cast<int>(DialogViewID::DATA_SOURCE_LABEL));
  data_source_label->SetDefaultTextStyle(views::style::STYLE_DISABLED);

  views::StyledLabel::RangeStyleInfo link_style =
      views::StyledLabel::RangeStyleInfo::CreateForLink(base::BindRepeating(
          [](base::WeakPtr<PaymentRequestDialogView> dialog) {
            if (dialog->IsInteractive()) {
              chrome::ShowSettingsSubPageForProfile(dialog->GetProfile(),
                                                    chrome::kPaymentsSubPage);
            }
          },
          dialog()));

  // TODO(pbos): Investigate whether this override is necessary.
  link_style.override_color = gfx::kGoogleBlue700;

  data_source_label->AddStyleRange(
      gfx::Range(link_begin, link_begin + link_length), link_style);
  data_source_label->SizeToFit(0);
  content_view->AddChildView(data_source_label.release());
  return content_view;
}

void PaymentSheetViewController::AddShippingButtonPressed() {
  dialog()->ShowShippingAddressEditor(
      BackNavigationType::kPaymentSheet, base::RepeatingClosure(),
      base::BindRepeating(&PaymentRequestState::AddAutofillShippingProfile,
                          state(), true),
      nullptr);
}

void PaymentSheetViewController::AddPaymentMethodButtonPressed() {
  dialog()->ShowCreditCardEditor(
      BackNavigationType::kPaymentSheet, base::RepeatingClosure(),
      base::BindRepeating(&PaymentRequestState::AddAutofillPaymentApp, state(),
                          true),
      nullptr);
}

void PaymentSheetViewController::AddContactInfoButtonPressed() {
  dialog()->ShowContactInfoEditor(
      BackNavigationType::kPaymentSheet, base::RepeatingClosure(),
      base::BindRepeating(&PaymentRequestState::AddAutofillContactProfile,
                          state(), true),
      nullptr);
}

}  // namespace payments
