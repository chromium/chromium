// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/key_storage_libsecret.h"

#include "base/base64.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "build/branding_buildflags.h"
#include "components/os_crypt/libsecret_util_linux.h"

namespace {

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
const char kApplicationName[] = "chrome";
#else
const char kApplicationName[] = "chromium";
#endif

// Deprecated in M55 (crbug.com/639298)
const SecretSchema kKeystoreSchemaV1 = {
    "chrome_libsecret_os_crypt_password",
    SECRET_SCHEMA_NONE,
    {
        {nullptr, SECRET_SCHEMA_ATTRIBUTE_STRING},
    }};

const SecretSchema kKeystoreSchemaV2 = {
    "chrome_libsecret_os_crypt_password_v2",
    SECRET_SCHEMA_DONT_MATCH_NAME,
    {
        {"application", SECRET_SCHEMA_ATTRIBUTE_STRING},
        {nullptr, SECRET_SCHEMA_ATTRIBUTE_STRING},
    }};

// From a search result, extracts a SecretValue, asserting that there was at
// most one result. Returns nullptr if there are no results.
SecretValue* ToSingleSecret(GList* secret_items) {
  GList* first = g_list_first(secret_items);
  if (first == nullptr)
    return nullptr;
  if (g_list_next(first) != nullptr) {
    VLOG(1) << "OSCrypt found more than one encryption keys.";
  }
  SecretItem* secret_item = static_cast<SecretItem*>(first->data);
  SecretValue* secret_value =
      LibsecretLoader::secret_item_get_secret(secret_item);
  return secret_value;
}

}  // namespace

std::string KeyStorageLibsecret::AddRandomPasswordInLibsecret() {
  std::string password;
  base::Base64Encode(base::RandBytesAsString(16), &password);
  GError* error = nullptr;
  bool success = LibsecretLoader::secret_password_store_sync(
      &kKeystoreSchemaV2, nullptr, KeyStorageLinux::kKey, password.c_str(),
      nullptr, &error, "application", kApplicationName, nullptr);
  if (error) {
    VLOG(1) << "Libsecret lookup failed: " << error->message;
    g_error_free(error);
    return std::string();
  }
  if (!success) {
    VLOG(1) << "Libsecret lookup failed.";
    return std::string();
  }

  VLOG(1) << "OSCrypt generated a new password.";
  return password;
}

std::string KeyStorageLibsecret::GetKeyImpl() {
  LibsecretAttributesBuilder attrs;
  attrs.Append("application", kApplicationName);

  LibsecretLoader::SearchHelper helper;
  helper.Search(&kKeystoreSchemaV2, attrs.Get(),
                SECRET_SEARCH_UNLOCK | SECRET_SEARCH_LOAD_SECRETS);
  if (!helper.success()) {
    VLOG(1) << "Libsecret lookup failed: " << helper.error()->message;
    return std::string();
  }

  SecretValue* password_libsecret = ToSingleSecret(helper.results());
  if (!password_libsecret) {
    std::string password = Migrate();
    if (!password.empty())
      return password;
    return AddRandomPasswordInLibsecret();
  }
  std::string password(
      LibsecretLoader::secret_value_get_text(password_libsecret));
  LibsecretLoader::secret_value_unref(password_libsecret);
  return password;
}

bool KeyStorageLibsecret::Init() {
  bool loaded = LibsecretLoader::EnsureLibsecretLoaded();
  if (loaded)
    LibsecretLoader::EnsureKeyringUnlocked();
  return loaded;
}

std::string KeyStorageLibsecret::Migrate() {
  LibsecretAttributesBuilder attrs;

  // Detect old entry.
  LibsecretLoader::SearchHelper helper;
  helper.Search(&kKeystoreSchemaV1, attrs.Get(),
                SECRET_SEARCH_UNLOCK | SECRET_SEARCH_LOAD_SECRETS);
  if (!helper.success())
    return std::string();

  SecretValue* password_libsecret = ToSingleSecret(helper.results());
  if (!password_libsecret)
    return std::string();

  VLOG(1) << "OSCrypt detected a deprecated password in Libsecret.";
  std::string password(
      LibsecretLoader::secret_value_get_text(password_libsecret));
  LibsecretLoader::secret_value_unref(password_libsecret);

  // Create new entry.
  GError* error = nullptr;
  bool success = LibsecretLoader::secret_password_store_sync(
      &kKeystoreSchemaV2, nullptr, KeyStorageLinux::kKey, password.c_str(),
      nullptr, &error, "application", kApplicationName, nullptr);
  if (error) {
    VLOG(1) << "Failed to store migrated password. " << error->message;
    g_error_free(error);
    return std::string();
  }
  if (!success) {
    VLOG(1) << "Failed to store migrated password.";
    return std::string();
  }

  // Delete old entry.
  // Even if deletion failed, we have to use the password that we created.
  success = LibsecretLoader::secret_password_clear_sync(
      &kKeystoreSchemaV1, nullptr, &error, nullptr);
  if (error) {
    VLOG(1) << "OSCrypt failed to delete deprecated password. "
            << error->message;
    g_error_free(error);
  }

  VLOG(1) << "OSCrypt migrated from deprecated password.";

  return password;
}
