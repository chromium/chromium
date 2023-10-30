// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_client.h"

#include "base/no_destructor.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_ablation_study.h"
#include "components/autofill/core/browser/autofill_compose_delegate.h"
#include "components/autofill/core/browser/payments/credit_card_access_manager.h"
#include "components/autofill/core/browser/payments/mandatory_reauth_manager.h"
#include "components/autofill/core/browser/payments/virtual_card_enrollment_manager.h"
#include "components/autofill/core/browser/single_field_form_fill_router.h"
#include "components/autofill/core/browser/ui/payments/bubble_show_options.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/plus_addresses/plus_address_types.h"
#include "components/version_info/channel.h"

namespace autofill {

AutofillClient::PopupOpenArgs::PopupOpenArgs() = default;
AutofillClient::PopupOpenArgs::PopupOpenArgs(
    const gfx::RectF& element_bounds,
    base::i18n::TextDirection text_direction,
    std::vector<Suggestion> suggestions,
    AutofillSuggestionTriggerSource trigger_source)
    : element_bounds(element_bounds),
      text_direction(text_direction),
      suggestions(std::move(suggestions)),
      trigger_source(trigger_source) {}
AutofillClient::PopupOpenArgs::PopupOpenArgs(
    const AutofillClient::PopupOpenArgs&) = default;
AutofillClient::PopupOpenArgs::PopupOpenArgs(AutofillClient::PopupOpenArgs&&) =
    default;
AutofillClient::PopupOpenArgs::~PopupOpenArgs() = default;
AutofillClient::PopupOpenArgs& AutofillClient::PopupOpenArgs::operator=(
    const AutofillClient::PopupOpenArgs&) = default;
AutofillClient::PopupOpenArgs& AutofillClient::PopupOpenArgs::operator=(
    AutofillClient::PopupOpenArgs&&) = default;

version_info::Channel AutofillClient::GetChannel() const {
  return version_info::Channel::UNKNOWN;
}

bool AutofillClient::IsOffTheRecord() {
  return false;
}

AutofillDownloadManager* AutofillClient::GetDownloadManager() {
  return nullptr;
}

const PersonalDataManager* AutofillClient::GetPersonalDataManager() const {
  return const_cast<AutofillClient*>(this)->GetPersonalDataManager();
}

AutofillOptimizationGuide* AutofillClient::GetAutofillOptimizationGuide()
    const {
  return nullptr;
}

AutofillMlPredictionModelHandler*
AutofillClient::GetAutofillMlPredictionModelHandler() {
  return nullptr;
}

IbanManager* AutofillClient::GetIbanManager() {
  return nullptr;
}

AutofillComposeDelegate* AutofillClient::GetComposeDelegate() {
  return nullptr;
}

plus_addresses::PlusAddressService* AutofillClient::GetPlusAddressService() {
  return nullptr;
}

void AutofillClient::OfferPlusAddressCreation(
    const url::Origin& main_frame_origin,
    plus_addresses::PlusAddressCallback callback) {
  // This is overridden by platform subclasses. Currently only
  // ChromeAutofillClient (Chrome Desktop & Android) implements this, with iOS
  // support also expected.
}

MerchantPromoCodeManager* AutofillClient::GetMerchantPromoCodeManager() {
  return nullptr;
}

std::unique_ptr<SingleFieldFormFillRouter>
AutofillClient::CreateSingleFieldFormFillRouter() {
  return std::make_unique<SingleFieldFormFillRouter>(
      GetAutocompleteHistoryManager(), GetIbanManager(),
      GetMerchantPromoCodeManager());
}

CreditCardCvcAuthenticator* AutofillClient::GetCvcAuthenticator() {
  return nullptr;
}

CreditCardOtpAuthenticator* AutofillClient::GetOtpAuthenticator() {
  return nullptr;
}

CreditCardRiskBasedAuthenticator* AutofillClient::GetRiskBasedAuthenticator() {
  return nullptr;
}

AutofillOfferManager* AutofillClient::GetAutofillOfferManager() {
  return nullptr;
}

GeoIpCountryCode AutofillClient::GetVariationConfigCountryCode() const {
  return GeoIpCountryCode(std::string());
}

profile_metrics::BrowserProfileType AutofillClient::GetProfileType() const {
  // This is an abstract interface and thus never instantiated directly,
  // therefore it is safe to always return |kRegular| here.
  return profile_metrics::BrowserProfileType::kRegular;
}

FastCheckoutClient* AutofillClient::GetFastCheckoutClient() {
  return nullptr;
}

void AutofillClient::ShowUnmaskAuthenticatorSelectionDialog(
    const std::vector<CardUnmaskChallengeOption>& challenge_options,
    base::OnceCallback<void(const std::string&)>
        confirm_unmask_challenge_option_callback,
    base::OnceClosure cancel_unmasking_closure) {
  // This is overridden by platform subclasses. Currently only
  // ChromeAutofillClient (Chrome Desktop and Clank) implements this.
}

void AutofillClient::DismissUnmaskAuthenticatorSelectionDialog(
    bool server_success) {
  // This is overridden by platform subclasses. Currently only
  // ChromeAutofillClient (Chrome Desktop and Clank) implements this.
}

VirtualCardEnrollmentManager*
AutofillClient::GetVirtualCardEnrollmentManager() {
  // This is overridden by platform subclasses. Currently only
  // ChromeAutofillClient (Chrome Desktop and Clank) implements this.
  return nullptr;
}

void AutofillClient::ShowVirtualCardEnrollDialog(
    const VirtualCardEnrollmentFields& virtual_card_enrollment_fields,
    base::OnceClosure accept_virtual_card_callback,
    base::OnceClosure decline_virtual_card_callback) {
  // This is overridden by platform subclasses. Currently only
  // ChromeAutofillClient (Chrome Desktop and Clank) implements this.
}

payments::MandatoryReauthManager*
AutofillClient::GetOrCreatePaymentsMandatoryReauthManager() {
  return nullptr;
}

void AutofillClient::ShowMandatoryReauthOptInPrompt(
    base::OnceClosure accept_mandatory_reauth_callback,
    base::OnceClosure cancel_mandatory_reauth_callback,
    base::RepeatingClosure close_mandatory_reauth_callback) {}

void AutofillClient::ShowMandatoryReauthOptInConfirmation() {}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
void AutofillClient::HideVirtualCardEnrollBubbleAndIconIfVisible() {
  // This is overridden by platform subclasses. Currently only
  // ChromeAutofillClient (Chrome Desktop) implements this.
}

void AutofillClient::ShowLocalCardMigrationDialog(
    base::OnceClosure show_migration_dialog_closure) {
  // This is overridden by platform subclasses
}

void AutofillClient::ConfirmMigrateLocalCardToCloud(
    const LegalMessageLines& legal_message_lines,
    const std::string& user_email,
    const std::vector<MigratableCreditCard>& migratable_credit_cards,
    LocalCardMigrationCallback start_migrating_cards_callback) {
  // This is overridden by platform subclasses
}

void AutofillClient::ShowLocalCardMigrationResults(
    const bool has_server_error,
    const std::u16string& tip_message,
    const std::vector<MigratableCreditCard>& migratable_credit_cards,
    MigrationDeleteCardCallback delete_local_card_callback) {
  // This is overridden by platform subclasses.
}

void AutofillClient::ShowWebauthnOfferDialog(
    WebauthnDialogCallback offer_dialog_callback) {
  // This is overridden by platform subclasses.
}

void AutofillClient::ShowWebauthnVerifyPendingDialog(
    WebauthnDialogCallback verify_pending_dialog_callback) {
  // This is overridden by platform subclasses.
}

void AutofillClient::UpdateWebauthnOfferDialogWithError() {
  // This is overridden by platform subclasses.
}

bool AutofillClient::CloseWebauthnDialog() {
  // This is overridden by platform subclasses.
  return false;
}
#else
void AutofillClient::ConfirmAccountNameFixFlow(
    base::OnceCallback<void(const std::u16string&)> callback) {
  // This is overridden by platform subclasses.
}

void AutofillClient::ConfirmExpirationDateFixFlow(
    const CreditCard& card,
    base::OnceCallback<void(const std::u16string&, const std::u16string&)>
        callback) {
  // This is overridden by platform subclasses.
}
#endif

#if !BUILDFLAG(IS_IOS)
std::unique_ptr<webauthn::InternalAuthenticator>
AutofillClient::CreateCreditCardInternalAuthenticator(AutofillDriver* driver) {
  return nullptr;
}
#endif

void AutofillClient::ShowCardUnmaskOtpInputDialog(
    const CardUnmaskChallengeOption& challenge_option,
    base::WeakPtr<OtpUnmaskDelegate> delegate) {
  // This is overridden by platform subclasses. Currently only
  // ChromeAutofillClient (Chrome Desktop and Clank) implements this.
}

void AutofillClient::OnUnmaskOtpVerificationResult(
    OtpUnmaskResult unmask_result) {
  // This is overridden by platform subclasses. Currently only
  // ChromeAutofillClient (Chrome Desktop and Clank) implements this.
}

void AutofillClient::ConfirmSaveCreditCardLocally(
    const CreditCard& card,
    AutofillClient::SaveCreditCardOptions options,
    LocalSaveCardPromptCallback callback) {
  // This is overridden by platform subclasses.
}

void AutofillClient::ConfirmSaveCreditCardToCloud(
    const CreditCard& card,
    const LegalMessageLines& legal_message_lines,
    SaveCreditCardOptions options,
    UploadSaveCardPromptCallback callback) {
  // This is overridden by platform subclasses.
}

void AutofillClient::CreditCardUploadCompleted(bool card_saved) {
  // This is overridden by platform subclasses.
}

void AutofillClient::ShowUnmaskPrompt(
    const CreditCard& card,
    const CardUnmaskPromptOptions& card_unmask_prompt_options,
    base::WeakPtr<CardUnmaskDelegate> delegate) {
  // This is overridden by platform subclasses.
}

void AutofillClient::OnUnmaskVerificationResult(PaymentsRpcResult result) {
  // This is overridden by platform subclasses.
}

void AutofillClient::UpdateOfferNotification(
    const AutofillOfferData* offer,
    const OfferNotificationOptions& options) {
  // This is overridden by platform subclasses. Currently only
  // ChromeAutofillClient (Chrome Desktop and Clank) implement this.
}

void AutofillClient::DismissOfferNotification() {
  // This is overridden by platform subclasses. Currently only
  // ChromeAutofillClient (Chrome Desktop and Clank) implements this.
}

void AutofillClient::OnVirtualCardDataAvailable(
    const VirtualCardManualFallbackBubbleOptions& options) {
  // This is overridden by platform subclasses. Currently only
  // ChromeAutofillClient (Chrome Desktop & Android) implements this.
}

void AutofillClient::ShowAutofillErrorDialog(
    const AutofillErrorDialogContext& context) {
  // This is overridden by platform subclasses. Currently only
  // ChromeAutofillClient (Chrome Desktop & Android) implements this.
}

void AutofillClient::ShowAutofillProgressDialog(
    AutofillProgressDialogType autofill_progress_dialog_type,
    base::OnceClosure cancel_callback) {
  // This is overridden by platform subclasses. Currently only
  // ChromeAutofillClient (Chrome Desktop & Android) implements this.
}

void AutofillClient::CloseAutofillProgressDialog(
    bool show_confirmation_before_closing,
    base::OnceClosure no_interactive_authentication_callback) {
  // This is overridden by platform subclasses. Currently only
  // ChromeAutofillClient (Chrome Desktop & Android) implements this.
}

LogManager* AutofillClient::GetLogManager() const {
  return nullptr;
}

const AutofillAblationStudy& AutofillClient::GetAblationStudy() const {
  // As finch configs are profile independent we can use a static instance here.
  static base::NoDestructor<AutofillAblationStudy> ablation_study;
  return *ablation_study;
}

std::unique_ptr<device_reauth::DeviceAuthenticator>
AutofillClient::GetDeviceAuthenticator() {
  return nullptr;
}

std::optional<AutofillClient::PopupScreenLocation>
AutofillClient::GetPopupScreenLocation() const {
  NOTIMPLEMENTED();
  return std::nullopt;
}

}  // namespace autofill
