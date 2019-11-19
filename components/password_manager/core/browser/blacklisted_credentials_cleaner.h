// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_BLACKLISTED_CREDENTIALS_CLEANER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_BLACKLISTED_CREDENTIALS_CLEANER_H_

#include <memory>
#include <vector>

#include "base/memory/ref_counted.h"
#include "components/password_manager/core/browser/credentials_cleaner.h"
#include "components/password_manager/core/browser/password_store_consumer.h"

class PrefService;

namespace autofill {
struct PasswordForm;
}

namespace password_manager {

class PasswordStore;

// This class is responsible for cleaning up blacklisted credentials. In
// particular, it does the following operations:
// - De-duplicate existing blacklisted credentials by making sure there is at
//   most one blacklisted credential per signon realm.
// - Normalize existing blacklisted credentials by clearing out all data except
//   scheme, signon realm and origin.
class BlacklistedCredentialsCleaner : public CredentialsCleaner,
                                      public PasswordStoreConsumer {
 public:
  BlacklistedCredentialsCleaner(scoped_refptr<PasswordStore> store,
                                PrefService* prefs);
  BlacklistedCredentialsCleaner(const BlacklistedCredentialsCleaner&) = delete;
  BlacklistedCredentialsCleaner& operator=(
      const BlacklistedCredentialsCleaner&) = delete;
  ~BlacklistedCredentialsCleaner() override;

  // CredentialsCleaner:
  bool NeedsCleaning() override;
  void StartCleaning(Observer* observer) override;

 private:
  // PasswordStoreConsumer:
  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<autofill::PasswordForm>> results) override;

  const scoped_refptr<PasswordStore> store_;

  PrefService* prefs_ = nullptr;

  Observer* observer_ = nullptr;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_BLACKLISTED_CREDENTIALS_CLEANER_H_
