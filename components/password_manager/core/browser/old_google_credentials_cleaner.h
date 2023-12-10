// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_OLD_GOOGLE_CREDENTIALS_CLEANER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_OLD_GOOGLE_CREDENTIALS_CLEANER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "components/password_manager/core/browser/credentials_cleaner.h"
#include "components/password_manager/core/browser/password_store/password_store_consumer.h"

class PrefService;

namespace password_manager {

class PasswordStoreInterface;

// This class is responsible for clearing all stored www.google.com passwords
// created before before 2012.
class OldGoogleCredentialCleaner : public PasswordStoreConsumer,
                                   public CredentialsCleaner {
 public:
  // The cleaning will be made for credentials from |store|.
  // A preference from |prefs| is used to set the last time (in seconds) when
  // the cleaning was performed.
  OldGoogleCredentialCleaner(scoped_refptr<PasswordStoreInterface> store,
                             PrefService* prefs);
  ~OldGoogleCredentialCleaner() override;

  OldGoogleCredentialCleaner(const OldGoogleCredentialCleaner&) = delete;
  OldGoogleCredentialCleaner& operator=(const OldGoogleCredentialCleaner&) =
      delete;

  // CredentialsCleaner:
  bool NeedsCleaning() override;
  void StartCleaning(Observer* observer) override;

 private:
  // PasswordStoreConsumer:
  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<PasswordForm>> results) override;

  // Clean-up is performed on |store_|.
  scoped_refptr<PasswordStoreInterface> store_;

  // |prefs_| is not an owning pointer. It is used to record he last time (in
  // seconds) when the cleaning was performed.
  raw_ptr<PrefService> prefs_;

  // Used to signal completion of the clean-up. It is null until
  // StartCleaning is called.
  raw_ptr<Observer> observer_ = nullptr;

  base::WeakPtrFactory<OldGoogleCredentialCleaner> weak_ptr_factory_{this};
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_OLD_GOOGLE_CREDENTIALS_CLEANER_H_
