// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_autofill_client.h"

#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "components/autofill/core/browser/autofill_progress_dialog_type.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/merchant_promo_code_manager.h"
#include "components/autofill/core/browser/payments/autofill_error_dialog_context.h"
#include "components/autofill/core/browser/payments/card_unmask_challenge_option.h"
#include "components/autofill/core/browser/payments/card_unmask_delegate.h"
#include "components/autofill/core/browser/payments/virtual_card_enrollment_manager.h"
#include "components/autofill/core/browser/ui/payments/bubble_show_options.h"
#include "components/autofill/core/browser/ui/payments/card_unmask_prompt_options.h"
#include "components/autofill/core/browser/ui/suggestion.h"

#if !BUILDFLAG(IS_IOS)
#include "components/webauthn/core/browser/internal_authenticator.h"
#endif  // !BUILDFLAG(IS_IOS)

namespace autofill::payments {

PaymentsAutofillClient::~PaymentsAutofillClient() = default;

#if BUILDFLAG(IS_ANDROID)
AutofillSaveCardBottomSheetBridge*
PaymentsAutofillClient::GetOrCreateAutofillSaveCardBottomSheetBridge() {
  return nullptr;
}
#elif !BUILDFLAG(IS_IOS)
void PaymentsAutofillClient::ShowLocalCardMigrationDialog(
    base::OnceClosure show_migration_dialog_closure) {}

void PaymentsAutofillClient::ConfirmMigrateLocalCardToCloud(
    const LegalMessageLines& legal_message_lines,
    const std::string& user_email,
    const std::vector<MigratableCreditCard>& migratable_credit_cards,
    LocalCardMigrationCallback start_migrating_cards_callback) {}

void PaymentsAutofillClient::ShowLocalCardMigrationResults(
    bool has_server_error,
    const std::u16string& tip_message,
    const std::vector<MigratableCreditCard>& migratable_credit_cards,
    MigrationDeleteCardCallback delete_local_card_callback) {}

void PaymentsAutofillClient::ShowWebauthnOfferDialog(
    WebauthnDialogCallback offer_dialog_callback) {}

void PaymentsAutofillClient::ShowWebauthnVerifyPendingDialog(
    WebauthnDialogCallback verify_pending_dialog_callback) {}

void PaymentsAutofillClient::UpdateWebauthnOfferDialogWithError() {}

bool PaymentsAutofillClient::CloseWebauthnDialog() {
  return false;
}

void PaymentsAutofillClient::HideVirtualCardEnrollBubbleAndIconIfVisible() {}
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
void PaymentsAutofillClient::ConfirmAccountNameFixFlow(
    base::OnceCallback<void(const std::u16string&)> callback) {}

void PaymentsAutofillClient::ConfirmExpirationDateFixFlow(
    const CreditCard& card,
    base::OnceCallback<void(const std::u16string&, const std::u16string&)>
        callback) {}
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)

bool PaymentsAutofillClient::HasCreditCardScanFeature() const {
  return false;
}

void PaymentsAutofillClient::ScanCreditCard(CreditCardScanCallback callback) {}

void PaymentsAutofillClient::ConfirmSaveCreditCardLocally(
    const CreditCard& card,
    SaveCreditCardOptions options,
    LocalSaveCardPromptCallback callback) {}

void PaymentsAutofillClient::ConfirmSaveCreditCardToCloud(
    const CreditCard& card,
    const LegalMessageLines& legal_message_lines,
    SaveCreditCardOptions options,
    UploadSaveCardPromptCallback callback) {}

void PaymentsAutofillClient::CreditCardUploadCompleted(
    PaymentsRpcResult result,
    std::optional<OnConfirmationClosedCallback>
        on_confirmation_closed_callback) {}

void PaymentsAutofillClient::HideSaveCardPrompt() {}

void PaymentsAutofillClient::ShowVirtualCardEnrollDialog(
    const VirtualCardEnrollmentFields& virtual_card_enrollment_fields,
    base::OnceClosure accept_virtual_card_callback,
    base::OnceClosure decline_virtual_card_callback) {}

void PaymentsAutofillClient::VirtualCardEnrollCompleted(
    PaymentsRpcResult result) {}

void PaymentsAutofillClient::OnVirtualCardDataAvailable(
    const VirtualCardManualFallbackBubbleOptions& options) {}

void PaymentsAutofillClient::ConfirmSaveIbanLocally(
    const Iban& iban,
    bool should_show_prompt,
    SaveIbanPromptCallback callback) {}

void PaymentsAutofillClient::ConfirmUploadIbanToCloud(
    const Iban& iban,
    LegalMessageLines legal_message_lines,
    bool should_show_prompt,
    SaveIbanPromptCallback callback) {}

void PaymentsAutofillClient::IbanUploadCompleted(bool iban_saved,
                                                 bool hit_max_strikes) {}

void PaymentsAutofillClient::ShowAutofillProgressDialog(
    AutofillProgressDialogType autofill_progress_dialog_type,
    base::OnceClosure cancel_callback) {}

void PaymentsAutofillClient::CloseAutofillProgressDialog(
    bool show_confirmation_before_closing,
    base::OnceClosure no_interactive_authentication_callback) {}

void PaymentsAutofillClient::ShowCardUnmaskOtpInputDialog(
    const CardUnmaskChallengeOption& challenge_option,
    base::WeakPtr<OtpUnmaskDelegate> delegate) {}

void PaymentsAutofillClient::ShowUnmaskAuthenticatorSelectionDialog(
    const std::vector<CardUnmaskChallengeOption>& challenge_options,
    base::OnceCallback<void(const std::string&)>
        confirm_unmask_challenge_option_callback,
    base::OnceClosure cancel_unmasking_closure) {}

void PaymentsAutofillClient::DismissUnmaskAuthenticatorSelectionDialog(
    bool server_success) {}

void PaymentsAutofillClient::OnUnmaskOtpVerificationResult(
    OtpUnmaskResult unmask_result) {}

PaymentsNetworkInterface*
PaymentsAutofillClient::GetPaymentsNetworkInterface() {
  return nullptr;
}

void PaymentsAutofillClient::ShowAutofillErrorDialog(
    AutofillErrorDialogContext context) {}

PaymentsWindowManager* PaymentsAutofillClient::GetPaymentsWindowManager() {
  return nullptr;
}

void PaymentsAutofillClient::ShowUnmaskPrompt(
    const CreditCard& card,
    const CardUnmaskPromptOptions& card_unmask_prompt_options,
    base::WeakPtr<CardUnmaskDelegate> delegate) {}

void PaymentsAutofillClient::OnUnmaskVerificationResult(
    PaymentsRpcResult result) {}

VirtualCardEnrollmentManager*
PaymentsAutofillClient::GetVirtualCardEnrollmentManager() {
  return nullptr;
}

CreditCardOtpAuthenticator* PaymentsAutofillClient::GetOtpAuthenticator() {
  return nullptr;
}

CreditCardRiskBasedAuthenticator*
PaymentsAutofillClient::GetRiskBasedAuthenticator() {
  return nullptr;
}

void PaymentsAutofillClient::ShowMandatoryReauthOptInPrompt(
    base::OnceClosure accept_mandatory_reauth_callback,
    base::OnceClosure cancel_mandatory_reauth_callback,
    base::RepeatingClosure close_mandatory_reauth_callback) {}

IbanManager* PaymentsAutofillClient::GetIbanManager() {
  return nullptr;
}

IbanAccessManager* PaymentsAutofillClient::GetIbanAccessManager() {
  return nullptr;
}

MerchantPromoCodeManager*
PaymentsAutofillClient::GetMerchantPromoCodeManager() {
  return nullptr;
}

void PaymentsAutofillClient::ShowMandatoryReauthOptInConfirmation() {}

void PaymentsAutofillClient::UpdateOfferNotification(
    const AutofillOfferData& offer,
    const OfferNotificationOptions& options) {}

void PaymentsAutofillClient::DismissOfferNotification() {}

void PaymentsAutofillClient::OpenPromoCodeOfferDetailsURL(const GURL& url) {}

AutofillOfferManager* PaymentsAutofillClient::GetAutofillOfferManager() {
  return nullptr;
}

const AutofillOfferManager* PaymentsAutofillClient::GetAutofillOfferManager()
    const {
  return const_cast<PaymentsAutofillClient*>(this)->GetAutofillOfferManager();
}

bool PaymentsAutofillClient::ShowTouchToFillCreditCard(
    base::WeakPtr<TouchToFillDelegate> delegate,
    base::span<const autofill::CreditCard> cards_to_suggest,
    base::span<const Suggestion> suggestions) {
  return false;
}

bool PaymentsAutofillClient::ShowTouchToFillIban(
    base::WeakPtr<TouchToFillDelegate> delegate,
    base::span<const autofill::Iban> ibans_to_suggest) {
  return false;
}

void PaymentsAutofillClient::HideTouchToFillPaymentMethod() {}

#if !BUILDFLAG(IS_IOS)
std::unique_ptr<webauthn::InternalAuthenticator>
PaymentsAutofillClient::CreateCreditCardInternalAuthenticator(
    AutofillDriver* driver) {
  return nullptr;
}
#endif

payments::MandatoryReauthManager*
PaymentsAutofillClient::GetOrCreatePaymentsMandatoryReauthManager() {
  return nullptr;
}

}  // namespace autofill::payments
