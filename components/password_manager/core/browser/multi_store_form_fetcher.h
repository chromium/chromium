// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MULTI_STORE_FORM_FETCHER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MULTI_STORE_FORM_FETCHER_H_

#include "base/macros.h"
#include "components/password_manager/core/browser/form_fetcher_impl.h"

namespace password_manager {

// Production implementation of FormFetcher that fetches credentials associated
// with a particular origin from both the account and profile password stores.
// When adding new member fields to this class, please, update the Clone()
// method accordingly.
class MultiStoreFormFetcher : public FormFetcherImpl {
 public:
  MultiStoreFormFetcher(PasswordStore::FormDigest form_digest,
                        const PasswordManagerClient* client,
                        bool should_migrate_http_passwords);
  ~MultiStoreFormFetcher() override;

  void Fetch() override;
  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<autofill::PasswordForm>> results) override;

 private:
  int wait_counter_ = 0;
  std::vector<std::unique_ptr<autofill::PasswordForm>> partial_results_;

  DISALLOW_COPY_AND_ASSIGN(MultiStoreFormFetcher);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MULTI_STORE_FORM_FETCHER_H_
