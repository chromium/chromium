// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/cvc_unmask_view_controller.h"

#include <memory>

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/autofill/risk_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/views/autofill/payments/payments_view_util.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
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
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/fill_layout.h"

namespace {

class ErrorLabelView : public views::Label {
 public:
  METADATA_HEADER(ErrorLabelView);

  ErrorLabelView() {
    SetID(static_cast<int>(payments::DialogViewID::CVC_ERROR_LABEL));
    SetMultiLine(true);
    SetHorizontalAlignment(gfx::ALIGN_LEFT);
    SetVisible(false);
  }

  // views::View:
  void OnThemeChanged() override {
    Label::OnThemeChanged();
    SetEnabledColor(GetColorProvider()->GetColor(ui::kColorAlertHighSeverity));
  }
};

BEGIN_METADATA(ErrorLabelView, views::Label)
END_METADATA

}  // namespace

namespace payments {

CvcUnmaskViewController::CvcUnmaskViewController(
    base::WeakPtr<PaymentRequestSpec> spec,
    base::WeakPtr<PaymentRequestState> state,
    base::WeakPtr<PaymentRequestDialogView> dialog,
    const autofill::CreditCard& credit_card,
    base::WeakPtr<autofill::payments::FullCardRequest::ResultDelegate>
        result_delegate,
    content::RenderFrameHost* render_frame_host)
    : PaymentRequestSheetController(spec, state, dialog),
      year_combobox_model_(credit_card.expiration_year()),
      credit_card_(credit_card),
      frame_routing_id_(render_frame_host->GetGlobalId()),
      payments_client_(
          render_frame_host->GetBrowserContext()
              ->GetDefaultStoragePartition()
              ->GetURLLoaderFactoryForBrowserProcess(),
          IdentityManagerFactory::GetForProfile(
              Profile::FromBrowserContext(
                  render_frame_host->GetBrowserContext())
                  ->GetOriginalProfile()),
          state->GetPersonalDataManager(),
          Profile::FromBrowserContext(render_frame_host->GetBrowserContext())
              ->IsOffTheRecord()),
      full_card_request_(this,
                         &payments_client_,
                         state->GetPersonalDataManager()) {
  full_card_request_.GetFullCard(
      credit_card, autofill::AutofillClient::UnmaskCardReason::kPaymentRequest,
      result_delegate, weak_ptr_factory_.GetWeakPtr());
}

CvcUnmaskViewController::~CvcUnmaskViewController() {
  if (month_combobox_)
    month_combobox_->SetModel(nullptr);
  if (year_combobox_)
    year_combobox_->SetModel(nullptr);
}

void CvcUnmaskViewController::LoadRiskData(
    base::OnceCallback<void(const std::string&)> callback) {
  auto* rfh = content::RenderFrameHost::FromID(frame_routing_id_);
  if (!rfh)
    return;

  auto* web_contents = content::WebContents::FromRenderFrameHost(rfh);
  if (!web_contents)
    return;

  autofill::risk_util::LoadRiskData(0, web_contents, std::move(callback));
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
    case autofill::AutofillClient::PaymentsRpcResult::kNone:
      NOTREACHED();
      [[fallthrough]];
    case autofill::AutofillClient::PaymentsRpcResult::kSuccess:
      // In the success case, don't show any error and don't hide the spinner
      // because the dialog is about to close when the merchant completes the
      // transaction.
      return;

    case autofill::AutofillClient::PaymentsRpcResult::kTryAgainFailure:
      DisplayError(l10n_util::GetStringUTF16(
          IDS_AUTOFILL_CARD_UNMASK_PROMPT_ERROR_TRY_AGAIN_CVC));
      break;

    case autofill::AutofillClient::PaymentsRpcResult::kPermanentFailure:
      DisplayError(l10n_util::GetStringUTF16(
          IDS_AUTOFILL_CARD_UNMASK_PROMPT_ERROR_PERMANENT));
      break;

    case autofill::AutofillClient::PaymentsRpcResult::kNetworkError:
      DisplayError(l10n_util::GetStringUTF16(
          IDS_AUTOFILL_CARD_UNMASK_PROMPT_ERROR_NETWORK));
      break;
    case autofill::AutofillClient::PaymentsRpcResult::
        kVcnRetrievalTryAgainFailure:
    case autofill::AutofillClient::PaymentsRpcResult::
        kVcnRetrievalPermanentFailure:
      NOTREACHED();
      break;
  }

  dialog()->HideProcessingSpinner();
}

std::u16string CvcUnmaskViewController::GetSheetTitle() {
  return l10n_util::GetStringFUTF16(IDS_AUTOFILL_CARD_UNMASK_PROMPT_TITLE,
                                    credit_card_.NetworkAndLastFourDigits());
}

void CvcUnmaskViewController::FillContentView(views::View* content_view) {
  const int vertical_spacing = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_UNRELATED_CONTROL_VERTICAL);
  const gfx::Insets content_insets = gfx::Insets::VH(
      kPaymentRequestRowVerticalInsets, kPaymentRequestRowHorizontalInsets);
  content_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, content_insets,
      vertical_spacing));

  // The prompt for server cards should reference Google Payments, whereas the
  // prompt for local cards should not.
  auto instructions = std::make_unique<views::Label>(l10n_util::GetStringUTF16(
      credit_card_.record_type() == autofill::CreditCard::LOCAL_CARD
          ? IDS_AUTOFILL_CARD_UNMASK_PROMPT_INSTRUCTIONS_LOCAL_CARD
          : IDS_AUTOFILL_CARD_UNMASK_PROMPT_INSTRUCTIONS));
  instructions->SetMultiLine(true);
  instructions->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  content_view->AddChildView(std::move(instructions));

  const int horizontal_spacing = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_CONTROL_HORIZONTAL);
  auto* card_info_view =
      content_view->AddChildView(std::make_unique<views::BoxLayoutView>());
  card_info_view->SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  card_info_view->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  card_info_view->SetBetweenChildSpacing(horizontal_spacing);

  if (credit_card_.ShouldUpdateExpiration()) {
    auto month = std::make_unique<views::Combobox>(&month_combobox_model_);
    month->SetAccessibleName(
        l10n_util::GetStringUTF16(IDS_SETTINGS_CREDIT_CARD_EXPIRATION_MONTH));
    month->SetCallback(base::BindRepeating(
        &CvcUnmaskViewController::OnPerformAction, base::Unretained(this)));
    month->SetID(static_cast<int>(DialogViewID::CVC_MONTH));
    month->SelectValue(credit_card_.Expiration2DigitMonthAsString());
    month->SetInvalid(true);
    month_combobox_ = month.get();
    card_info_view->AddChildView(std::move(month));

    auto year = std::make_unique<views::Combobox>(&year_combobox_model_);
    year->SetAccessibleName(
        l10n_util::GetStringUTF16(IDS_SETTINGS_CREDIT_CARD_EXPIRATION_YEAR));
    year->SetCallback(base::BindRepeating(
        &CvcUnmaskViewController::OnPerformAction, base::Unretained(this)));
    year->SetID(static_cast<int>(DialogViewID::CVC_YEAR));
    year->SelectValue(credit_card_.Expiration4DigitYearAsString());
    year->SetInvalid(true);
    year_combobox_ = year.get();
    card_info_view->AddChildView(std::move(year));
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
  card_info_view->AddChildView(std::move(cvc_image));

  std::unique_ptr<views::Textfield> cvc_field = autofill::CreateCvcTextfield();
  cvc_field->set_controller(this);
  cvc_field->SetID(static_cast<int>(DialogViewID::CVC_PROMPT_TEXT_FIELD));
  cvc_field_ = card_info_view->AddChildView(std::move(cvc_field));

  auto* error_view =
      content_view->AddChildView(std::make_unique<views::BoxLayoutView>());
  error_view->SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  error_view->SetBetweenChildSpacing(horizontal_spacing);

  auto error_icon = std::make_unique<views::ImageView>();
  error_icon->SetID(static_cast<int>(DialogViewID::CVC_ERROR_ICON));
  error_icon->SetImage(ui::ImageModel::FromVectorIcon(
      vector_icons::kWarningIcon, ui::kColorAlertHighSeverity, 16));
  error_icon->SetVisible(false);
  error_view->AddChildView(std::move(error_icon));
  error_view->AddChildView(std::make_unique<ErrorLabelView>());
}

std::u16string CvcUnmaskViewController::GetPrimaryButtonLabel() {
  return l10n_util::GetStringUTF16(IDS_CONFIRM);
}

PaymentRequestSheetController::ButtonCallback
CvcUnmaskViewController::GetPrimaryButtonCallback() {
  return base::BindRepeating(&CvcUnmaskViewController::CvcConfirmed,
                             base::Unretained(this));
}

int CvcUnmaskViewController::GetPrimaryButtonId() {
  return static_cast<int>(DialogViewID::CVC_PROMPT_CONFIRM_BUTTON);
}

bool CvcUnmaskViewController::GetPrimaryButtonEnabled() {
  return false;  // Only enabled when a valid CVC is entered.
}

bool CvcUnmaskViewController::ShouldShowSecondaryButton() {
  // Do not show the "Cancel Payment" button.
  return false;
}

void CvcUnmaskViewController::CvcConfirmed() {
  const std::u16string& cvc = cvc_field_->GetText();
  if (unmask_delegate_) {
    autofill::CardUnmaskDelegate::UserProvidedUnmaskDetails details;
    details.cvc = cvc;
    if (credit_card_.ShouldUpdateExpiration()) {
      views::Combobox* month = static_cast<views::Combobox*>(
          dialog()->GetViewByID(static_cast<int>(DialogViewID::CVC_MONTH)));
      DCHECK(month);
      views::Combobox* year = static_cast<views::Combobox*>(
          dialog()->GetViewByID(static_cast<int>(DialogViewID::CVC_YEAR)));
      DCHECK(year);

      details.exp_month =
          month->GetTextForRow(month->GetSelectedIndex().value());
      details.exp_year = year->GetTextForRow(year->GetSelectedIndex().value());
    }
    unmask_delegate_->OnUnmaskPromptAccepted(details);
  }
}

void CvcUnmaskViewController::DisplayError(std::u16string error) {
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
  std::u16string trimmed_text;
  base::TrimWhitespace(cvc_field_->GetText(), base::TRIM_ALL, &trimmed_text);
  bool cvc_valid = autofill::IsValidCreditCardSecurityCode(
      trimmed_text, credit_card_.network());
  cvc_field_->SetInvalid(!cvc_valid);

  views::Combobox* month = static_cast<views::Combobox*>(
      dialog()->GetViewByID(static_cast<int>(DialogViewID::CVC_MONTH)));
  views::Combobox* year = static_cast<views::Combobox*>(
      dialog()->GetViewByID(static_cast<int>(DialogViewID::CVC_YEAR)));

  bool expiration_date_valid =
      !credit_card_.ShouldUpdateExpiration();

  if (month && year) {
    DCHECK(!expiration_date_valid);
    int month_value = 0;
    int year_value = 0;
    bool parsable =
        base::StringToInt(
            month->GetTextForRow(month->GetSelectedIndex().value()),
            &month_value) &&
        base::StringToInt(year->GetTextForRow(year->GetSelectedIndex().value()),
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

void CvcUnmaskViewController::BackButtonPressed() {
  if (dialog()->IsInteractive() && unmask_delegate_) {
    unmask_delegate_->OnUnmaskPromptClosed();
    dialog()->GoBack();
  }
}

void CvcUnmaskViewController::ContentsChanged(
    views::Textfield* sender,
    const std::u16string& new_contents) {
  UpdatePayButtonState();
}

void CvcUnmaskViewController::OnPerformAction() {
  if (!dialog()->IsInteractive())
    return;

  UpdatePayButtonState();
}

}  // namespace payments
