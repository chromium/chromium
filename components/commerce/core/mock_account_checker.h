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
  MockAccountChecker();
  MockAccountChecker(const MockAccountChecker&) = delete;
  ~MockAccountChecker() override;

  bool IsSignedIn() override;

  bool IsAnonymizedUrlDataCollectionEnabled() override;

  bool IsWebAndAppActivityEnabled() override;

  void SetSignedIn(bool signed_in);

  void SetAnonymizedUrlDataCollectionEnabled(bool enabled);

  void SetWebAndAppActivityEnabled(bool enabled);

 private:
  bool signed_in_;
  bool anonymized_url_data_collection_enabled_;
  bool web_and_app_activity_enabled_;
};

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_MOCK_ACCOUNT_CHECKER_H_
