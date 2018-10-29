// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_HASH_PASSWORD_MANAGER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_HASH_PASSWORD_MANAGER_H_

#include "base/macros.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "components/password_manager/core/browser/password_hash_data.h"

class PrefService;

namespace password_manager {

// Responsible for saving, clearing, retrieving and encryption of a password
// hash data in preferences.
// All methods should be called on UI thread.
class HashPasswordManager {
 public:
  HashPasswordManager() = default;
  explicit HashPasswordManager(PrefService* prefs);
  ~HashPasswordManager() = default;

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

 private:
  // Saves encrypted string |s| in a preference |pref_name|. Returns true on
  // success.
  bool EncryptAndSaveToPrefs(const std::string& pref_name,
                             const std::string& s);

  // Encrypts and saves |password_hash_data| to prefs. Returns true on success.
  bool EncryptAndSave(const PasswordHashData& password_hash_data);

  // Retrieves and decrypts string value from a preference |pref_name|. Returns
  // an empty string on failure.
  std::string RetrievedDecryptedStringFromPrefs(const std::string& pref_name);

  PrefService* prefs_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(HashPasswordManager);
};

}  // namespace password_manager
#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_HASH_PASSWORD_MANAGER_H_
