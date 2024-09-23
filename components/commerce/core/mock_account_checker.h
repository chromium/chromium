// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_MOCK_ACCOUNT_CHECKER_H_
#define COMPONENTS_COMMERCE_CORE_MOCK_ACCOUNT_CHECKER_H_

#include <string>

#include "components/commerce/core/account_checker.h"
#include "components/sync/base/data_type.h"
#include "testing/gmock/include/gmock/gmock.h"

class PrefService;

namespace commerce {

// Used to mock user account status in tests.
class MockAccountChecker : public AccountChecker {
 public:
  // Construct an AccountChecker with the fewest restrictions by default.
  MockAccountChecker();
  MockAccountChecker(const MockAccountChecker&) = delete;
  ~MockAccountChecker() override;

  MOCK_METHOD(bool, IsSignedIn, (), (override));

  MOCK_METHOD(bool, IsSyncingBookmarks, (), (override));

  MOCK_METHOD(bool,
              IsSyncTypeEnabled,
              (syncer::UserSelectableType type),
              (override));

  MOCK_METHOD(bool, IsAnonymizedUrlDataCollectionEnabled, (), (override));

  MOCK_METHOD(bool, IsSubjectToParentalControls, (), (override));

  MOCK_METHOD(bool, CanUseModelExecutionFeatures, (), (override));

  MOCK_METHOD(std::string, GetCountry, (), (override));

  MOCK_METHOD(std::string, GetLocale, (), (override));

  MOCK_METHOD(PrefService*, GetPrefs, (), (override));

  void SetSignedIn(bool signed_in);

  void SetSyncingBookmarks(bool syncing);

  void SetAnonymizedUrlDataCollectionEnabled(bool enabled);

  void SetIsSubjectToParentalControls(bool subject_to_parental_controls);

  void SetCanUseModelExecutionFeatures(bool can_use_model_execution_features);

  void SetCountry(std::string country);

  void SetLocale(std::string locale);

  void SetPrefs(PrefService* prefs);
};

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_MOCK_ACCOUNT_CHECKER_H_
