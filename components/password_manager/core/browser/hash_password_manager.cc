// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/hash_password_manager.h"

#include <vector>

#include "base/base64.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "components/os_crypt/os_crypt.h"
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
  if (encrypted_base64_string.empty())
    return std::string();

  std::string encrypted_string;
  if (!base::Base64Decode(encrypted_base64_string, &encrypted_string))
    return std::string();

  std::string plain_text;
  if (!OSCrypt::DecryptString(encrypted_string, &plain_text))
    return std::string();

  return plain_text;
}

// Returns empty string if encryption fails.
std::string EncryptString(const std::string& plain_text) {
  std::string encrypted_text;
  if (!OSCrypt::EncryptString(plain_text, &encrypted_text)) {
    return std::string();
  }
  std::string encrypted_base64_text;
  base::Base64Encode(encrypted_text, &encrypted_base64_text);
  return encrypted_base64_text;
}

std::string GetAndDecryptField(const base::Value& dict,
                               const std::string& field_key) {
  const base::Value* encrypted_field_value = dict.FindKey(field_key);
  return encrypted_field_value
             ? DecryptBase64String(encrypted_field_value->GetString())
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
  if (s.empty() || !salt)
    return false;

  size_t separator_index = s.find(kSeparator);
  if (separator_index == std::string::npos)
    return false;

  std::string prefix = s.substr(0, separator_index);
  *salt = s.substr(separator_index + 1);
  return !salt->empty() && base::StringToSizeT(prefix, password_length);
}

std::string BooleanToString(bool bool_value) {
  return bool_value ? "true" : "false";
}

// Helper function to convert a dictionary value to PasswordWordHashData.
base::Optional<PasswordHashData> ConvertToPasswordHashData(
    const base::Value& dict) {
  PasswordHashData result;
  result.username = GetAndDecryptField(dict, kUsernameFieldKey);
  if (result.username.empty())
    return base::nullopt;

  if (!base::StringToUint64(GetAndDecryptField(dict, kHashFieldKey),
                            &result.hash)) {
    return base::nullopt;
  }

  if (!StringToLengthAndSalt(GetAndDecryptField(dict, kLengthAndSaltFieldKey),
                             &result.length, &result.salt)) {
    return base::nullopt;
  }

  result.is_gaia_password = GetAndDecryptField(dict, kIsGaiaFieldKey) == "true";

  return result;
}

}  // namespace

HashPasswordManager::HashPasswordManager(PrefService* prefs) : prefs_(prefs) {}
HashPasswordManager::HashPasswordManager() = default;
HashPasswordManager::~HashPasswordManager() = default;

bool HashPasswordManager::SavePasswordHash(const std::string username,
                                           const base::string16& password,
                                           bool is_gaia_password) {
  if (!prefs_)
    return false;

  // If we've already saved password hash for |username|, and the |password| is
  // unchanged, no need to save password hash again. Instead we update the last
  // sign in timestamp.
  ListPrefUpdate update(prefs_, prefs::kPasswordHashDataList);
  for (base::Value& password_hash_data : update.Get()->GetList()) {
    if (AreUsernamesSame(
            GetAndDecryptField(password_hash_data, kUsernameFieldKey),
            IsGaiaPassword(password_hash_data), username, is_gaia_password)) {
      base::Optional<PasswordHashData> existing_password_hash =
          ConvertToPasswordHashData(password_hash_data);
      if (existing_password_hash && existing_password_hash->MatchesPassword(
                                        username, password, is_gaia_password)) {
        password_hash_data.SetKey(kLastSignInTimeFieldKey,
                                  base::Value(base::Time::Now().ToDoubleT()));
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
  if (!is_first_sign_in && is_saved)
    state_callback_list_.Notify(username);
  return is_saved;
}

bool HashPasswordManager::SavePasswordHash(
    const PasswordHashData& password_hash_data) {
  bool should_save = password_hash_data.force_update ||
                     !HasPasswordHash(password_hash_data.username,
                                      password_hash_data.is_gaia_password);
  return should_save ? EncryptAndSave(password_hash_data) : false;
}

void HashPasswordManager::ClearSavedPasswordHash() {
  if (prefs_)
    prefs_->ClearPref(prefs::kSyncPasswordHash);
}

void HashPasswordManager::ClearSavedPasswordHash(const std::string& username,
                                                 bool is_gaia_password) {
  if (!prefs_)
    return;

  ListPrefUpdate update(prefs_, prefs::kPasswordHashDataList);
  update->EraseListValueIf([&](const auto& dict) {
    return AreUsernamesSame(GetAndDecryptField(dict, kUsernameFieldKey),
                            IsGaiaPassword(dict), username, is_gaia_password);
  });
}

void HashPasswordManager::ClearAllPasswordHash(bool is_gaia_password) {
  if (!prefs_)
    return;

  ListPrefUpdate update(prefs_, prefs::kPasswordHashDataList);
  update->EraseListValueIf([&](const auto& dict) {
    return GetAndDecryptField(dict, kIsGaiaFieldKey) ==
           BooleanToString(is_gaia_password);
  });
}

void HashPasswordManager::ClearAllNonGmailPasswordHash() {
  if (!prefs_)
    return;

  ListPrefUpdate update(prefs_, prefs::kPasswordHashDataList);
  update->EraseListValueIf([](const base::Value& data) {
    if (GetAndDecryptField(data, kIsGaiaFieldKey) == "false") {
      return false;
    }
    std::string username = GetAndDecryptField(data, kUsernameFieldKey);
    std::string email =
        CanonicalizeUsername(username, /*is_gaia_account=*/true);
    return email.find("@gmail.com") == std::string::npos;
  });
}

std::vector<PasswordHashData> HashPasswordManager::RetrieveAllPasswordHashes() {
  std::vector<PasswordHashData> result;
  if (!prefs_ || !prefs_->HasPrefPath(prefs::kPasswordHashDataList))
    return result;

  const base::ListValue* hash_list =
      prefs_->GetList(prefs::kPasswordHashDataList);

  for (const base::Value& entry : hash_list->GetList()) {
    base::Optional<PasswordHashData> password_hash_data =
        ConvertToPasswordHashData(entry);
    if (password_hash_data)
      result.push_back(std::move(*password_hash_data));
  }
  return result;
}

base::Optional<PasswordHashData> HashPasswordManager::RetrievePasswordHash(
    const std::string& username,
    bool is_gaia_password) {
  if (!prefs_ || username.empty() ||
      !prefs_->HasPrefPath(prefs::kPasswordHashDataList)) {
    return base::nullopt;
  }

  for (const base::Value& entry :
       prefs_->GetList(prefs::kPasswordHashDataList)->GetList()) {
    if (AreUsernamesSame(GetAndDecryptField(entry, kUsernameFieldKey),
                         IsGaiaPassword(entry), username, is_gaia_password)) {
      return ConvertToPasswordHashData(entry);
    }
  }

  return base::nullopt;
}

bool HashPasswordManager::HasPasswordHash(const std::string& username,
                                          bool is_gaia_password) {
  if (username.empty() || !prefs_ ||
      !prefs_->HasPrefPath(prefs::kPasswordHashDataList)) {
    return false;
  }

  for (const base::Value& entry :
       prefs_->GetList(prefs::kPasswordHashDataList)->GetList()) {
    if (AreUsernamesSame(GetAndDecryptField(entry, kUsernameFieldKey),
                         IsGaiaPassword(entry), username, is_gaia_password)) {
      return true;
    }
  }

  return false;
}

std::unique_ptr<StateSubscription> HashPasswordManager::RegisterStateCallback(
    const base::Callback<void(const std::string& username)>& callback) {
  return state_callback_list_.Add(callback);
}

bool HashPasswordManager::EncryptAndSave(
    const PasswordHashData& password_hash_data) {
  if (!prefs_ || password_hash_data.username.empty()) {
    return false;
  }

  std::string encrypted_username = EncryptString(CanonicalizeUsername(
      password_hash_data.username, password_hash_data.is_gaia_password));
  if (encrypted_username.empty())
    return false;

  std::string encrypted_hash =
      EncryptString(base::NumberToString(password_hash_data.hash));
  if (encrypted_hash.empty())
    return false;

  std::string encrypted_length_and_salt = EncryptString(LengthAndSaltToString(
      password_hash_data.salt, password_hash_data.length));
  if (encrypted_length_and_salt.empty())
    return false;

  std::string encrypted_is_gaia_value =
      EncryptString(BooleanToString(password_hash_data.is_gaia_password));
  if (encrypted_is_gaia_value.empty())
    return false;

  base::DictionaryValue encrypted_password_hash_entry;
  encrypted_password_hash_entry.SetKey(kUsernameFieldKey,
                                       base::Value(encrypted_username));
  encrypted_password_hash_entry.SetKey(kHashFieldKey,
                                       base::Value(encrypted_hash));
  encrypted_password_hash_entry.SetKey(kLengthAndSaltFieldKey,
                                       base::Value(encrypted_length_and_salt));
  encrypted_password_hash_entry.SetKey(kIsGaiaFieldKey,
                                       base::Value(encrypted_is_gaia_value));
  encrypted_password_hash_entry.SetKey(
      kLastSignInTimeFieldKey, base::Value(base::Time::Now().ToDoubleT()));
  ListPrefUpdate update(prefs_, prefs::kPasswordHashDataList);
  size_t num_erased = update->EraseListValueIf([&](const auto& dict) {
    return AreUsernamesSame(GetAndDecryptField(dict, kUsernameFieldKey),
                            IsGaiaPassword(dict), password_hash_data.username,
                            password_hash_data.is_gaia_password);
  });

  if (num_erased == 0 &&
      update->GetList().size() >= kMaxPasswordHashDataDictSize) {
    // Erase the oldest sign-in password hash data.
    update->EraseListIter(std::min_element(
        update->GetList().begin(), update->GetList().end(),
        [](const auto& lhs, const auto& rhs) {
          return lhs.FindKey(kLastSignInTimeFieldKey)->GetDouble() <
                 rhs.FindKey(kLastSignInTimeFieldKey)->GetDouble();
        }));
  }

  update->Append(std::move(encrypted_password_hash_entry));
  return true;
}

}  // namespace password_manager
