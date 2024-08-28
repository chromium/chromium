// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_OS_CRYPT_ASYNC_MIGRATOR_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_OS_CRYPT_ASYNC_MIGRATOR_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "components/password_manager/core/browser/credentials_cleaner.h"
#include "components/password_manager/core/browser/password_store/password_store_consumer.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"

class PrefService;

namespace password_manager {

// This class is responsible for re-encrypting all passwords using OSCryptAsync
// API.
// https://source.chromium.org/chromium/chromium/src/+/main:components/os_crypt/async/README.md
class OSCryptAsyncMigrator : public PasswordStoreConsumer,
                             public CredentialsCleaner {
 public:
  OSCryptAsyncMigrator(scoped_refptr<PasswordStoreInterface> store,
                       IsAccountStore is_account_store,
                       PrefService* prefs);
  ~OSCryptAsyncMigrator() override;

  OSCryptAsyncMigrator(const OSCryptAsyncMigrator&) = delete;
  OSCryptAsyncMigrator& operator=(const OSCryptAsyncMigrator&) = delete;

  // CredentialsCleaner:
  bool NeedsCleaning() override;
  void StartCleaning(Observer* observer) override;

 private:
  // PasswordStoreConsumer:
  void OnGetPasswordStoreResultsOrErrorFrom(
      PasswordStoreInterface* store,
      LoginsResultOrError results_or_error) override;

  void MarkMigrationComplete();

  scoped_refptr<PasswordStoreInterface> store_;
  std::string_view store_pref_name_;

  // |prefs_| is not an owning pointer. It is used to record he last time (in
  // seconds) when the cleaning was performed.
  raw_ptr<PrefService> prefs_;

  // Used to signal completion of the clean-up. It is null until
  // StartCleaning is called.
  raw_ptr<Observer> observer_ = nullptr;

  base::WeakPtrFactory<OSCryptAsyncMigrator> weak_ptr_factory_{this};
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_OS_CRYPT_ASYNC_MIGRATOR_H_
