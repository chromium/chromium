// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_client.h"

#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_ablation_study.h"
#include "components/autofill/core/browser/autofill_compose_delegate.h"
#include "components/autofill/core/browser/autofill_plus_address_delegate.h"
#include "components/autofill/core/browser/filling_product.h"
#include "components/autofill/core/browser/payments/credit_card_access_manager.h"
#include "components/autofill/core/browser/payments/mandatory_reauth_manager.h"
#include "components/autofill/core/browser/ui/payments/bubble_show_options.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/version_info/channel.h"

namespace autofill {

AutofillClient::PopupOpenArgs::PopupOpenArgs() = default;
AutofillClient::PopupOpenArgs::PopupOpenArgs(
    const gfx::RectF& element_bounds,
    base::i18n::TextDirection text_direction,
    std::vector<Suggestion> suggestions,
    AutofillSuggestionTriggerSource trigger_source,
    int32_t form_control_ax_id)
    : element_bounds(element_bounds),
      text_direction(text_direction),
      suggestions(std::move(suggestions)),
      trigger_source(trigger_source),
      form_control_ax_id(form_control_ax_id) {}
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

bool AutofillClient::IsOffTheRecord() const {
  return false;
}

AutofillCrowdsourcingManager* AutofillClient::GetCrowdsourcingManager() {
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

IbanAccessManager* AutofillClient::GetIbanAccessManager() {
  return nullptr;
}

AutofillComposeDelegate* AutofillClient::GetComposeDelegate() {
  return nullptr;
}

AutofillPlusAddressDelegate* AutofillClient::GetPlusAddressDelegate() {
  return nullptr;
}

void AutofillClient::OfferPlusAddressCreation(
    const url::Origin& main_frame_origin,
    PlusAddressCallback callback) {}

MerchantPromoCodeManager* AutofillClient::GetMerchantPromoCodeManager() {
  return nullptr;
}

payments::PaymentsAutofillClient* AutofillClient::GetPaymentsAutofillClient() {
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
void AutofillClient::ShowVirtualCardEnrollDialog(
    const VirtualCardEnrollmentFields& virtual_card_enrollment_fields,
    base::OnceClosure accept_virtual_card_callback,
    base::OnceClosure decline_virtual_card_callback) {
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
}

void AutofillClient::ShowWebauthnOfferDialog(
    WebauthnDialogCallback offer_dialog_callback) {
}

void AutofillClient::ShowWebauthnVerifyPendingDialog(
    WebauthnDialogCallback verify_pending_dialog_callback) {
}

void AutofillClient::UpdateWebauthnOfferDialogWithError() {
}

bool AutofillClient::CloseWebauthnDialog() {
  return false;
}

#else
void AutofillClient::ConfirmAccountNameFixFlow(
    base::OnceCallback<void(const std::u16string&)> callback) {
}

void AutofillClient::ConfirmExpirationDateFixFlow(
    const CreditCard& card,
    base::OnceCallback<void(const std::u16string&, const std::u16string&)>
        callback) {
}
#endif

#if !BUILDFLAG(IS_IOS)
std::unique_ptr<webauthn::InternalAuthenticator>
AutofillClient::CreateCreditCardInternalAuthenticator(AutofillDriver* driver) {
  return nullptr;
}
#endif

void AutofillClient::ConfirmSaveCreditCardLocally(
    const CreditCard& card,
    AutofillClient::SaveCreditCardOptions options,
    LocalSaveCardPromptCallback callback) {
}

void AutofillClient::ConfirmSaveCreditCardToCloud(
    const CreditCard& card,
    const LegalMessageLines& legal_message_lines,
    SaveCreditCardOptions options,
    UploadSaveCardPromptCallback callback) {
}

bool AutofillClient::ShowTouchToFillIban(
    base::WeakPtr<TouchToFillDelegate> delegate,
    base::span<const autofill::Iban> ibans_to_suggest) {
  return false;
}

void AutofillClient::UpdateOfferNotification(
    const AutofillOfferData* offer,
    const OfferNotificationOptions& options) {
}

void AutofillClient::DismissOfferNotification() {
}

void AutofillClient::OnVirtualCardDataAvailable(
    const VirtualCardManualFallbackBubbleOptions& options) {
}

LogManager* AutofillClient::GetLogManager() const {
  return nullptr;
}

const AutofillAblationStudy& AutofillClient::GetAblationStudy() const {
  // As finch configs are profile independent we can use a static instance here.
  static base::NoDestructor<AutofillAblationStudy> ablation_study;
  return *ablation_study;
}

void AutofillClient::OpenPromoCodeOfferDetailsURL(const GURL& url) {
  NOTIMPLEMENTED();
}

bool AutofillClient::ShouldFormatForLargeKeyboardAccessory() const {
  return false;
}

void AutofillClient::TriggerUserPerceptionOfAutofillSurvey(
    FillingProduct filling_product,
    const std::map<std::string, std::string>& field_filling_stats_data) {
  NOTIMPLEMENTED();
}

std::unique_ptr<device_reauth::DeviceAuthenticator>
AutofillClient::GetDeviceAuthenticator() {
  return nullptr;
}

void AutofillClient::ShowAutofillFieldIphForManualFallbackFeature(
    const FormFieldData&) {}

void AutofillClient::HideAutofillFieldIphForManualFallbackFeature() {}

void AutofillClient::NotifyAutofillManualFallbackUsed() {}

std::optional<AutofillClient::PopupScreenLocation>
AutofillClient::GetPopupScreenLocation() const {
  NOTIMPLEMENTED();
  return std::nullopt;
}

base::span<const Suggestion> AutofillClient::GetAutofillSuggestions() const {
  NOTIMPLEMENTED();
  return {};
}

void AutofillClient::set_test_addresses(
    std::vector<AutofillProfile> test_addresses) {}

base::span<const AutofillProfile> AutofillClient::GetTestAddresses() const {
  return {};
}

}  // namespace autofill
