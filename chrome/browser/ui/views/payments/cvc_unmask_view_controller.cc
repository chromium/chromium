// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/cvc_unmask_view_controller.h"

#include <memory>

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autofill/risk_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/views/autofill/payments/payments_view_util.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "chrome/browser/ui/views/payments/payment_request_views_util.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/payments/full_card_request.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/grit/components_scaled_resources.h"
#include "components/payments/content/payment_request_state.h"
#include "components/payments/core/payment_request_delegate.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/grid_layout.h"

namespace payments {

enum class Tags {
  CONFIRM_TAG = static_cast<int>(PaymentRequestCommonTags::PAY_BUTTON_TAG),
};

CvcUnmaskViewController::CvcUnmaskViewController(
    base::WeakPtr<PaymentRequestSpec> spec,
    base::WeakPtr<PaymentRequestState> state,
    base::WeakPtr<PaymentRequestDialogView> dialog,
    const autofill::CreditCard& credit_card,
    base::WeakPtr<autofill::payments::FullCardRequest::ResultDelegate>
        result_delegate,
    content::WebContents* web_contents)
    : PaymentRequestSheetController(spec, state, dialog),
      year_combobox_model_(credit_card.expiration_year()),
      credit_card_(credit_card),
      web_contents_(web_contents),
      payments_client_(
          content::BrowserContext::GetDefaultStoragePartition(
              web_contents_->GetBrowserContext())
              ->GetURLLoaderFactoryForBrowserProcess(),
          IdentityManagerFactory::GetForProfile(
              Profile::FromBrowserContext(web_contents_->GetBrowserContext())
                  ->GetOriginalProfile()),
          state->GetPersonalDataManager(),
          Profile::FromBrowserContext(web_contents_->GetBrowserContext())
              ->IsOffTheRecord()),
      full_card_request_(this,
                         &payments_client_,
                         state->GetPersonalDataManager()) {
  full_card_request_.GetFullCard(
      credit_card,
      autofill::AutofillClient::UnmaskCardReason::UNMASK_FOR_PAYMENT_REQUEST,
      result_delegate, weak_ptr_factory_.GetWeakPtr());
}

CvcUnmaskViewController::~CvcUnmaskViewController() {}

void CvcUnmaskViewController::LoadRiskData(
    base::OnceCallback<void(const std::string&)> callback) {
  autofill::risk_util::LoadRiskData(0, web_contents_, std::move(callback));
}

void CvcUnmaskViewController::ShowUnmaskPrompt(
    const autofill::CreditCard& card,
    autofill::AutofillClient::UnmaskCardReason reason,
    base::WeakPtr<autofill::CardUnmaskDelegate> delegate) {
  unmask_delegate_ = delegate;
}

void CvcUnmaskViewController::OnUnmaskVerificationResult(
    autofill::AutofillClient::PaymentsRpcResult result) {
  switch (result) {
    case autofill::AutofillClient::NONE:
      NOTREACHED();
      FALLTHROUGH;
    case autofill::AutofillClient::SUCCESS:
      // In the success case, don't show any error and don't hide the spinner
      // because the dialog is about to close when the merchant completes the
      // transaction.
      return;

    case autofill::AutofillClient::TRY_AGAIN_FAILURE:
      DisplayError(l10n_util::GetStringUTF16(
          IDS_AUTOFILL_CARD_UNMASK_PROMPT_ERROR_TRY_AGAIN_CVC));
      break;

    case autofill::AutofillClient::PERMANENT_FAILURE:
      DisplayError(l10n_util::GetStringUTF16(
          IDS_AUTOFILL_CARD_UNMASK_PROMPT_ERROR_PERMANENT));
      break;

    case autofill::AutofillClient::NETWORK_ERROR:
      DisplayError(l10n_util::GetStringUTF16(
          IDS_AUTOFILL_CARD_UNMASK_PROMPT_ERROR_NETWORK));
      break;
  }

  dialog()->HideProcessingSpinner();
}

base::string16 CvcUnmaskViewController::GetSheetTitle() {
  return l10n_util::GetStringFUTF16(IDS_AUTOFILL_CARD_UNMASK_PROMPT_TITLE,
                                    credit_card_.NetworkAndLastFourDigits());
}

void CvcUnmaskViewController::FillContentView(views::View* content_view) {
  views::GridLayout* layout =
      content_view->SetLayoutManager(std::make_unique<views::GridLayout>());
  content_view->SetBorder(views::CreateEmptyBorder(
      kPaymentRequestRowVerticalInsets, kPaymentRequestRowHorizontalInsets,
      kPaymentRequestRowVerticalInsets, kPaymentRequestRowHorizontalInsets));

  views::ColumnSet* instructions_columns = layout->AddColumnSet(0);
  instructions_columns->AddColumn(
      views::GridLayout::Alignment::FILL, views::GridLayout::Alignment::LEADING,
      1.0, views::GridLayout::ColumnSize::kUsePreferred, 0, 0);

  layout->StartRow(views::GridLayout::kFixedSize, 0);
  // The prompt for server cards should reference Google Payments, whereas the
  // prompt for local cards should not.
  auto instructions = std::make_unique<views::Label>(l10n_util::GetStringUTF16(
      credit_card_.record_type() == autofill::CreditCard::LOCAL_CARD
          ? IDS_AUTOFILL_CARD_UNMASK_PROMPT_INSTRUCTIONS_LOCAL_CARD
          : IDS_AUTOFILL_CARD_UNMASK_PROMPT_INSTRUCTIONS));
  instructions->SetMultiLine(true);
  instructions->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  layout->AddView(std::move(instructions));

  // Space between the instructions and the CVC field.
  layout->AddPaddingRow(views::GridLayout::kFixedSize, 16);

  views::ColumnSet* cvc_field_columns = layout->AddColumnSet(1);
  constexpr int kPadding = 16;

  bool requesting_expiration =
      credit_card_.ShouldUpdateExpiration(autofill::AutofillClock::Now());
  if (requesting_expiration) {
    // Month dropdown column
    cvc_field_columns->AddColumn(
        views::GridLayout::Alignment::LEADING,
        views::GridLayout::Alignment::BASELINE, views::GridLayout::kFixedSize,
        views::GridLayout::ColumnSize::kUsePreferred, 0, 0);
    cvc_field_columns->AddPaddingColumn(views::GridLayout::kFixedSize,
                                        kPadding);
    // Year dropdown column
    cvc_field_columns->AddColumn(
        views::GridLayout::Alignment::LEADING,
        views::GridLayout::Alignment::BASELINE, views::GridLayout::kFixedSize,
        views::GridLayout::ColumnSize::kUsePreferred, 0, 0);
    cvc_field_columns->AddPaddingColumn(views::GridLayout::kFixedSize,
                                        kPadding);
  }
  // CVC image
  cvc_field_columns->AddColumn(views::GridLayout::Alignment::LEADING,
                               views::GridLayout::Alignment::BASELINE,
                               views::GridLayout::kFixedSize,
                               views::GridLayout::ColumnSize::kFixed, 32, 32);
  cvc_field_columns->AddPaddingColumn(views::GridLayout::kFixedSize, kPadding);
  // CVC field
  cvc_field_columns->AddColumn(views::GridLayout::Alignment::FILL,
                               views::GridLayout::Alignment::BASELINE,
                               views::GridLayout::kFixedSize,
                               views::GridLayout::ColumnSize::kFixed, 80, 80);

  layout->StartRow(views::GridLayout::kFixedSize, 1);
  if (requesting_expiration) {
    auto month = std::make_unique<views::Combobox>(&month_combobox_model_);
    month->set_callback(base::BindRepeating(
        &CvcUnmaskViewController::OnPerformAction, base::Unretained(this)));
    month->SetID(static_cast<int>(DialogViewID::CVC_MONTH));
    month->SelectValue(credit_card_.Expiration2DigitMonthAsString());
    month->SetInvalid(true);
    layout->AddView(std::move(month));

    auto year = std::make_unique<views::Combobox>(&year_combobox_model_);
    year->set_callback(base::BindRepeating(
        &CvcUnmaskViewController::OnPerformAction, base::Unretained(this)));
    year->SetID(static_cast<int>(DialogViewID::CVC_YEAR));
    year->SelectValue(credit_card_.Expiration4DigitYearAsString());
    year->SetInvalid(true);
    layout->AddView(std::move(year));
  }

  auto cvc_image = std::make_unique<views::ImageView>();
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  // TODO(anthonyvd): Consider using
  // CardUnmaskPromptControllerImpl::GetCvcImageRid.
  cvc_image->SetImage(rb.GetImageSkiaNamed(
      credit_card_.network() == autofill::kAmericanExpressCard
          ? IDR_CREDIT_CARD_CVC_HINT_AMEX
          : IDR_CREDIT_CARD_CVC_HINT));
  cvc_image->SetTooltipText(l10n_util::GetStringUTF16(
      IDS_AUTOFILL_CARD_UNMASK_CVC_IMAGE_DESCRIPTION));
  layout->AddView(std::move(cvc_image));

  std::unique_ptr<views::Textfield> cvc_field = autofill::CreateCvcTextfield();
  cvc_field->set_controller(this);
  cvc_field->SetID(static_cast<int>(DialogViewID::CVC_PROMPT_TEXT_FIELD));
  cvc_field_ = layout->AddView(std::move(cvc_field));

  // Space between the CVC field and the error field.
  layout->AddPaddingRow(views::GridLayout::kFixedSize, 16);

  views::ColumnSet* error_columns = layout->AddColumnSet(2);
  // A column for the error icon
  error_columns->AddColumn(views::GridLayout::Alignment::LEADING,
                           views::GridLayout::Alignment::LEADING,
                           views::GridLayout::kFixedSize,
                           views::GridLayout::ColumnSize::kUsePreferred, 0, 0);
  error_columns->AddPaddingColumn(views::GridLayout::kFixedSize, kPadding);
  // A column for the error label
  error_columns->AddColumn(views::GridLayout::Alignment::LEADING,
                           views::GridLayout::Alignment::LEADING, 1.0,
                           views::GridLayout::ColumnSize::kUsePreferred, 0, 0);

  layout->StartRow(views::GridLayout::kFixedSize, 2);
  auto error_icon = std::make_unique<views::ImageView>();
  error_icon->SetID(static_cast<int>(DialogViewID::CVC_ERROR_ICON));
  error_icon->SetImage(
      gfx::CreateVectorIcon(vector_icons::kWarningIcon, 16,
                            error_icon->GetNativeTheme()->GetSystemColor(
                                ui::NativeTheme::kColorId_AlertSeverityHigh)));
  error_icon->SetVisible(false);
  layout->AddView(std::move(error_icon));

  auto error_label = std::make_unique<views::Label>();
  error_label->SetID(static_cast<int>(DialogViewID::CVC_ERROR_LABEL));
  error_label->SetMultiLine(true);
  error_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  error_label->SetEnabledColor(error_label->GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_AlertSeverityHigh));
  error_label->SetVisible(false);

  layout->AddView(std::move(error_label));
}

std::unique_ptr<views::Button> CvcUnmaskViewController::CreatePrimaryButton() {
  auto button = std::make_unique<views::MdTextButton>(
      this, l10n_util::GetStringUTF16(IDS_CONFIRM));
  button->SetProminent(true);
  button->SetEnabled(false);  // Only enabled when a valid CVC is entered.
  button->SetID(static_cast<int>(DialogViewID::CVC_PROMPT_CONFIRM_BUTTON));
  button->set_tag(static_cast<int>(Tags::CONFIRM_TAG));
  return button;
}

bool CvcUnmaskViewController::ShouldShowSecondaryButton() {
  // Do not show the "Cancel Payment" button.
  return false;
}

void CvcUnmaskViewController::ButtonPressed(views::Button* sender,
                                            const ui::Event& event) {
  if (!dialog()->IsInteractive())
    return;

  switch (sender->tag()) {
    case static_cast<int>(Tags::CONFIRM_TAG):
      CvcConfirmed();
      break;
    case static_cast<int>(PaymentRequestCommonTags::BACK_BUTTON_TAG):
      unmask_delegate_->OnUnmaskPromptClosed();
      dialog()->GoBack();
      break;
    default:
      PaymentRequestSheetController::ButtonPressed(sender, event);
  }
}

void CvcUnmaskViewController::CvcConfirmed() {
  const base::string16& cvc = cvc_field_->GetText();
  if (unmask_delegate_) {
    autofill::CardUnmaskDelegate::UserProvidedUnmaskDetails details;
    details.cvc = cvc;
    if (credit_card_.ShouldUpdateExpiration(autofill::AutofillClock::Now())) {
      views::Combobox* month = static_cast<views::Combobox*>(
          dialog()->GetViewByID(static_cast<int>(DialogViewID::CVC_MONTH)));
      DCHECK(month);
      views::Combobox* year = static_cast<views::Combobox*>(
          dialog()->GetViewByID(static_cast<int>(DialogViewID::CVC_YEAR)));
      DCHECK(year);

      details.exp_month = month->GetTextForRow(month->GetSelectedIndex());
      details.exp_year = year->GetTextForRow(year->GetSelectedIndex());
    }
    unmask_delegate_->OnUnmaskPromptAccepted(details);
  }
}

void CvcUnmaskViewController::DisplayError(base::string16 error) {
  views::Label* error_label = static_cast<views::Label*>(
      dialog()->GetViewByID(static_cast<int>(DialogViewID::CVC_ERROR_LABEL)));
  error_label->SetText(error);
  error_label->SetVisible(true);
  dialog()
      ->GetViewByID(static_cast<int>(DialogViewID::CVC_ERROR_ICON))
      ->SetVisible(true);
  RelayoutPane();
}

void CvcUnmaskViewController::UpdatePayButtonState() {
  base::string16 trimmed_text;
  base::TrimWhitespace(cvc_field_->GetText(), base::TRIM_ALL, &trimmed_text);
  bool cvc_valid = autofill::IsValidCreditCardSecurityCode(
      trimmed_text, credit_card_.network());
  cvc_field_->SetInvalid(!cvc_valid);

  views::Combobox* month = static_cast<views::Combobox*>(
      dialog()->GetViewByID(static_cast<int>(DialogViewID::CVC_MONTH)));
  views::Combobox* year = static_cast<views::Combobox*>(
      dialog()->GetViewByID(static_cast<int>(DialogViewID::CVC_YEAR)));

  bool expiration_date_valid =
      !credit_card_.ShouldUpdateExpiration(autofill::AutofillClock::Now());

  if (month && year) {
    DCHECK(!expiration_date_valid);
    int month_value = 0;
    int year_value = 0;
    bool parsable =
        base::StringToInt(month->GetTextForRow(month->GetSelectedIndex()),
                          &month_value) &&
        base::StringToInt(year->GetTextForRow(year->GetSelectedIndex()),
                          &year_value);

    if (!parsable) {
      // The "Month" or "Year" placeholder items are selected instead of actual
      // month/year values.
      expiration_date_valid = false;
    } else {
      // Convert 2 digit year to 4 digit year.
      if (year_value < 100) {
        base::Time::Exploded now;
        autofill::AutofillClock::Now().LocalExplode(&now);
        year_value += (now.year / 100) * 100;
      }

      expiration_date_valid = autofill::IsValidCreditCardExpirationDate(
          year_value, month_value, autofill::AutofillClock::Now());

      month->SetInvalid(!expiration_date_valid);
      year->SetInvalid(!expiration_date_valid);
    }
  }

  primary_button()->SetEnabled(cvc_valid && expiration_date_valid);
}

bool CvcUnmaskViewController::GetSheetId(DialogViewID* sheet_id) {
  *sheet_id = DialogViewID::CVC_UNMASK_SHEET;
  return true;
}

views::View* CvcUnmaskViewController::GetFirstFocusedView() {
  return cvc_field_;
}

void CvcUnmaskViewController::ContentsChanged(
    views::Textfield* sender,
    const base::string16& new_contents) {
  UpdatePayButtonState();
}

void CvcUnmaskViewController::OnPerformAction() {
  if (!dialog()->IsInteractive())
    return;

  UpdatePayButtonState();
}

}  // namespace payments
