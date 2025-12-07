// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/foundations/autofill_client.h"

#include <optional>
#include <utility>

#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/notimplemented.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/filling/filling_product.h"
#include "components/autofill/core/browser/integrators/autofill_ai/autofill_ai_manager.h"
#include "components/autofill/core/browser/integrators/compose/autofill_compose_delegate.h"
#include "components/autofill/core/browser/integrators/identity_credential/identity_credential_delegate.h"
#include "components/autofill/core/browser/integrators/password_manager/password_manager_delegate.h"
#include "components/autofill/core/browser/integrators/plus_addresses/autofill_plus_address_delegate.h"
#include "components/autofill/core/browser/payments/credit_card_access_manager.h"
#include "components/autofill/core/browser/studies/autofill_ablation_study.h"
#include "components/autofill/core/browser/studies/autofill_experiments.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/ui/popup_open_enums.h"
#include "components/optimization_guide/core/model_execution/remote_model_executor.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
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

const EntityDataManager* AutofillClient::GetEntityDataManager() const {
  return const_cast<AutofillClient*>(this)->GetEntityDataManager();
}

bool AutofillClient::HasPersonalDataManager() const {
  return true;
}

const PersonalDataManager& AutofillClient::GetPersonalDataManager() const {
  return const_cast<AutofillClient*>(this)->GetPersonalDataManager();
}

const ValuablesDataManager* AutofillClient::GetValuablesDataManager() const {
  return const_cast<AutofillClient*>(this)->GetValuablesDataManager();
}

AutofillOptimizationGuideDecider*
AutofillClient::GetAutofillOptimizationGuideDecider() const {
  return nullptr;
}

FieldClassificationModelHandler*
AutofillClient::GetAutofillFieldClassificationModelHandler() {
  return nullptr;
}

FieldClassificationModelHandler*
AutofillClient::GetPasswordManagerFieldClassificationModelHandler() {
  return nullptr;
}

AutofillComposeDelegate* AutofillClient::GetComposeDelegate() {
  return nullptr;
}

AutofillPlusAddressDelegate* AutofillClient::GetPlusAddressDelegate() {
  return nullptr;
}

PasswordManagerDelegate* AutofillClient::GetPasswordManagerDelegate(
    const FieldGlobalId& field_id) {
  return nullptr;
}

void AutofillClient::GetAiPageContent(GetAiPageContentCallback callback) {
  std::move(callback).Run(std::nullopt);
}

AutofillAiManager* AutofillClient::GetAutofillAiManager() {
  return nullptr;
}

AutofillAiModelCache* AutofillClient::GetAutofillAiModelCache() {
  return nullptr;
}

AutofillAiModelExecutor* AutofillClient::GetAutofillAiModelExecutor() {
  return nullptr;
}

optimization_guide::RemoteModelExecutor*
AutofillClient::GetRemoteModelExecutor() {
  return nullptr;
}

IdentityCredentialDelegate* AutofillClient::GetIdentityCredentialDelegate() {
  return nullptr;
}

const GoogleGroupsManager* AutofillClient::GetGoogleGroupsManager() const {
  return nullptr;
}

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

LogManager* AutofillClient::GetCurrentLogManager() {
  return nullptr;
}

bool AutofillClient::ShouldFormatForLargeKeyboardAccessory() const {
  return false;
}

const AutofillAblationStudy& AutofillClient::GetAblationStudy() const {
  return AutofillAblationStudy::disabled_study();
}

#if BUILDFLAG(IS_ANDROID)
AutofillSnackbarControllerImpl*
AutofillClient::GetAutofillSnackbarController() {
  return nullptr;
}
#endif

void AutofillClient::TriggerUserPerceptionOfAutofillSurvey(
    FillingProduct filling_product,
    const std::map<std::string, std::string>& field_filling_stats_data) {
  NOTIMPLEMENTED();
}

void AutofillClient::TriggerDeclinedSaveAddressReasonSurvey() {
  NOTIMPLEMENTED();
}

void AutofillClient::TriggerAutofillAiFillingJourneySurvey(
    bool suggestion_accepted,
    EntityType entity_type,
    const base::flat_set<EntityTypeName>& saved_entities,
    const FieldTypeSet& triggering_field_types) {
  NOTIMPLEMENTED();
}

void AutofillClient::TriggerAutofillAiSavePromptSurvey(
    bool prompt_accepted,
    EntityType entity_type,
    const base::flat_set<EntityTypeName>& saved_entities) {
  NOTIMPLEMENTED();
}

bool AutofillClient::IsActorTaskActive() const {
  return false;
}

std::unique_ptr<device_reauth::DeviceAuthenticator>
AutofillClient::GetDeviceAuthenticator() {
  return nullptr;
}

void AutofillClient::ShowPlusAddressEmailOverrideNotification(
    const std::string& original_email,
    EmailOverrideUndoCallback email_override_undo_callback) {}

bool AutofillClient::ShowAutofillFieldIphForFeature(
    const FormFieldData&,
    AutofillClient::IphFeature feature) {
  return false;
}

void AutofillClient::HideAutofillFieldIph() {}

void AutofillClient::NotifyIphFeatureUsed(AutofillClient::IphFeature feature) {}

std::optional<AutofillClient::PopupScreenLocation>
AutofillClient::GetPopupScreenLocation() const {
  NOTIMPLEMENTED();
  return std::nullopt;
}

std::optional<AutofillClient::SuggestionUiSessionId>
AutofillClient::GetSessionIdForCurrentAutofillSuggestions() const {
  return std::nullopt;
}

base::span<const Suggestion> AutofillClient::GetAutofillSuggestions() const {
  NOTIMPLEMENTED();
  return {};
}

void AutofillClient::UpdateAutofillSuggestions(
    const std::vector<Suggestion>& suggestions,
    FillingProduct main_filling_product,
    AutofillSuggestionTriggerSource trigger_source) {
  NOTIMPLEMENTED();
}

bool AutofillClient::IsCvcSavingSupported() const {
  return true;
}

bool AutofillClient::IsCreditCardUploadEnabled() const {
  return ::autofill::IsCreditCardUploadEnabled(
      GetSyncService(), *GetPrefs(),
      GetPersonalDataManager()
          .payments_data_manager()
          .GetCountryCodeForExperimentGroup(),
      GetPersonalDataManager()
          .payments_data_manager()
          .GetPaymentsSigninStateForMetrics(),
      const_cast<AutofillClient*>(this)->GetCurrentLogManager());
}

void AutofillClient::set_test_addresses(
    std::vector<AutofillProfile> test_addresses) {}

base::span<const AutofillProfile> AutofillClient::GetTestAddresses() const {
  return {};
}

PasswordFormClassification AutofillClient::ClassifyAsPasswordForm(
    AutofillManager& manager,
    FormGlobalId form_id,
    FieldGlobalId field_id) const {
  return {};
}

void AutofillClient::TriggerPlusAddressUserPerceptionSurvey(
    plus_addresses::hats::SurveyType survey_type) {}

const syncer::SyncService* AutofillClient::GetSyncService() const {
  return const_cast<const syncer::SyncService*>(
      const_cast<AutofillClient*>(this)->GetSyncService());
}

optimization_guide::ModelQualityLogsUploaderService*
AutofillClient::GetMqlsUploadService() {
  return nullptr;
}

void AutofillClient::ShowEntityImportBubble(
    EntityInstance new_entity,
    std::optional<EntityInstance> old_entity,
    EntityImportPromptResultCallback prompt_closed_callback) {}

void AutofillClient::ShowEmailVerifiedToast() {
  NOTIMPLEMENTED();
}

OtpFieldDetector* AutofillClient::GetOtpFieldDetector() {
  return nullptr;
}

one_time_tokens::OneTimeTokenService* AutofillClient::GetOneTimeTokenService()
    const {
  return nullptr;
}

bool AutofillClient::DocumentUsedWebOTP() {
  return false;
}

PasswordManagerAutofillHelperDelegate*
AutofillClient::GetPasswordManagerAutofillHelper() {
  return nullptr;
}

AutofillManager* AutofillClient::GetAutofillManagerForPrimaryMainFrame() {
  return nullptr;
}

OtpPhishGuardDelegate* AutofillClient::GetOtpPhishGuardDelegate() {
  return nullptr;
}

}  // namespace autofill
