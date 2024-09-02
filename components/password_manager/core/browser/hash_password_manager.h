// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_HASH_PASSWORD_MANAGER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_HASH_PASSWORD_MANAGER_H_

#include <optional>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "components/password_manager/core/browser/password_hash_data.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/scoped_user_pref_update.h"

class PrefService;

namespace password_manager {

// Helper function to convert a dictionary value to PasswordWordHashData.
std::optional<PasswordHashData> ConvertToPasswordHashData(
    const base::Value& dict);

// Responsible for saving, clearing, retrieving and encryption of a password
// hash data in preferences.
// All methods should be called on UI thread.
class HashPasswordManager {
 public:
  HashPasswordManager();
  explicit HashPasswordManager(PrefService* prefs);

  HashPasswordManager(const HashPasswordManager&) = delete;
  HashPasswordManager& operator=(const HashPasswordManager&) = delete;

  ~HashPasswordManager();

  bool SavePasswordHash(const std::string& username,
                        const std::u16string& password,
                        bool is_gaia_password = true);
  bool SavePasswordHash(const PasswordHashData& password_hash_data);
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
  std::optional<PasswordHashData> RetrievePasswordHash(
      const std::string& username,
      bool is_gaia_password);

  // Whether password hash of |username| and |is_gaia_password| is stored.
  bool HasPasswordHash(const std::string& username, bool is_gaia_password);

  // Moves enterpise password hashes from the profile storage to the local
  // state storage.
  void MigrateEnterprisePasswordHashes();

  void set_prefs(PrefService* prefs) { prefs_ = prefs; }

  void set_local_prefs(PrefService* local_prefs) { local_prefs_ = local_prefs; }

  // Adds a listener for when |kPasswordHashDataList| list might have changed.
  // Should only be called on the UI thread. The callback is only called when
  // the sign-in isn't the first change on the |kPasswordHashDataList| and
  // saving the password hash actually succeeded.
  virtual base::CallbackListSubscription RegisterStateCallback(
      const base::RepeatingCallback<void(const std::string& username)>&
          callback);

 private:
  // Encrypts and saves |password_hash_data| to prefs. Returns true on success.
  bool EncryptAndSave(const PasswordHashData& password_hash_data);

  // Retrieves all saved password hashes from |hash_list| as a
  // PasswordHashData collection.
  std::vector<PasswordHashData> RetrieveAllPasswordHashesInternal(
      const base::Value::List& hash_list) const;

  const base::Value::List* GetPrefList(bool is_gaia_password) const;
  std::unique_ptr<ScopedListPrefUpdate> GetScopedListPrefUpdate(
      bool is_gaia_password) const;

  // CHECKs PrefServices.
  void CheckPrefs(bool is_gaia_password) const;

  raw_ptr<PrefService> prefs_ = nullptr;

  raw_ptr<PrefService> local_prefs_ = nullptr;

  // Callbacks when |kPasswordHashDataList| might have changed.
  // Should only be accessed on the UI thread. The callback is only called when
  // the sign-in isn't the first change on the |kPasswordHashDataList| and
  // saving the password hash actually succeeded.
  base::RepeatingCallbackList<void(const std::string& username)>
      state_callback_list_;
};

}  // namespace password_manager
#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_HASH_PASSWORD_MANAGER_H_
