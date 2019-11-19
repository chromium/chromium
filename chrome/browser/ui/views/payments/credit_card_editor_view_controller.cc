// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/credit_card_editor_view_controller.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/ui/autofill/payments/autofill_dialog_models.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "chrome/browser/ui/views/payments/payment_request_views_util.h"
#include "chrome/browser/ui/views/payments/validating_combobox.h"
#include "chrome/browser/ui/views/payments/validating_textfield.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/payments/payments_service_url.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/ui/address_combobox_model.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/payments/content/payment_request_spec.h"
#include "components/payments/content/payment_request_state.h"
#include "components/payments/core/autofill_card_validation.h"
#include "components/payments/core/payment_request_data_util.h"
#include "components/payments/core/strings_util.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/view.h"

namespace payments {

namespace {

// Opacity of card network icons when they are not selected and are displayed
// alongside a selected icon.
const float kDimmedCardIconOpacity = 0.33f;

// This is not quite right but is the closest server type that wasn't already
// used.
const auto kBillingAddressType = autofill::ADDRESS_BILLING_LINE1;

bool IsCardExpired(const base::string16& month,
                   const base::string16& year,
                   const std::string& app_locale) {
  autofill::CreditCard card;
  card.SetExpirationMonthFromString(month, app_locale);
  card.SetExpirationYearFromString(year);
  return card.IsExpired(autofill::AutofillClock::Now());
}

// Validates the two comboboxes used for expiration date.
class ExpirationDateValidationDelegate : public ValidationDelegate {
 public:
  ExpirationDateValidationDelegate(EditorViewController* controller,
                                   const std::string& app_locale,
                                   bool initially_valid)
      : controller_(controller),
        app_locale_(app_locale),
        initially_valid_(initially_valid) {}

  bool IsValidTextfield(views::Textfield* textfield,
                        base::string16* error_message) override {
    NOTREACHED();
    return true;
  }

  bool IsValidCombobox(views::Combobox* combobox,
                       base::string16* error_message) override {
    // View will have no parent if it's not been attached yet. Use initial
    // validity state.
    views::View* view_parent = combobox->parent();
    if (!view_parent) {
      *error_message =
          initially_valid_
              ? base::string16()
              : l10n_util::GetStringUTF16(
                    IDS_PAYMENTS_VALIDATION_INVALID_CREDIT_CARD_EXPIRED);
      return initially_valid_;
    }

    // Get the combined date from the month and year dropdowns.
    views::Combobox* month_combobox = static_cast<views::Combobox*>(
        view_parent->GetViewByID(EditorViewController::GetInputFieldViewId(
            autofill::CREDIT_CARD_EXP_MONTH)));
    base::string16 month =
        month_combobox->model()->GetItemAt(month_combobox->GetSelectedIndex());

    views::Combobox* year_combobox = static_cast<views::Combobox*>(
        view_parent->GetViewByID(EditorViewController::GetInputFieldViewId(
            autofill::CREDIT_CARD_EXP_4_DIGIT_YEAR)));
    base::string16 year =
        year_combobox->model()->GetItemAt(year_combobox->GetSelectedIndex());

    bool is_expired = IsCardExpired(month, year, app_locale_);
    month_combobox->SetInvalid(is_expired);
    year_combobox->SetInvalid(is_expired);

    *error_message =
        is_expired ? l10n_util::GetStringUTF16(
                         IDS_PAYMENTS_VALIDATION_INVALID_CREDIT_CARD_EXPIRED)
                   : base::string16();
    return !is_expired;
  }

  bool TextfieldValueChanged(views::Textfield* textfield,
                             bool was_blurred) override {
    NOTREACHED();
    return true;
  }

  bool ComboboxValueChanged(views::Combobox* combobox) override {
    base::string16 error_message;
    bool is_valid = IsValidCombobox(combobox, &error_message);
    controller_->DisplayErrorMessageForField(
        autofill::CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR, error_message);
    return is_valid;
  }

  void ComboboxModelChanged(views::Combobox* combobox) override {}

 private:
  EditorViewController* controller_;
  const std::string app_locale_;
  bool initially_valid_;

  DISALLOW_COPY_AND_ASSIGN(ExpirationDateValidationDelegate);
};

}  // namespace

CreditCardEditorViewController::CreditCardEditorViewController(
    PaymentRequestSpec* spec,
    PaymentRequestState* state,
    PaymentRequestDialogView* dialog,
    BackNavigationType back_navigation,
    int next_ui_tag,
    base::OnceClosure on_edited,
    base::OnceCallback<void(const autofill::CreditCard&)> on_added,
    autofill::CreditCard* credit_card,
    bool is_incognito)
    : EditorViewController(spec, state, dialog, back_navigation, is_incognito),
      on_edited_(std::move(on_edited)),
      on_added_(std::move(on_added)),
      credit_card_to_edit_(credit_card),
      add_billing_address_button_tag_(next_ui_tag) {
  if (spec)
    supported_card_networks_ = spec->supported_card_networks_set();
}

CreditCardEditorViewController::~CreditCardEditorViewController() {}

// Creates the "Cards accepted" view with a row of icons at the top of the
// credit card editor.
// +----------------------------------------------+
// | Cards Accepted                               |
// |                                              |
// | | VISA | | MC | | AMEX |                     |
// +----------------------------------------------+
std::unique_ptr<views::View>
CreditCardEditorViewController::CreateHeaderView() {
  std::unique_ptr<views::View> view = std::make_unique<views::View>();

  // 9dp is required between the first row (label) and second row (icons).
  constexpr int kRowVerticalSpacing = 9;
  // 6dp is added to the bottom padding, for a total of 12 between the icons and
  // the first input field.
  constexpr int kRowBottomPadding = 6;
  auto layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets(kRowBottomPadding, kPaymentRequestRowHorizontalInsets),
      kRowVerticalSpacing);
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kStart);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);
  view->SetLayoutManager(std::move(layout));

  // "Cards accepted" label is "hint" grey.
  view->AddChildView(CreateHintLabel(GetAcceptedCardTypesText(
                                         spec()->supported_card_types_set()))
                         .release());

  // 8dp padding is required between icons.
  constexpr int kPaddingBetweenCardIcons = 8;
  std::unique_ptr<views::View> icons_row = std::make_unique<views::View>();
  icons_row->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
      kPaddingBetweenCardIcons));

  std::string selected_network =
      credit_card_to_edit_ ? autofill::data_util::GetPaymentRequestData(
                                 credit_card_to_edit_->network())
                                 .basic_card_issuer_network
                           : "";
  constexpr gfx::Size kCardIconSize = gfx::Size(30, 18);
  for (const std::string& supported_network :
       spec()->supported_card_networks()) {
    const std::string autofill_card_type =
        autofill::data_util::GetIssuerNetworkForBasicCardIssuerNetwork(
            supported_network);
    // Icon is fully opaque if no network is selected, or if it is the selected
    // network.
    float opacity =
        selected_network.empty() || selected_network == supported_network
            ? 1.0f
            : kDimmedCardIconOpacity;
    std::unique_ptr<views::ImageView> card_icon_view = CreateAppIconView(
        autofill::data_util::GetPaymentRequestData(autofill_card_type)
            .icon_resource_id,
        gfx::ImageSkia(), base::UTF8ToUTF16(supported_network), opacity);
    card_icon_view->SetImageSize(kCardIconSize);

    // Keep track of this card icon to later adjust opacity.
    card_icons_[supported_network] = card_icon_view.get();

    icons_row->AddChildView(card_icon_view.release());
  }
  view->AddChildView(icons_row.release());

  // If dealing with a server card, we add "From Google Payments" with an edit
  // link.
  if (IsEditingServerCard()) {
    std::unique_ptr<views::View> data_source = std::make_unique<views::View>();
    data_source->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
        kPaddingBetweenCardIcons));

    // "From Google Payments".
    data_source->AddChildView(
        CreateHintLabel(
            l10n_util::GetStringUTF16(IDS_AUTOFILL_FROM_GOOGLE_ACCOUNT_LONG))
            .release());

    // "Edit" link.
    base::string16 link_text =
        l10n_util::GetStringUTF16(IDS_AUTOFILL_WALLET_MANAGEMENT_LINK_TEXT);
    auto edit_link = std::make_unique<views::StyledLabel>(link_text, this);
    edit_link->SetID(
        static_cast<int>(DialogViewID::GOOGLE_PAYMENTS_EDIT_LINK_LABEL));
    edit_link->AddStyleRange(
        gfx::Range(0, link_text.size()),
        views::StyledLabel::RangeStyleInfo::CreateForLink());
    edit_link->SizeToFit(0);
    data_source->AddChildView(edit_link.release());

    view->AddChildView(data_source.release());
  }

  return view;
}

std::unique_ptr<views::View>
CreditCardEditorViewController::CreateCustomFieldView(
    autofill::ServerFieldType type,
    views::View** focusable_field,
    bool* valid,
    base::string16* error_message) {
  DCHECK_EQ(type, autofill::CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR);

  std::unique_ptr<views::View> view = std::make_unique<views::View>();
  if (IsEditingServerCard()) {
    std::unique_ptr<views::Label> exp_label = std::make_unique<views::Label>(
        credit_card_to_edit_->ExpirationDateForDisplay());
    exp_label->SetID(
        GetInputFieldViewId(autofill::CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR));
    exp_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

    view = std::move(exp_label);
  } else {
    // Two comboboxes, one for month and the other for year.
    views::GridLayout* combobox_layout =
        view->SetLayoutManager(std::make_unique<views::GridLayout>());
    views::ColumnSet* columns = combobox_layout->AddColumnSet(0);
    columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                       1.0, views::GridLayout::USE_PREF, 0, 0);
    // Space between the two comboboxes.
    constexpr int kHorizontalSpacing = 8;
    columns->AddPaddingColumn(views::GridLayout::kFixedSize,
                              kHorizontalSpacing);
    columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                       1.0, views::GridLayout::USE_PREF, 0, 0);

    combobox_layout->StartRow(views::GridLayout::kFixedSize, 0);
    constexpr int kInputFieldHeight = 28;
    EditorField tmp_month{
        autofill::CREDIT_CARD_EXP_MONTH,
        l10n_util::GetStringUTF16(IDS_SETTINGS_CREDIT_CARD_EXPIRATION_MONTH),
        EditorField::LengthHint::HINT_SHORT,
        /*required=*/true, EditorField::ControlType::COMBOBOX};
    std::unique_ptr<ValidatingCombobox> month_combobox =
        CreateComboboxForField(tmp_month, error_message);
    *focusable_field = combobox_layout->AddView(
        std::move(month_combobox), 1, 1, views::GridLayout::FILL,
        views::GridLayout::FILL, 0, kInputFieldHeight);

    EditorField tmp_year{
        autofill::CREDIT_CARD_EXP_4_DIGIT_YEAR,
        l10n_util::GetStringUTF16(IDS_SETTINGS_CREDIT_CARD_EXPIRATION_YEAR),
        EditorField::LengthHint::HINT_SHORT,
        /*required=*/true, EditorField::ControlType::COMBOBOX};
    std::unique_ptr<ValidatingCombobox> year_combobox =
        CreateComboboxForField(tmp_year, error_message);
    combobox_layout->AddView(std::move(year_combobox), 1, 1,
                             views::GridLayout::FILL, views::GridLayout::FILL,
                             0, kInputFieldHeight);
  }

  // Set the initial validity of the custom view.
  base::string16 month =
      GetInitialValueForType(autofill::CREDIT_CARD_EXP_MONTH);
  base::string16 year =
      GetInitialValueForType(autofill::CREDIT_CARD_EXP_4_DIGIT_YEAR);
  *valid = !IsCardExpired(month, year, state()->GetApplicationLocale());
  return view;
}

std::unique_ptr<views::View>
CreditCardEditorViewController::CreateExtraViewForField(
    autofill::ServerFieldType type) {
  if (type != kBillingAddressType)
    return nullptr;

  auto button_view = std::make_unique<views::View>();
  button_view->SetLayoutManager(std::make_unique<views::FillLayout>());

  // The button to add new billing addresses.
  auto add_button =
      views::MdTextButton::Create(this, l10n_util::GetStringUTF16(IDS_ADD));
  add_button->SetID(static_cast<int>(DialogViewID::ADD_BILLING_ADDRESS_BUTTON));
  add_button->set_tag(add_billing_address_button_tag_);
  add_button->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  button_view->AddChildView(std::move(add_button));
  return button_view;
}

bool CreditCardEditorViewController::IsEditingExistingItem() {
  return !!credit_card_to_edit_;
}

std::vector<EditorField> CreditCardEditorViewController::GetFieldDefinitions() {
  bool is_server_card = IsEditingServerCard();
  return std::vector<EditorField>{
      {autofill::CREDIT_CARD_NUMBER,
       l10n_util::GetStringUTF16(IDS_SETTINGS_CREDIT_CARD_NUMBER),
       EditorField::LengthHint::HINT_SHORT, /*required=*/true,
       is_server_card ? EditorField::ControlType::READONLY_LABEL
                      : EditorField::ControlType::TEXTFIELD_NUMBER},
      {autofill::CREDIT_CARD_NAME_FULL,
       l10n_util::GetStringUTF16(IDS_SETTINGS_NAME_ON_CREDIT_CARD),
       EditorField::LengthHint::HINT_SHORT, /*required=*/true,
       is_server_card ? EditorField::ControlType::READONLY_LABEL
                      : EditorField::ControlType::TEXTFIELD},
      {autofill::CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR,
       l10n_util::GetStringUTF16(IDS_SETTINGS_CREDIT_CARD_EXPIRATION_DATE),
       EditorField::LengthHint::HINT_SHORT, /*required=*/true,
       EditorField::ControlType::CUSTOMFIELD},
      {kBillingAddressType,
       l10n_util::GetStringUTF16(IDS_AUTOFILL_FIELD_LABEL_BILLING_ADDRESS),
       EditorField::LengthHint::HINT_SHORT, /*required=*/true,
       EditorField::ControlType::COMBOBOX}};
}

base::string16 CreditCardEditorViewController::GetInitialValueForType(
    autofill::ServerFieldType type) {
  if (!credit_card_to_edit_ || type == kBillingAddressType)
    return base::string16();

  base::string16 info = credit_card_to_edit_->GetInfo(
      autofill::AutofillType(type), state()->GetApplicationLocale());

  return type == autofill::CREDIT_CARD_NUMBER
             ? data_util::FormatCardNumberForDisplay(info)
             : info;
}

bool CreditCardEditorViewController::ValidateModelAndSave() {
  if (IsEditingServerCard()) {
    views::Combobox* address_combobox = static_cast<views::Combobox*>(
        dialog()->GetViewByID(GetInputFieldViewId(kBillingAddressType)));
    if (address_combobox->GetInvalid())
      return false;

    autofill::AddressComboboxModel* model =
        static_cast<autofill::AddressComboboxModel*>(address_combobox->model());

    credit_card_to_edit_->set_billing_address_id(
        model->GetItemIdentifierAt(address_combobox->GetSelectedIndex()));
    if (!is_incognito()) {
      state()->GetPersonalDataManager()->UpdateServerCardMetadata(
          *credit_card_to_edit_);
    }
    return true;
  }

  const std::string& locale = state()->GetApplicationLocale();
  // Use a temporary object for validation.
  autofill::CreditCard credit_card;
  credit_card.set_origin(autofill::kSettingsOrigin);

  if (!ValidateInputFields())
    return false;

  for (const auto& field : text_fields()) {
    // ValidatingTextfield* is the key, EditorField is the value.
    DCHECK_EQ(autofill::CREDIT_CARD,
              autofill::AutofillType(field.second.type).group());
    credit_card.SetInfo(autofill::AutofillType(field.second.type),
                        field.first->GetText(), locale);
  }
  for (const auto& field : comboboxes()) {
    // ValidatingCombobox* is the key, EditorField is the value.
    ValidatingCombobox* combobox = field.first;

    if (field.second.type == kBillingAddressType) {
      autofill::AddressComboboxModel* model =
          static_cast<autofill::AddressComboboxModel*>(combobox->model());

      credit_card.set_billing_address_id(
          model->GetItemIdentifierAt(combobox->GetSelectedIndex()));
    } else {
      credit_card.SetInfo(autofill::AutofillType(field.second.type),
                          combobox->GetTextForRow(combobox->GetSelectedIndex()),
                          locale);
    }
  }

  // TODO(crbug.com/711365): Display global error message.
  if (GetCompletionStatusForCard(
          credit_card, locale,
          state()->GetPersonalDataManager()->GetProfiles()) !=
      CREDIT_CARD_COMPLETE) {
    return false;
  }

  if (!credit_card_to_edit_) {
    if (!is_incognito()) {
      // Add the card (will not add a duplicate).
      state()->GetPersonalDataManager()->AddCreditCard(credit_card);
    }
    std::move(on_added_).Run(credit_card);
  } else {
    credit_card_to_edit_->set_billing_address_id(
        credit_card.billing_address_id());
    // We were in edit mode. Copy the data from the temporary object to retain
    // the edited object's other properties (use count, use date, guid, etc.).
    for (const auto& field : text_fields()) {
      credit_card_to_edit_->SetInfo(
          autofill::AutofillType(field.second.type),
          credit_card.GetInfo(autofill::AutofillType(field.second.type),
                              locale),
          locale);
    }
    for (const auto& field : comboboxes()) {
      // The billing address is transfered above.
      if (field.second.type == kBillingAddressType)
        continue;

      credit_card_to_edit_->SetInfo(
          autofill::AutofillType(field.second.type),
          credit_card.GetInfo(autofill::AutofillType(field.second.type),
                              locale),
          locale);
    }
    if (!is_incognito())
      state()->GetPersonalDataManager()->UpdateCreditCard(
          *credit_card_to_edit_);
    std::move(on_edited_).Run();
  }

  return true;
}

std::unique_ptr<ValidationDelegate>
CreditCardEditorViewController::CreateValidationDelegate(
    const EditorField& field) {
  if (field.type == autofill::CREDIT_CARD_EXP_MONTH ||
      field.type == autofill::CREDIT_CARD_EXP_4_DIGIT_YEAR) {
    bool initially_valid =
        credit_card_to_edit_
            ? !credit_card_to_edit_->IsExpired(autofill::AutofillClock::Now())
            : true;
    return std::make_unique<ExpirationDateValidationDelegate>(
        this, state()->GetApplicationLocale(), initially_valid);
  }
  // The supported card networks for non-cc-number types are not passed to avoid
  // the data copy in the delegate.
  return std::make_unique<
      CreditCardEditorViewController::CreditCardValidationDelegate>(field,
                                                                    this);
}

std::unique_ptr<ui::ComboboxModel>
CreditCardEditorViewController::GetComboboxModelForType(
    const autofill::ServerFieldType& type) {
  switch (type) {
    case autofill::CREDIT_CARD_EXP_MONTH: {
      return std::make_unique<autofill::MonthComboboxModel>();
    }
    case autofill::CREDIT_CARD_EXP_4_DIGIT_YEAR:
      return std::make_unique<autofill::YearComboboxModel>(
          credit_card_to_edit_ ? credit_card_to_edit_->expiration_year() : 0);
    case kBillingAddressType:
      // The combobox filled with potential billing addresses. It's fine to pass
      // empty string as the default selected guid if there are no cards being
      // edited.
      return std::make_unique<autofill::AddressComboboxModel>(
          *state()->GetPersonalDataManager(), state()->GetApplicationLocale(),
          credit_card_to_edit_ ? credit_card_to_edit_->billing_address_id()
                               : "");
    default:
      NOTREACHED();
      break;
  }
  return std::unique_ptr<ui::ComboboxModel>();
}

void CreditCardEditorViewController::StyledLabelLinkClicked(
    views::StyledLabel* label,
    const gfx::Range& range,
    int event_flags) {
  // The only thing that can trigger this is the user clicking on the "edit"
  // link for a server card.
  chrome::ScopedTabbedBrowserDisplayer displayer(dialog()->GetProfile());
  ShowSingletonTab(displayer.browser(),
                   autofill::payments::GetManageAddressesUrl());
}

void CreditCardEditorViewController::SelectBasicCardNetworkIcon(
    const std::string& basic_card_network) {
  // If empty string was passed or if the icon representing |basic_card_network|
  // is not present (i.e. not supported), all icons have full opacity.
  bool full_opacity =
      card_icons_.find(basic_card_network) == card_icons_.end() ||
      basic_card_network.empty();
  for (auto network_icon_it : card_icons_) {
    float target_opacity =
        full_opacity || network_icon_it.first == basic_card_network
            ? 1.0f
            : kDimmedCardIconOpacity;
    network_icon_it.second->layer()->SetOpacity(target_opacity);
    network_icon_it.second->layer()->ScheduleDraw();
  }
}

void CreditCardEditorViewController::FillContentView(
    views::View* content_view) {
  EditorViewController::FillContentView(content_view);
  // We need to search from the content view here, since the dialog may not have
  // the content view added to it yet.
  views::Combobox* combobox = static_cast<views::Combobox*>(
      content_view->GetViewByID(GetInputFieldViewId(kBillingAddressType)));
  // When the combobox has a single item, it's because it has no addresses
  // (otherwise, it would have the select header, and a separator before the
  // first address to choose from).
  DCHECK(combobox);
  combobox->SetEnabled(combobox->GetRowCount() > 1);
}

bool CreditCardEditorViewController::IsValidCreditCardNumber(
    const base::string16& card_number,
    base::string16* error_message) {
  return autofill::IsValidCreditCardNumberForBasicCardNetworks(
      card_number, supported_card_networks_, error_message);
  // TODO(crbug.com/725604): The UI should offer to load / update the existing
  // credit card info if another local credit card has already been created with
  // this number. (Does not apply to server cards, which can be accessed only in
  // tokenized form through Google Pay.)
}

base::string16 CreditCardEditorViewController::GetSheetTitle() {
  if (!credit_card_to_edit_)
    return l10n_util::GetStringUTF16(IDS_PAYMENTS_ADD_CARD);

  // Gets the completion message, or empty if nothing is missing from the card.
  base::string16 title = GetCompletionMessageForCard(GetCompletionStatusForCard(
      *credit_card_to_edit_, state()->GetApplicationLocale(),
      state()->GetPersonalDataManager()->GetProfiles()));
  return title.empty() ? l10n_util::GetStringUTF16(IDS_PAYMENTS_EDIT_CARD)
                       : title;
}

void CreditCardEditorViewController::ButtonPressed(views::Button* sender,
                                                   const ui::Event& event) {
  if (sender->tag() == add_billing_address_button_tag_) {
    dialog()->ShowShippingAddressEditor(
        BackNavigationType::kOneStep,
        /*on_edited=*/
        base::OnceClosure(),
        /*on_added=*/
        base::BindOnce(
            &CreditCardEditorViewController::AddAndSelectNewBillingAddress,
            weak_ptr_factory_.GetWeakPtr()),
        /*profile=*/nullptr);
  } else {
    EditorViewController::ButtonPressed(sender, event);
  }
}

void CreditCardEditorViewController::AddAndSelectNewBillingAddress(
    const autofill::AutofillProfile& profile) {
  state()->AddAutofillShippingProfile(false, profile);
  views::Combobox* address_combobox = static_cast<views::Combobox*>(
      dialog()->GetViewByID(GetInputFieldViewId(kBillingAddressType)));
  autofill::AddressComboboxModel* model =
      static_cast<autofill::AddressComboboxModel*>(address_combobox->model());
  int index = model->AddNewProfile(profile);
  // SetSelectedIndex doesn't trigger a perform action notification, which is
  // needed to update the valid state.
  address_combobox->SetSelectedRow(index);
  // The combobox might be initially disabled in FillContentView, but we've
  // added an item; check if we should re-enable it.
  address_combobox->SetEnabled(address_combobox->GetRowCount() > 1);
  // But it needs to be blured at least once.
  address_combobox->OnBlur();
}

bool CreditCardEditorViewController::IsEditingServerCard() const {
  return credit_card_to_edit_ && credit_card_to_edit_->record_type() !=
                                     autofill::CreditCard::LOCAL_CARD;
}

CreditCardEditorViewController::CreditCardValidationDelegate::
    CreditCardValidationDelegate(const EditorField& field,
                                 CreditCardEditorViewController* controller)
    : field_(field), controller_(controller) {}
CreditCardEditorViewController::CreditCardValidationDelegate::
    ~CreditCardValidationDelegate() {}

bool CreditCardEditorViewController::CreditCardValidationDelegate::
    ShouldFormat() {
  return field_.type == autofill::CREDIT_CARD_NUMBER;
}

base::string16
CreditCardEditorViewController::CreditCardValidationDelegate::Format(
    const base::string16& text) {
  return data_util::FormatCardNumberForDisplay(text);
}

bool CreditCardEditorViewController::CreditCardValidationDelegate::
    IsValidTextfield(views::Textfield* textfield,
                     base::string16* error_message) {
  return ValidateValue(textfield->GetText(), error_message);
}

bool CreditCardEditorViewController::CreditCardValidationDelegate::
    IsValidCombobox(views::Combobox* combobox, base::string16* error_message) {
  return ValidateCombobox(combobox, error_message);
}

bool CreditCardEditorViewController::CreditCardValidationDelegate::
    TextfieldValueChanged(views::Textfield* textfield, bool was_blurred) {
  // The only behavior pre-blur is selecting the card icon.
  if (field_.type == autofill::CREDIT_CARD_NUMBER) {
    std::string basic_card_network =
        autofill::data_util::GetPaymentRequestData(
            autofill::CreditCard::GetCardNetwork(textfield->GetText()))
            .basic_card_issuer_network;
    controller_->SelectBasicCardNetworkIcon(basic_card_network);
  }

  // We return true if the field was not yet blurred, because validation should
  // not occur yet.
  if (!was_blurred)
    return true;

  base::string16 error_message;
  bool is_valid = ValidateValue(textfield->GetText(), &error_message);
  controller_->DisplayErrorMessageForField(field_.type, error_message);

  return is_valid;
}

bool CreditCardEditorViewController::CreditCardValidationDelegate::
    ComboboxValueChanged(views::Combobox* combobox) {
  base::string16 error_message;
  bool is_valid = ValidateCombobox(combobox, nullptr);
  controller_->DisplayErrorMessageForField(field_.type, error_message);
  return is_valid;
}

bool CreditCardEditorViewController::CreditCardValidationDelegate::
    ValidateValue(const base::string16& value, base::string16* error_message) {
  if (!value.empty()) {
    base::string16 local_error_message;
    bool is_valid = false;
    if (field_.type == autofill::CREDIT_CARD_NUMBER) {
      is_valid =
          controller_->IsValidCreditCardNumber(value, &local_error_message);
    } else {
      is_valid =
          autofill::IsValidForType(value, field_.type, &local_error_message);
    }
    if (error_message)
      *error_message = local_error_message;
    return is_valid;
  }

  if (error_message && field_.required) {
    *error_message = l10n_util::GetStringUTF16(
        IDS_PREF_EDIT_DIALOG_FIELD_REQUIRED_VALIDATION_MESSAGE);
  }
  return !field_.required;
}

bool CreditCardEditorViewController::CreditCardValidationDelegate::
    ValidateCombobox(views::Combobox* combobox, base::string16* error_message) {
  // The billing address ID is the selected item identifier and not the combobox
  // value itself.
  if (field_.type == kBillingAddressType) {
    // TODO(crbug.com/718905) Find a way to deal with existing incomplete
    // addresses when choosing them as billing addresses.
    autofill::AddressComboboxModel* model =
        static_cast<autofill::AddressComboboxModel*>(combobox->model());
    if (model->GetItemIdentifierAt(combobox->GetSelectedIndex()).empty()) {
      if (error_message) {
        *error_message =
            l10n_util::GetStringUTF16(IDS_PAYMENTS_BILLING_ADDRESS_REQUIRED);
      }
      return false;
    }
    return true;
  }
  return ValidateValue(combobox->GetTextForRow(combobox->GetSelectedIndex()),
                       error_message);
}

bool CreditCardEditorViewController::GetSheetId(DialogViewID* sheet_id) {
  *sheet_id = DialogViewID::CREDIT_CARD_EDITOR_SHEET;
  return true;
}

}  // namespace payments
