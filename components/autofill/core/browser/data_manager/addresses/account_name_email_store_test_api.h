// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_ADDRESSES_ACCOUNT_NAME_EMAIL_STORE_TEST_API_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_ADDRESSES_ACCOUNT_NAME_EMAIL_STORE_TEST_API_H_

#include "components/autofill/core/browser/data_manager/addresses/account_name_email_store.h"
#include "components/signin/public/identity_manager/account_info.h"

namespace autofill {

class AccountNameEmailStoreTestApi {
 public:
  explicit AccountNameEmailStoreTestApi(AccountNameEmailStore* store)
      : store_(store) {}

  std::string HashAccountInfo(const AccountInfo& info) const {
    return store_->HashAccountInfo(info);
  }

 private:
  const raw_ptr<AccountNameEmailStore> store_;
};

AccountNameEmailStoreTestApi test_api(AccountNameEmailStore* store) {
  return AccountNameEmailStoreTestApi{store};
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_ADDRESSES_ACCOUNT_NAME_EMAIL_STORE_TEST_API_H_
