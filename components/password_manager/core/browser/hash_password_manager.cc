// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/hash_password_manager.h"

#include <vector>

#include "base/base64.h"
#include "base/strings/string_number_conversions.h"
#include "components/os_crypt/sync/os_crypt.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace {

constexpr char kSeparator = '.';
constexpr char kHashFieldKey[] = "hash";
constexpr char kLastSignInTimeFieldKey[] = "last_signin";
constexpr char kLengthAndSaltFieldKey[] = "salt_length";
constexpr char kUsernameFieldKey[] = "username";
constexpr char kIsGaiaFieldKey[] = "is_gaia";

// The maximum number of password hash data we store in prefs.
constexpr size_t kMaxPasswordHashDataDictSize = 5;

}  // namespace

namespace password_manager {

namespace {

// Returns empty string if decryption fails.
std::string DecryptBase64String(const std::string& encrypted_base64_string) {
  if (encrypted_base64_string.empty()) {
    return std::string();
  }

  std::string encrypted_string;
  if (!base::Base64Decode(encrypted_base64_string, &encrypted_string)) {
    return std::string();
  }

  std::string plain_text;
  if (!OSCrypt::DecryptString(encrypted_string, &plain_text)) {
    return std::string();
  }

  return plain_text;
}

// Returns empty string if encryption fails.
std::string EncryptString(const std::string& plain_text) {
  std::string encrypted_text;
  if (!OSCrypt::EncryptString(plain_text, &encrypted_text)) {
    return std::string();
  }
  return base::Base64Encode(encrypted_text);
}

std::string GetAndDecryptField(const base::Value& dict,
                               const std::string& field_key) {
  const std::string* encrypted_field_value =
      dict.GetDict().FindString(field_key);
  return encrypted_field_value ? DecryptBase64String(*encrypted_field_value)
                               : std::string();
}

bool IsGaiaPassword(const base::Value& dict) {
  return GetAndDecryptField(dict, kIsGaiaFieldKey) == "true";
}

// Packs |salt| and |password_length| to a string.
std::string LengthAndSaltToString(const std::string& salt,
                                  size_t password_length) {
  return base::NumberToString(password_length) + kSeparator + salt;
}

// Unpacks |salt| and |password_length| from a string |s|.
// Returns true on success.
bool StringToLengthAndSalt(const std::string& s,
                           size_t* password_length,
                           std::string* salt) {
  if (s.empty() || !salt) {
    return false;
  }

  size_t separator_index = s.find(kSeparator);
  if (separator_index == std::string::npos) {
    return false;
  }

  std::string prefix = s.substr(0, separator_index);
  *salt = s.substr(separator_index + 1);
  return !salt->empty() && base::StringToSizeT(prefix, password_length);
}

std::string BooleanToString(bool bool_value) {
  return bool_value ? "true" : "false";
}
}  // namespace

std::optional<PasswordHashData> ConvertToPasswordHashData(
    const base::Value& dict) {
  PasswordHashData result;
  result.username = GetAndDecryptField(dict, kUsernameFieldKey);
  if (result.username.empty()) {
    return std::nullopt;
  }

  if (!base::StringToUint64(GetAndDecryptField(dict, kHashFieldKey),
                            &result.hash)) {
    return std::nullopt;
  }

  if (!StringToLengthAndSalt(GetAndDecryptField(dict, kLengthAndSaltFieldKey),
                             &result.length, &result.salt)) {
    return std::nullopt;
  }

  result.is_gaia_password = GetAndDecryptField(dict, kIsGaiaFieldKey) == "true";

  return result;
}

// TODO(b/325053878): Refactor class after safe_browsing_ui.* migration to
// the //chrome directory.
HashPasswordManager::HashPasswordManager(PrefService* prefs) : prefs_(prefs) {}
HashPasswordManager::HashPasswordManager() = default;
HashPasswordManager::~HashPasswordManager() = default;

bool HashPasswordManager::SavePasswordHash(const std::string& username,
                                           const std::u16string& password,
                                           bool is_gaia_password) {
  CheckPrefs(is_gaia_password);
  // TODO(b/324872193): Modify ScopedListPrefUpdate so unique_ptr isn't
  // necessary in this class.
  std::unique_ptr<ScopedListPrefUpdate> update =
      GetScopedListPrefUpdate(is_gaia_password);
  // If we've already saved password hash for |username|, and the |password| is
  // unchanged, no need to save password hash again. Instead we update the last
  // sign in timestamp.
  for (base::Value& password_hash_data : update->Get()) {
    if (AreUsernamesSame(
            GetAndDecryptField(password_hash_data, kUsernameFieldKey),
            IsGaiaPassword(password_hash_data), username, is_gaia_password)) {
      std::optional<PasswordHashData> existing_password_hash =
          ConvertToPasswordHashData(password_hash_data);
      if (existing_password_hash && existing_password_hash->MatchesPassword(
                                        username, password, is_gaia_password)) {
        password_hash_data.GetDict().Set(
            kLastSignInTimeFieldKey,
            base::Time::Now().InSecondsFSinceUnixEpoch());
        return true;
      }
    }
  }
  // A password hash does not exist when it is first sign-in.
  bool is_first_sign_in = !HasPasswordHash(username, is_gaia_password);
  bool is_saved = SavePasswordHash(
      PasswordHashData(username, password, true, is_gaia_password));
  // Currently, the only callback in this list is
  // CheckGaiaPasswordChangeForAllSignedInUsers which is in
  // ChromePasswordProtectionService. We only want to notify ChromePPS only when
  // a user has changed their password. This means that an existing password
  // hash has to already exist in the password store and the SavePasswordHash
  // has to succeed.
  if (!is_first_sign_in && is_saved) {
    state_callback_list_.Notify(username);
  }
  return is_saved;
}

bool HashPasswordManager::SavePasswordHash(
    const PasswordHashData& password_hash_data) {
  bool should_save = password_hash_data.force_update ||
                     !HasPasswordHash(password_hash_data.username,
                                      password_hash_data.is_gaia_password);
  return should_save ? EncryptAndSave(password_hash_data) : false;
}

void HashPasswordManager::ClearSavedPasswordHash(const std::string& username,
                                                 bool is_gaia_password) {
  CheckPrefs(is_gaia_password);

  std::unique_ptr<ScopedListPrefUpdate> update =
      GetScopedListPrefUpdate(is_gaia_password);

  (*update)->EraseIf([&](const auto& dict) {
    return AreUsernamesSame(GetAndDecryptField(dict, kUsernameFieldKey),
                            IsGaiaPassword(dict), username, is_gaia_password);
  });
}

void HashPasswordManager::ClearAllPasswordHash(bool is_gaia_password) {
  CheckPrefs(is_gaia_password);
  std::unique_ptr<ScopedListPrefUpdate> update =
      GetScopedListPrefUpdate(is_gaia_password);

  (*update)->EraseIf([&](const auto& dict) {
    return GetAndDecryptField(dict, kIsGaiaFieldKey) ==
           BooleanToString(is_gaia_password);
  });
}

void HashPasswordManager::ClearAllNonGmailPasswordHash() {
  CHECK(prefs_);

  ScopedListPrefUpdate update(prefs_, prefs::kPasswordHashDataList);
  update->EraseIf([](const base::Value& data) {
    if (GetAndDecryptField(data, kIsGaiaFieldKey) == "false") {
      return false;
    }
    std::string username = GetAndDecryptField(data, kUsernameFieldKey);
    std::string email =
        CanonicalizeUsername(username, /*is_gaia_account=*/true);
    return email.find("@gmail.com") == std::string::npos;
  });
  ClearAllPasswordHash(/*is_gaia_password=*/false);
}

std::vector<PasswordHashData> HashPasswordManager::RetrieveAllPasswordHashes() {
  CHECK(prefs_);
  std::vector<PasswordHashData> result;
  if (base::FeatureList::IsEnabled(
          features::kLocalStateEnterprisePasswordHashes)) {
    // TODO(b/325053878): Replace w/ CHECK once safe.
    if (!local_prefs_) {
      return result;
    }
  }
  // Required check to avoid returning an empty pref value.
  if (prefs_->HasPrefPath(prefs::kPasswordHashDataList)) {
    result = RetrieveAllPasswordHashesInternal(
        prefs_->GetList(prefs::kPasswordHashDataList));
  }
  if (base::FeatureList::IsEnabled(
          features::kLocalStateEnterprisePasswordHashes)) {
    // Required check to avoid returning an empty pref value.
    if (local_prefs_->HasPrefPath(prefs::kLocalPasswordHashDataList)) {
      std::vector<PasswordHashData> enterprise_result =
          RetrieveAllPasswordHashesInternal(
              local_prefs_->GetList(prefs::kLocalPasswordHashDataList));
      result.insert(result.end(), enterprise_result.begin(),
                    enterprise_result.end());
    }
  }

  return result;
}

std::vector<PasswordHashData>
HashPasswordManager::RetrieveAllPasswordHashesInternal(
    const base::Value::List& hash_list) const {
  std::vector<PasswordHashData> result;
  for (const base::Value& entry : hash_list) {
    std::optional<PasswordHashData> password_hash_data =
        ConvertToPasswordHashData(entry);
    if (password_hash_data) {
      result.push_back(std::move(*password_hash_data));
    }
  }
  return result;
}

std::optional<PasswordHashData> HashPasswordManager::RetrievePasswordHash(
    const std::string& username,
    bool is_gaia_password) {
  CHECK(prefs_);
  if (username.empty()) {
    return std::nullopt;
  }
  const base::Value::List* hash_list = GetPrefList(is_gaia_password);
  if (!hash_list) {
    return std::nullopt;
  }
  for (const base::Value& entry : *hash_list) {
    if (AreUsernamesSame(GetAndDecryptField(entry, kUsernameFieldKey),
                         IsGaiaPassword(entry), username, is_gaia_password)) {
      return ConvertToPasswordHashData(entry);
    }
  }

  return std::nullopt;
}

bool HashPasswordManager::HasPasswordHash(const std::string& username,
                                          bool is_gaia_password) {
  CheckPrefs(is_gaia_password);
  if (username.empty()) {
    return false;
  }
  const base::Value::List* hash_list = GetPrefList(is_gaia_password);
  if (!hash_list) {
    return false;
  }
  for (const base::Value& entry : *hash_list) {
    if (AreUsernamesSame(GetAndDecryptField(entry, kUsernameFieldKey),
                         IsGaiaPassword(entry), username, is_gaia_password)) {
      return true;
    }
  }

  return false;
}

base::CallbackListSubscription HashPasswordManager::RegisterStateCallback(
    const base::RepeatingCallback<void(const std::string& username)>&
        callback) {
  return state_callback_list_.Add(callback);
}

bool HashPasswordManager::EncryptAndSave(
    const PasswordHashData& password_hash_data) {
  CheckPrefs(password_hash_data.is_gaia_password);
  if (password_hash_data.username.empty()) {
    return false;
  }

  std::string encrypted_username = EncryptString(CanonicalizeUsername(
      password_hash_data.username, password_hash_data.is_gaia_password));
  if (encrypted_username.empty()) {
    return false;
  }

  std::string encrypted_hash =
      EncryptString(base::NumberToString(password_hash_data.hash));
  if (encrypted_hash.empty()) {
    return false;
  }

  std::string encrypted_length_and_salt = EncryptString(LengthAndSaltToString(
      password_hash_data.salt, password_hash_data.length));
  if (encrypted_length_and_salt.empty()) {
    return false;
  }

  std::string encrypted_is_gaia_value =
      EncryptString(BooleanToString(password_hash_data.is_gaia_password));
  if (encrypted_is_gaia_value.empty()) {
    return false;
  }

  base::Value::Dict encrypted_password_hash_entry;
  encrypted_password_hash_entry.Set(kUsernameFieldKey, encrypted_username);
  encrypted_password_hash_entry.Set(kHashFieldKey, encrypted_hash);
  encrypted_password_hash_entry.Set(kLengthAndSaltFieldKey,
                                    encrypted_length_and_salt);
  encrypted_password_hash_entry.Set(kIsGaiaFieldKey, encrypted_is_gaia_value);
  encrypted_password_hash_entry.Set(
      kLastSignInTimeFieldKey, base::Time::Now().InSecondsFSinceUnixEpoch());
  std::unique_ptr<ScopedListPrefUpdate> update =
      GetScopedListPrefUpdate(password_hash_data.is_gaia_password);

  base::Value::List& update_list = update->Get();
  size_t num_erased = update_list.EraseIf([&](const auto& dict) {
    return AreUsernamesSame(GetAndDecryptField(dict, kUsernameFieldKey),
                            IsGaiaPassword(dict), password_hash_data.username,
                            password_hash_data.is_gaia_password);
  });

  if (num_erased == 0 && update_list.size() >= kMaxPasswordHashDataDictSize) {
    // Erase the oldest sign-in password hash data.
    update_list.erase(std::min_element(
        update_list.begin(), update_list.end(),
        [](const auto& lhs, const auto& rhs) {
          return *lhs.GetDict().FindDouble(kLastSignInTimeFieldKey) <
                 *rhs.GetDict().FindDouble(kLastSignInTimeFieldKey);
        }));
  }

  update_list.Append(std::move(encrypted_password_hash_entry));
  return true;
}

const base::Value::List* HashPasswordManager::GetPrefList(
    bool is_gaia_password) const {
  if (!is_gaia_password) {
    if (base::FeatureList::IsEnabled(
            features::kLocalStateEnterprisePasswordHashes)) {
      // Required check to avoid returning an empty pref value.
      if (!local_prefs_->HasPrefPath(prefs::kLocalPasswordHashDataList)) {
        return nullptr;
      }
      return &local_prefs_->GetList(prefs::kLocalPasswordHashDataList);
    }
  }
  // Required check to avoid returning an empty pref value.
  if (!prefs_->HasPrefPath(prefs::kPasswordHashDataList)) {
    return nullptr;
  }
  return &prefs_->GetList(prefs::kPasswordHashDataList);
}

std::unique_ptr<ScopedListPrefUpdate>
HashPasswordManager::GetScopedListPrefUpdate(bool is_gaia_password) const {
  if (!is_gaia_password) {
    if (base::FeatureList::IsEnabled(
            features::kLocalStateEnterprisePasswordHashes)) {
      return std::make_unique<ScopedListPrefUpdate>(
          local_prefs_, prefs::kLocalPasswordHashDataList);
    }
  }
  return std::make_unique<ScopedListPrefUpdate>(prefs_,
                                                prefs::kPasswordHashDataList);
}

void HashPasswordManager::CheckPrefs(bool is_gaia_password) const {
  CHECK(prefs_);
  if (!is_gaia_password) {
    if (base::FeatureList::IsEnabled(
            features::kLocalStateEnterprisePasswordHashes)) {
      CHECK(local_prefs_);
    }
  }
}

void HashPasswordManager::MigrateEnterprisePasswordHashes() {
  CHECK(prefs_);
  // TODO(b/325053878): Replace w/ CHECK once safe.
  // Ensure pref has been registered before proceeding.
  if (!local_prefs_ ||
      !local_prefs_->FindPreference(prefs::kLocalPasswordHashDataList)) {
    return;
  }
  ScopedListPrefUpdate update(prefs_, prefs::kPasswordHashDataList);
  ScopedListPrefUpdate enterprise_update(local_prefs_,
                                         prefs::kLocalPasswordHashDataList);
  base::Value::List& update_list = update.Get();
  base::Value::List& enterprise_update_list = enterprise_update.Get();
  for (auto it = update_list.begin(); it != update_list.end();) {
    if (!IsGaiaPassword(*it) &&
        base::FeatureList::IsEnabled(
            features::kLocalStateEnterprisePasswordHashes)) {
      enterprise_update_list.Append(std::move(it->GetDict()));
      it = update_list.erase(it);
      continue;
    }
    ++it;
  }
}

}  // namespace password_manager
