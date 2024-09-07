// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/mock_account_checker.h"
#include "components/prefs/pref_service.h"

namespace commerce {

MockAccountChecker::MockAccountChecker()
    : AccountChecker("", "", nullptr, nullptr, nullptr, nullptr) {
  // Default to an account checker with the fewest restrictions.
  SetSignedIn(true);
  SetSyncingBookmarks(true);
  SetAnonymizedUrlDataCollectionEnabled(true);
  SetIsSubjectToParentalControls(false);
  SetCanUseModelExecutionFeatures(true);
  SetCountry("us");
  SetLocale("en-us");
}

MockAccountChecker::~MockAccountChecker() = default;

void MockAccountChecker::SetSignedIn(bool signed_in) {
  ON_CALL(*this, IsSignedIn).WillByDefault(testing::Return(signed_in));
}

void MockAccountChecker::SetSyncingBookmarks(bool syncing) {
  ON_CALL(*this, IsSyncingBookmarks).WillByDefault(testing::Return(syncing));
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

}  // namespace commerce
