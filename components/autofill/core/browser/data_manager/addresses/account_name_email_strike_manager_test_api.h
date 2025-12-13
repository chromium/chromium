// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_ADDRESSES_ACCOUNT_NAME_EMAIL_STRIKE_MANAGER_TEST_API_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_ADDRESSES_ACCOUNT_NAME_EMAIL_STRIKE_MANAGER_TEST_API_H_

#include "components/autofill/core/browser/data_manager/addresses/account_name_email_strike_manager.h"

namespace autofill {

class AccountNameEmailStrikeManagerTestApi {
 public:
  explicit AccountNameEmailStrikeManagerTestApi(
      AccountNameEmailStrikeManager* strike_manager)
      : strike_manager_(strike_manager) {}

  bool was_name_email_profile_suggestion_shown() const {
    return strike_manager_->was_name_email_profile_suggestion_shown_;
  }

  bool was_name_email_profile_filled() const {
    return strike_manager_->was_name_email_profile_filled_;
  }

 private:
  const raw_ptr<AccountNameEmailStrikeManager> strike_manager_;
};

AccountNameEmailStrikeManagerTestApi test_api(
    AccountNameEmailStrikeManager* store) {
  return AccountNameEmailStrikeManagerTestApi{store};
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_ADDRESSES_ACCOUNT_NAME_EMAIL_STRIKE_MANAGER_TEST_API_H_
