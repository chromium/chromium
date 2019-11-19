// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_HASH_PASSWORD_MANAGER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_HASH_PASSWORD_MANAGER_H_

#include "base/callback.h"
#include "base/callback_list.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "components/password_manager/core/browser/password_hash_data.h"

class PrefService;

using StateSubscription =
    base::CallbackList<void(const std::string& username)>::Subscription;

namespace password_manager {

// Responsible for saving, clearing, retrieving and encryption of a password
// hash data in preferences.
// All methods should be called on UI thread.
class HashPasswordManager {
 public:
  HashPasswordManager();
  explicit HashPasswordManager(PrefService* prefs);
  ~HashPasswordManager();

  bool SavePasswordHash(const std::string username,
                        const base::string16& password,
                        bool is_gaia_password = true);
  bool SavePasswordHash(const PasswordHashData& password_hash_data);
  void ClearSavedPasswordHash();
  void ClearSavedPasswordHash(const std::string& username,
                              bool is_gaia_password);
  // If |is_gaia_password| is true, clears all Gaia password hashes, otherwise
  // clears all enterprise password hashes.
  void ClearAllPasswordHash(bool is_gaia_password);

  // Clears all non-Gmail Gaia password hashes.
  void ClearAllNonGmailPasswordHash();

  // Returns empty array if no hash is available.
  std::vector<PasswordHashData> RetrieveAllPasswordHashes();

  // Returns empty if no hash matching |username| and |is_gaia_password| is
  // available.
  base::Optional<PasswordHashData> RetrievePasswordHash(
      const std::string& username,
      bool is_gaia_password);

  // Whether password hash of |username| and |is_gaia_password| is stored.
  bool HasPasswordHash(const std::string& username, bool is_gaia_password);

  void set_prefs(PrefService* prefs) { prefs_ = prefs; }

  // Adds a listener for when |kPasswordHashDataList| list might have changed.
  // Should only be called on the UI thread. The callback is only called when
  // the sign-in isn't the first change on the |kPasswordHashDataList| and
  // saving the password hash actually succeeded.
  virtual std::unique_ptr<StateSubscription> RegisterStateCallback(
      const base::Callback<void(const std::string& username)>& callback);

 private:
  // Encrypts and saves |password_hash_data| to prefs. Returns true on success.
  bool EncryptAndSave(const PasswordHashData& password_hash_data);

  PrefService* prefs_ = nullptr;

  // Callbacks when |kPasswordHashDataList| might have changed.
  // Should only be accessed on the UI thread. The callback is only called when
  // the sign-in isn't the first change on the |kPasswordHashDataList| and
  // saving the password hash actually succeeded.
  base::CallbackList<void(const std::string& username)> state_callback_list_;

  DISALLOW_COPY_AND_ASSIGN(HashPasswordManager);
};

}  // namespace password_manager
#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_HASH_PASSWORD_MANAGER_H_
