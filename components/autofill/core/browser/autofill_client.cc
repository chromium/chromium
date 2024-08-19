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
#include "components/autofill/core/browser/autofill_prediction_improvements_delegate.h"
#include "components/autofill/core/browser/filling_product.h"
#include "components/autofill/core/browser/payments/credit_card_access_manager.h"
#include "components/autofill/core/browser/ui/popup_open_enums.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/version_info/channel.h"

namespace autofill {

AutofillClient::PopupOpenArgs::PopupOpenArgs() = default;
AutofillClient::PopupOpenArgs::PopupOpenArgs(
    const gfx::RectF& element_bounds,
    base::i18n::TextDirection text_direction,
    std::vector<Suggestion> suggestions,
    AutofillSuggestionTriggerSource trigger_source,
    int32_t form_control_ax_id,
    PopupAnchorType anchor_type)
    : element_bounds(element_bounds),
      text_direction(text_direction),
      suggestions(std::move(suggestions)),
      trigger_source(trigger_source),
      form_control_ax_id(form_control_ax_id),
      anchor_type(anchor_type) {}
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

AutofillComposeDelegate* AutofillClient::GetComposeDelegate() {
  return nullptr;
}

AutofillPlusAddressDelegate* AutofillClient::GetPlusAddressDelegate() {
  return nullptr;
}

AutofillPredictionImprovementsDelegate*
AutofillClient::GetAutofillPredictionImprovementsDelegate() {
  return nullptr;
}

void AutofillClient::OfferPlusAddressCreation(
    const url::Origin& main_frame_origin,
    PlusAddressCallback callback) {}

payments::PaymentsAutofillClient* AutofillClient::GetPaymentsAutofillClient() {
  return nullptr;
}

const payments::PaymentsAutofillClient*
AutofillClient::GetPaymentsAutofillClient() const {
  // Gets a pointer to a non-const implementation of
  // payments::PaymentsAutofillClient for the given platform this is called on,
  // which is then converted to a pointer to a const implementation. The
  // implementation returned will already be an existing object that is created
  // when the given implementation of AutofillClient is created. If there is no
  // payments::PaymentsAutofillClient for a given platform this will return
  // nullptr.
  return const_cast<AutofillClient*>(this)->GetPaymentsAutofillClient();
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

#if !BUILDFLAG(IS_IOS)
std::unique_ptr<webauthn::InternalAuthenticator>
AutofillClient::CreateCreditCardInternalAuthenticator(AutofillDriver* driver) {
  return nullptr;
}
#endif

LogManager* AutofillClient::GetLogManager() const {
  return nullptr;
}

bool AutofillClient::ShouldFormatForLargeKeyboardAccessory() const {
  return false;
}

const AutofillAblationStudy& AutofillClient::GetAblationStudy() const {
  return AutofillAblationStudy::disabled_study();
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

AutofillClient::PasswordFormClassification
AutofillClient::ClassifyAsPasswordForm(AutofillManager& manager,
                                       FormGlobalId form_id,
                                       FieldGlobalId field_id) const {
  return {};
}

}  // namespace autofill
