// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/mock_account_checker.h"

#include "components/commerce/core/pref_names.h"
#include "components/optimization_guide/core/feature_registry/feature_registration.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"

namespace commerce {

MockAccountChecker::MockAccountChecker()
    : AccountChecker("", "", nullptr, nullptr, nullptr, nullptr) {
  // Default to an account checker with the fewest restrictions.
  SetSignedIn(true);
  SetAllSyncTypesEnabled(true);
  SetAnonymizedUrlDataCollectionEnabled(true);
  SetIsSubjectToParentalControls(false);
  SetCanUseModelExecutionFeatures(true);
  SetSyncAvailable(true);
  // Default pref service can be overwritten by SetPrefs below.
  default_pref_service_ = std::make_unique<TestingPrefServiceSimple>();
  RegisterCommercePrefs(default_pref_service_->registry());
  SetPrefs(default_pref_service_.get());
}

MockAccountChecker::~MockAccountChecker() = default;

void MockAccountChecker::SetSignedIn(bool signed_in) {
  ON_CALL(*this, IsSignedIn).WillByDefault(testing::Return(signed_in));
}

void MockAccountChecker::SetAllSyncTypesEnabled(bool enabled) {
  ON_CALL(*this, IsSyncTypeEnabled).WillByDefault(testing::Return(enabled));
}

void MockAccountChecker::SetSyncAvailable(bool available) {
  ON_CALL(*this, IsSyncAvailable).WillByDefault(testing::Return(available));
}

void MockAccountChecker::SetAnonymizedUrlDataCollectionEnabled(bool enabled) {
  ON_CALL(*this, IsAnonymizedUrlDataCollectionEnabled)
      .WillByDefault(testing::Return(enabled));
}

void MockAccountChecker::SetIsSubjectToParentalControls(
    bool subject_to_parental_controls) {
  ON_CALL(*this, IsSubjectToParentalControls)
      .WillByDefault(testing::Return(subject_to_parental_controls));
}

void MockAccountChecker::SetCanUseModelExecutionFeatures(
    bool can_use_model_execution_features) {
  ON_CALL(*this, CanUseModelExecutionFeatures)
      .WillByDefault(testing::Return(can_use_model_execution_features));
}

void MockAccountChecker::SetCountry(std::string country) {
  ON_CALL(*this, GetCountry).WillByDefault(testing::Return(country));
}

void MockAccountChecker::SetLocale(std::string locale) {
  ON_CALL(*this, GetLocale).WillByDefault(testing::Return(locale));
}

void MockAccountChecker::SetPrefs(PrefService* prefs) {
  ON_CALL(*this, GetPrefs).WillByDefault(testing::Return(prefs));
}

void MockAccountChecker::RegisterCommercePrefs(PrefRegistrySimple* registry) {
  RegisterPrefs(registry);

  registry->RegisterIntegerPref(
      optimization_guide::prefs::kProductSpecificationsEnterprisePolicyAllowed,
      0);
}

}  // namespace commerce
