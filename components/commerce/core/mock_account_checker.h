// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_MOCK_ACCOUNT_CHECKER_H_
#define COMPONENTS_COMMERCE_CORE_MOCK_ACCOUNT_CHECKER_H_

#include "components/commerce/core/account_checker.h"

namespace commerce {

// Used to mock user account status in tests.
class MockAccountChecker : public AccountChecker {
 public:
  // Construct an AccountChecker with the fewest restrictions by default.
  MockAccountChecker();
  MockAccountChecker(const MockAccountChecker&) = delete;
  ~MockAccountChecker() override;

  bool IsSignedIn() override;

  bool IsAnonymizedUrlDataCollectionEnabled() override;

  bool IsWebAndAppActivityEnabled() override;

  bool IsSubjectToParentalControls() override;

  void SetSignedIn(bool signed_in);

  void SetAnonymizedUrlDataCollectionEnabled(bool enabled);

  void SetWebAndAppActivityEnabled(bool enabled);

  void SetIsSubjectToParentalControls(bool subject_to_parental_controls);

 private:
  bool signed_in_{true};
  bool anonymized_url_data_collection_enabled_{true};
  bool web_and_app_activity_enabled_{true};
  bool is_subject_to_parental_controls_{false};
};

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_MOCK_ACCOUNT_CHECKER_H_
