// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/saved_info_handler.h"

#include "base/functional/bind.h"
#include "chrome/browser/affiliations/affiliation_service_factory.h"
#include "chrome/browser/autofill/autofill_image_fetcher_factory.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/autofill/valuables_data_manager_factory.h"
#include "chrome/browser/password_manager/account_password_store_factory.h"
#include "chrome/browser/password_manager/profile_password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/webauthn/passkey_model_factory.h"
#include "chrome/common/webui_url_constants.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/data_manager/valuables/valuables_data_manager.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "content/public/browser/web_ui.h"

namespace settings {

SavedInfoHandler::SavedInfoHandler(Profile* profile) : profile_(profile) {}

SavedInfoHandler::~SavedInfoHandler() = default;

void SavedInfoHandler::RegisterMessages() {
  if (profile_->IsOffTheRecord()) {
    return;
  }

  web_ui()->RegisterMessageCallback(
      "getPasswordCount",
      base::BindRepeating(&SavedInfoHandler::HandleGetPasswordCount,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getLoyaltyCardsCount",
      base::BindRepeating(&SavedInfoHandler::HandleGetLoyaltyCardsCount,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "requestDataManagementSurvey",
      base::BindRepeating(&SavedInfoHandler::HandleRequestDataManagementSurvey,
                          base::Unretained(this)));
}

void SavedInfoHandler::OnJavascriptAllowed() {
  if (!saved_passwords_presenter_) {
    auto* affiliation_service =
        AffiliationServiceFactory::GetForProfile(profile_);
    auto* passkey_model = PasskeyModelFactory::GetForProfile(profile_);
    if (!affiliation_service || !passkey_model) {
      return;
    }
    saved_passwords_presenter_ =
        std::make_unique<password_manager::SavedPasswordsPresenter>(
            affiliation_service,
            ProfilePasswordStoreFactory::GetForProfile(
                profile_, ServiceAccessType::EXPLICIT_ACCESS),
            AccountPasswordStoreFactory::GetForProfile(
                profile_, ServiceAccessType::EXPLICIT_ACCESS),
            passkey_model);
    saved_passwords_presenter_->Init();
  }

  password_observation_.Reset();
  password_observation_.Observe(saved_passwords_presenter_.get());

  auto* passkey_model = PasskeyModelFactory::GetForProfile(profile_);
  passkey_observation_.Reset();
  passkey_observation_.Observe(passkey_model);

  if (autofill::ValuablesDataManager* valuables_data_manager =
          autofill::ValuablesDataManagerFactory::GetForProfile(profile_)) {
    valuables_data_manager_observation_.Reset();
    valuables_data_manager_observation_.Observe(valuables_data_manager);
  }
}

void SavedInfoHandler::OnJavascriptDisallowed() {
  password_observation_.Reset();
  passkey_observation_.Reset();
  valuables_data_manager_observation_.Reset();
  if (saved_passwords_presenter_) {
    saved_passwords_presenter_.reset();
  }
}

void SavedInfoHandler::OnSavedPasswordsChanged(
    const password_manager::PasswordStoreChangeList& changes) {
  FireWebUIListener("password-count-changed", GetPasswordCounts());
}

void SavedInfoHandler::OnPasskeysChanged(
    const std::vector<webauthn::PasskeyModelChange>& changes) {
  FireWebUIListener("password-count-changed", GetPasswordCounts());
}

void SavedInfoHandler::OnValuablesDataChanged() {
  FireWebUIListener("loyalty-cards-count-changed", GetLoyaltyCardsCount());
}

base::DictValue SavedInfoHandler::GetPasswordCounts() {
  base::DictValue dict;
  auto* passwords_presenter = password_observation_.GetSource();
  if (passwords_presenter) {
    const size_t password_count = passwords_presenter->GetSavedPasswords().size();
    dict.Set("passwordCount", static_cast<int>(password_count));
  }
  auto* passkey_model = passkey_observation_.GetSource();
  if (passkey_model) {
    const size_t passkey_count =
        passkey_model
            ->GetPasskeys(webauthn::PasskeyModel::AnyRp(),
                          webauthn::PasskeyModel::ShadowedCredentials::kInclude)
            .size();
    dict.Set("passkeyCount", static_cast<int>(passkey_count));
  }
  return dict;
}

void SavedInfoHandler::HandleGetPasswordCount(const base::ListValue& args) {
  AllowJavascript();
  const base::Value& callback_id = args[0];
  ResolveJavascriptCallback(callback_id, GetPasswordCounts());
}

base::Value SavedInfoHandler::GetLoyaltyCardsCount() {
  autofill::ValuablesDataManager* valuables_data_manager =
      autofill::ValuablesDataManagerFactory::GetForProfile(profile_);
  if (!valuables_data_manager) {
    return base::Value();
  }
  size_t loyalty_cards_count = valuables_data_manager->GetLoyaltyCards().size();
  return base::Value(static_cast<int>(loyalty_cards_count));
}

void SavedInfoHandler::HandleGetLoyaltyCardsCount(const base::ListValue& args) {
  AllowJavascript();
  const base::Value& callback_id = args[0];
  ResolveJavascriptCallback(callback_id, GetLoyaltyCardsCount());
}

// Type of HaTS survey, used to gauge user perception on a data management
// surface.
// LINT.IfChange(DataManagementSurvey)
enum class DataManagementSurvey {
  kYourSavedInfo = 0,
  kPasswords = 1,
  kPayments = 2,
  kContactInfo = 3,
  kIdentityDocs = 4,
  kTravel = 5,
};
// LINT.ThenChange(/chrome/browser/resources/settings/your_saved_info_page/saved_info_handler_proxy.ts:DataManagementSurvey)

const std::string GetManagementSurveyTrigger(DataManagementSurvey survey) {
  switch (survey) {
    case DataManagementSurvey::kYourSavedInfo:
      return kHatsSurveyTriggerManageYourSavedInfoPerception;
    case DataManagementSurvey::kPasswords:
      return kHatsSurveyTriggerManagePasswordsPerception;
    case DataManagementSurvey::kPayments:
      return kHatsSurveyTriggerManagePaymentsPerception;
    case DataManagementSurvey::kContactInfo:
      return kHatsSurveyTriggerManageContactInfoPerception;
    case DataManagementSurvey::kIdentityDocs:
      return kHatsSurveyTriggerManageIdentityDocsPerception;
    case DataManagementSurvey::kTravel:
      return kHatsSurveyTriggerManageTravelPerception;
  }
  NOTREACHED();
}

const base::Feature& GetManagementSurveyFeature(DataManagementSurvey survey) {
  switch (survey) {
    case DataManagementSurvey::kYourSavedInfo:
      return autofill::features::kManageYourSavedInfoPerceptionSurvey;
    case DataManagementSurvey::kPasswords:
      return autofill::features::kManagePasswordsPerceptionSurvey;
    case DataManagementSurvey::kPayments:
      return autofill::features::kManagePaymentsPerceptionSurvey;
    case DataManagementSurvey::kContactInfo:
      return autofill::features::kManageContactInfoPerceptionSurvey;
    case DataManagementSurvey::kIdentityDocs:
      return autofill::features::kManageIdentityDocsPerceptionSurvey;
    case DataManagementSurvey::kTravel:
      return autofill::features::kManageTravelPerceptionSurvey;
  }
  NOTREACHED();
}

void SavedInfoHandler::HandleRequestDataManagementSurvey(
    const base::ListValue& args) {
  AllowJavascript();
  CHECK_EQ(2U, args.size());
  auto survey = static_cast<DataManagementSurvey>(args[0].GetInt());
  bool is_from_home_page = args[1].GetBool();

  auto& feature = GetManagementSurveyFeature(survey);
  if (!base::FeatureList::IsEnabled(feature) ||
      !base::FeatureList::IsEnabled(
          autofill::features::kYourSavedInfoSettingsPage)) {
    return;
  }

  HatsService* hats_service =
      HatsServiceFactory::GetForProfile(profile_, /*create_if_necessary=*/true);
  // The HaTS service may not be available for the profile, for example if it
  // is a guest profile.
  if (!hats_service) {
    return;
  }

  const std::string trigger = GetManagementSurveyTrigger(survey);
  const SurveyBitsData product_specific_bits_data = {
      {"Visit from Your saved info", is_from_home_page},
  };
  hats_service->LaunchDelayedSurvey(trigger, 10000, product_specific_bits_data,
                                    /*product_specific_string_data=*/{});
}

}  // namespace settings
