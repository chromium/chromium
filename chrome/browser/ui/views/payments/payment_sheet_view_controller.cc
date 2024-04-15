// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/payment_sheet_view_controller.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/i18n/message_formatter.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
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
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/color/color_id.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/font.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/range/range.h"
#include "ui/gfx/text_elider.h"
#include "ui/gfx/text_utils.h"
#include "ui/gfx/vector_icon_utils.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/table_layout.h"
#include "ui/views/layout/table_layout_view.h"
#include "ui/views/view.h"

namespace payments {
namespace {

// A class that ensures proper elision of labels in the form
// "[preview] and N more" where preview might be elided to allow "and N more" to
// be always visible.
class PreviewEliderLabel : public views::Label {
  METADATA_HEADER(PreviewEliderLabel, views::Label)

 public:
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

    // TODO(crbug.com/40517112): Display something meaningful if the preview
    // can't be elided enough for the string to fit.
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

BEGIN_METADATA(PreviewEliderLabel)
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
    views::LayoutAlignment vertical_alignment =
        views::LayoutAlignment::kStart) {
  constexpr int kNameColumnWidth = 112;
  constexpr int kPaddingAfterName = 32;
  constexpr int kPaddingColumnsWidth = 25;

  auto table_layout = std::make_unique<views::TableLayout>();
  table_layout
      // A column for the section name.
      ->AddColumn(views::LayoutAlignment::kStart, vertical_alignment,
                  views::TableLayout::kFixedSize,
                  views::TableLayout::ColumnSize::kFixed, kNameColumnWidth, 0)
      .AddPaddingColumn(views::TableLayout::kFixedSize, kPaddingAfterName)
      // A column for the content.
      .AddColumn(views::LayoutAlignment::kStretch, vertical_alignment, 1.0,
                 views::TableLayout::ColumnSize::kUsePreferred, 0, 0)
      // A column for the extra content.
      .AddColumn(views::LayoutAlignment::kEnd, views::LayoutAlignment::kCenter,
                 views::TableLayout::kFixedSize,
                 views::TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddPaddingColumn(views::TableLayout::kFixedSize, kPaddingColumnsWidth)
      // A column for the trailing_button.
      .AddColumn(views::LayoutAlignment::kEnd, views::LayoutAlignment::kCenter,
                 views::TableLayout::kFixedSize,
                 views::TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddRows(1, views::TableLayout::kFixedSize);

  const int trailing_inset = extra_trailing_inset
                                 ? kPaymentRequestRowHorizontalInsets +
                                       kPaymentRequestRowExtraRightInset
                                 : kPaymentRequestRowHorizontalInsets;
  const auto row_insets = gfx::Insets::TLBR(
      kPaymentRequestRowVerticalInsets, kPaymentRequestRowHorizontalInsets,
      kPaymentRequestRowVerticalInsets, trailing_inset);

  return views::Builder<PaymentRequestRowView>()
      .SetLayoutManager(std::move(table_layout))
      .SetCallback(std::move(callback))
      .SetClickable(clickable)
      .SetRowInsets(row_insets)
      .SetAccessibleName(
          l10n_util::GetStringFUTF16(IDS_PAYMENTS_ROW_ACCESSIBLE_NAME_FORMAT,
                                     section_name, accessible_content))
      .AddChildren(
          views::Builder<views::Label>(CreateMediumLabel(section_name))
              .SetMultiLine(true)
              .SetHorizontalAlignment(gfx::ALIGN_LEFT),
          content_view ? views::Builder<views::View>(std::move(content_view))
                             .SetCanProcessEventsWithinSubtree(false)
                       : views::Builder<views::View>(),
          extra_content_view
              ? views::Builder<views::View>(std::move(extra_content_view))
                    .SetCanProcessEventsWithinSubtree(false)
              : views::Builder<views::View>(),
          views::Builder<views::View>(std::move(trailing_button)))
      .Build();
}

std::unique_ptr<views::View> CreateInlineCurrencyAmountItem(
    const std::u16string& currency,
    const std::u16string& amount,
    bool hint_color,
    bool bold) {
  DCHECK(!bold || !hint_color);
  return views::Builder<views::TableLayoutView>()
      .AddColumn(views::LayoutAlignment::kStart, views::LayoutAlignment::kStart,
                 views::TableLayout::kFixedSize,
                 views::TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddColumn(views::LayoutAlignment::kEnd, views::LayoutAlignment::kStart,
                 1.0, views::TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddRows(1, views::TableLayout::kFixedSize, 0)
      .AddChildren((bold ? views::Builder<views::Label>(CreateBoldLabel(u""))
                         : (hint_color ? views::Builder<views::Label>(
                                             CreateHintLabel(u""))
                                       : views::Builder<views::Label>()))
                       .SetText(currency),
                   (bold ? views::Builder<views::Label>(CreateBoldLabel(u""))
                         : views::Builder<views::Label>())
                       .SetText(amount)
                       .SetMultiLine(true)
                       .SetHorizontalAlignment(gfx::ALIGN_LEFT)
                       .SetAllowCharacterBreak(true))
      .Build();
}

// A class used to build Payment Sheet Rows. Construct an instance of it, chain
// calls to argument-setting functions, then call one of the CreateWith*
// functions to create the row view.
class PaymentSheetRowBuilder {
 public:
  PaymentSheetRowBuilder(PaymentSheetViewController* controller,
                         const std::u16string& section_name)
      : controller_(controller), section_name_(section_name) {}

  PaymentSheetRowBuilder(const PaymentSheetRowBuilder&) = delete;
  PaymentSheetRowBuilder& operator=(const PaymentSheetRowBuilder&) = delete;

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
    auto chevron =
        std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
            vector_icons::kSubmenuArrowIcon, ui::kColorIcon,
            gfx::GetDefaultSizeOfVectorIcon(vector_icons::kSubmenuArrowIcon)));
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
    button->SetStyle(ui::ButtonStyle::kProminent);
    button->SetID(id_);
    button->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
    button->SetEnabled(button_enabled);
    return CreatePaymentSheetRow(
        views::Button::PressedCallback(), section_name_, accessible_content_,
        std::move(content_view), nullptr, std::move(button),
        /*clickable=*/false,
        /*extra_trailing_inset=*/false, views::LayoutAlignment::kCenter);
  }

  views::Button::PressedCallback GetPressedCallback() const {
    return base::BindRepeating(&PaymentSheetViewController::ButtonPressed,
                               base::Unretained(controller_), closure_);
  }

  const raw_ptr<PaymentSheetViewController> controller_;
  std::u16string section_name_;
  std::u16string accessible_content_;
  base::RepeatingClosure closure_;
  int id_;
};

}  // namespace

PaymentSheetViewController::PaymentSheetViewController(
    base::WeakPtr<PaymentRequestSpec> spec,
    base::WeakPtr<PaymentRequestState> state,
    base::WeakPtr<PaymentRequestDialogView> dialog)
    : PaymentRequestSheetController(spec, state, dialog),
      input_protector_(
          std::make_unique<views::InputEventActivationProtector>()) {
  DCHECK(spec);
  DCHECK(state);
  spec->AddObserver(this);
  state->AddObserver(this);

  // This class is constructed as the view is being shown, so we mark it as
  // visible now. The view may become hidden again in the future (if the user
  // clicks into a sub-view), but we only need to defend the initial showing
  // against acccidental clicks on [Continue] and so this location suffices.
  input_protector_->VisibilityChanged(/*is_visible=*/true);
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

PaymentRequestSheetController::ButtonCallback
PaymentSheetViewController::GetPrimaryButtonCallback() {
  PaymentRequestSheetController::ButtonCallback parent_callback =
      PaymentRequestSheetController::GetPrimaryButtonCallback();
  return base::BindRepeating(
      &PaymentSheetViewController::PossiblyIgnorePrimaryButtonPress,
      weak_ptr_factory_.GetWeakPtr(), std::move(parent_callback));
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

  auto builder = views::Builder<views::View>(content_view)
                     .SetLayoutManager(std::make_unique<views::BoxLayout>(
                         views::BoxLayout::Orientation::kVertical));

  if (!spec()->retry_error_message().empty()) {
    builder.AddChild(views::Builder<views::View>(CreateWarningView(
        spec()->retry_error_message(), true /* show_icon */)));
  }

  // The shipping address and contact info rows are optional.
  std::unique_ptr<PaymentRequestRowView> summary_row =
      CreatePaymentSheetSummaryRow();
  if (!summary_row)
    return std::move(builder).BuildChildren();

  PaymentRequestRowView* previous_row = summary_row.get();
  builder.AddChild(views::Builder<views::View>(std::move(summary_row)));

  if (state()->ShouldShowShippingSection()) {
    std::unique_ptr<PaymentRequestRowView> shipping_row = CreateShippingRow();
    if (!shipping_row)
      return std::move(builder).BuildChildren();

    shipping_row->set_previous_row(previous_row->AsWeakPtr());
    previous_row = shipping_row.get();
    builder.AddChild(views::Builder<views::View>(std::move(shipping_row)));
    // It's possible for requestShipping to be true and for there to be no
    // shipping options yet (they will come in updateWith).
    // TODO(crbug.com/40513573): Put a better placeholder row, instead of no
    // row.
    std::unique_ptr<PaymentRequestRowView> shipping_option_row =
        CreateShippingOptionRow();
    if (shipping_option_row) {
      shipping_option_row->set_previous_row(previous_row->AsWeakPtr());
      previous_row = shipping_option_row.get();
      builder.AddChild(
          views::Builder<views::View>(std::move(shipping_option_row)));
    }
  }
  std::unique_ptr<PaymentRequestRowView> payment_method_row =
      CreatePaymentMethodRow();
  payment_method_row->set_previous_row(previous_row->AsWeakPtr());
  previous_row = payment_method_row.get();
  builder.AddChild(views::Builder<views::View>(std::move(payment_method_row)));
  if (state()->ShouldShowContactSection()) {
    std::unique_ptr<PaymentRequestRowView> contact_info_row =
        CreateContactInfoRow();
    contact_info_row->set_previous_row(previous_row->AsWeakPtr());
    previous_row = contact_info_row.get();
    builder.AddChild(views::Builder<views::View>(std::move(contact_info_row)));
  }
  builder.AddChild(views::Builder<views::View>(CreateDataSourceRow()));

  std::move(builder).BuildChildren();
}

// Adds the product logo to the footer.
// +---------------------------------------------------------+
// | (•) chrome                               | PAY | CANCEL |
// +---------------------------------------------------------+
std::unique_ptr<views::View>
PaymentSheetViewController::CreateExtraFooterView() {
  return CreateProductLogoFooterView();
}

bool PaymentSheetViewController::GetSheetId(DialogViewID* sheet_id) {
  *sheet_id = DialogViewID::PAYMENT_REQUEST_SHEET;
  return true;
}

base::WeakPtr<PaymentRequestSheetController>
PaymentSheetViewController::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
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

  constexpr int kItemSummaryPriceFixedWidth = 96;
  auto view_builder =
      views::Builder<views::TableLayoutView>()
          .AddColumn(views::LayoutAlignment::kStart,
                     views::LayoutAlignment::kStart, 1.0,
                     views::TableLayout::ColumnSize::kUsePreferred, 0, 0)
          .AddColumn(views::LayoutAlignment::kStretch,
                     views::LayoutAlignment::kStart,
                     views::TableLayout::kFixedSize,
                     views::TableLayout::ColumnSize::kFixed,
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
    view_builder.AddRows(1, views::TableLayout::kFixedSize, 0)
        .AddChildren(
            views::Builder<views::Label>()
                .SetText(base::UTF8ToUTF16((*items[i])->label))
                .SetHorizontalAlignment(gfx::ALIGN_LEFT),
            views::Builder<views::View>(CreateInlineCurrencyAmountItem(
                is_mixed_currency
                    ? base::UTF8ToUTF16(
                          spec()->GetFormattedCurrencyCode((*items[i])->amount))
                    : std::u16string(),
                spec()->GetFormattedCurrencyAmount((*items[i])->amount), true,
                false)));
  }

  size_t hidden_item_count = items.size() - displayed_items;
  if (hidden_item_count > 0) {
    view_builder.AddRows(1, views::TableLayout::kFixedSize, 0)
        .AddChildren(
            views::Builder<views::Label>(
                CreateHintLabel(l10n_util::GetPluralStringFUTF16(
                    IDS_PAYMENT_REQUEST_ORDER_SUMMARY_MORE_ITEMS,
                    hidden_item_count))),
            is_mixed_currency
                ? views::Builder<
                      views::View>(CreateHintLabel(l10n_util::GetStringUTF16(
                      IDS_PAYMENT_REQUEST_ORDER_SUMMARY_MULTIPLE_CURRENCY_INDICATOR)))
                : views::Builder<views::View>());
  }

  PaymentApp* selected_app = state()->selected_app();
  const mojom::PaymentItemPtr& total = spec()->GetTotal(selected_app);
  std::u16string total_label_text = base::UTF8ToUTF16(total->label);
  std::u16string total_currency_code =
      base::UTF8ToUTF16(spec()->GetFormattedCurrencyCode(
          spec()->GetTotal(state()->selected_app())->amount));
  std::u16string total_amount = spec()->GetFormattedCurrencyAmount(
      spec()->GetTotal(state()->selected_app())->amount);

  view_builder.AddRows(1, views::TableLayout::kFixedSize, 0)
      .AddChildren(
          views::Builder<views::Label>(CreateBoldLabel(total_label_text)),
          views::Builder<views::View>(CreateInlineCurrencyAmountItem(
              total_currency_code, total_amount, false, true)));

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

  return builder.CreateWithChevron(std::move(view_builder).Build(), nullptr);
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

// Creates the Payment Method row, which contains a "Payment" label, the
// selected Payment Method's name and details, the Payment Method's icon, and a
// chevron.
// +----------------------------------------------+
// | Payment         BobBucks                     |
// |                 bobbucks.dev      | ICON | > |
// |                                              |
// +----------------------------------------------+
std::unique_ptr<PaymentRequestRowView>
PaymentSheetViewController::CreatePaymentMethodRow() {
  PaymentApp* selected_app = state()->selected_app();

  PaymentSheetRowBuilder builder(
      this, l10n_util::GetStringUTF16(
                IDS_PAYMENT_REQUEST_PAYMENT_METHOD_SECTION_NAME));
  builder.Id(DialogViewID::PAYMENT_SHEET_PAYMENT_METHOD_SECTION)
      .Closure(base::BindRepeating(
          &PaymentRequestDialogView::ShowPaymentMethodSheet, dialog()));

  // This method may be called with either no app pre-selected (e.g., if no app
  // has a valid icon), or without any apps available at all (e.g., if we have a
  // ServiceWorker payment app that has not yet been loaded). In those cases, we
  // render a 'choose' dialog instead of the app details.
  if (!selected_app) {
    const std::u16string label = state()->available_apps().empty()
                                     ? std::u16string()
                                     : state()->available_apps()[0]->GetLabel();
    return builder.CreateWithButton(
        label, l10n_util::GetStringUTF16(IDS_CHOOSE), true);
  }

  auto content_view =
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kVertical)
          .SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kStart)
          .SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kCenter)
          .AddChildren(views::Builder<views::Label>()
                           .SetText(selected_app->GetLabel())
                           .SetHorizontalAlignment(gfx::ALIGN_LEFT),
                       views::Builder<views::Label>()
                           .SetText(selected_app->GetSublabel())
                           .SetHorizontalAlignment(gfx::ALIGN_LEFT));

  std::unique_ptr<views::ImageView> icon_view =
      CreateAppIconView(selected_app->icon_resource_id(),
                        selected_app->icon_bitmap(), selected_app->GetLabel());

  return builder.AccessibleContent(selected_app->GetLabel())
      .CreateWithChevron(std::move(content_view).Build(), std::move(icon_view));
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
  static constexpr autofill::FieldType kLabelFields[] = {
      autofill::NAME_FULL, autofill::PHONE_HOME_WHOLE_NUMBER,
      autofill::EMAIL_ADDRESS};
  const std::u16string preview =
      state()->contact_profiles()[0]->ConstructInferredLabel(
          kLabelFields, std::size(kLabelFields),
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

  views::StyledLabel::RangeStyleInfo link_style =
      views::StyledLabel::RangeStyleInfo::CreateForLink(base::BindRepeating(
          [](base::WeakPtr<PaymentRequestDialogView> dialog) {
            if (dialog->IsInteractive()) {
              chrome::ShowSettingsSubPageForProfile(dialog->GetProfile(),
                                                    chrome::kPaymentsSubPage);
            }
          },
          dialog()));

  return views::Builder<views::BoxLayoutView>()
      .SetOrientation(views::BoxLayout::Orientation::kVertical)
      .SetInsideBorderInsets(
          gfx::Insets::VH(0, kPaymentRequestRowHorizontalInsets))
      .SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kStart)
      .SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kStart)
      .AddChild(
          views::Builder<views::StyledLabel>()
              .SetText(data_source)
              .SetBorder(
                  views::CreateEmptyBorder(gfx::Insets::TLBR(22, 0, 0, 0)))
              .SetID(static_cast<int>(DialogViewID::DATA_SOURCE_LABEL))
              .SetDefaultTextStyle(views::style::STYLE_DISABLED)
              .AddStyleRange(gfx::Range(link_begin, link_begin + link_length),
                             link_style)
              .SizeToFit(0))
      .Build();
}

void PaymentSheetViewController::AddShippingButtonPressed() {
  dialog()->ShowShippingAddressEditor(
      BackNavigationType::kPaymentSheet, base::RepeatingClosure(),
      base::BindRepeating(&PaymentRequestState::AddAutofillShippingProfile,
                          state(), true),
      nullptr);
}

void PaymentSheetViewController::AddContactInfoButtonPressed() {
  dialog()->ShowContactInfoEditor(
      BackNavigationType::kPaymentSheet, base::RepeatingClosure(),
      base::BindRepeating(&PaymentRequestState::AddAutofillContactProfile,
                          state(), true),
      nullptr);
}

void PaymentSheetViewController::PossiblyIgnorePrimaryButtonPress(
    PaymentRequestSheetController::ButtonCallback callback,
    const ui::Event& event) {
  if (input_protector_->IsPossiblyUnintendedInteraction(event)) {
    return;
  }
  callback.Run(event);
}

}  // namespace payments
