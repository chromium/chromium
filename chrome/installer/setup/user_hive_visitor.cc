// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/setup/user_hive_visitor.h"

#include <string>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"
#include "chrome/installer/util/scoped_token_privilege.h"
#include "components/base32/base32.h"

namespace installer {

namespace {

// A helper for loading and opening a hive into a random subkey of
// HKEY_LOCAL_MACHINE.
class ScopedUserHive {
 public:
  explicit ScopedUserHive(const base::FilePath& hive_file);

  ScopedUserHive(const ScopedUserHive&) = delete;
  ScopedUserHive& operator=(const ScopedUserHive&) = delete;

  ~ScopedUserHive();

  // Returns true if the hive file was loaded.
  bool valid() const { return key_.Valid(); }

  // Returns the key at the root of the loaded hive, or nullptr if not valid.
  base::win::RegKey* key() { return key_.Valid() ? &key_ : nullptr; }

 private:
  // The randomly-chosen name of the subkey under HKLM where the file is loaded.
  // If empty, the file is not loaded.
  std::wstring subkey_name_;

  // The loaded key.
  base::win::RegKey key_;
};

ScopedUserHive::ScopedUserHive(const base::FilePath& hive_file) {
  // Generate a random name for the key at which the file will be loaded.
  subkey_name_ = base::ASCIIToWide(base32::Base32Encode(
      base::RandBytesAsVector(10), base32::Base32EncodePolicy::OMIT_PADDING));
  DCHECK_EQ(16U, subkey_name_.size());

  LONG result = ::RegLoadKey(HKEY_LOCAL_MACHINE, subkey_name_.c_str(),
                             hive_file.value().c_str());
  if (result != ERROR_SUCCESS) {
    // Clear subkey_name_ since the load failed so that an unload will not be
    // attempted in the dtor.
    subkey_name_.clear();
    ::SetLastError(result);
    PLOG(ERROR) << "Failed loading user hive file \"" << hive_file.value()
                << "\"";
    return;
  }

  // Open the newly-loaded key.
  result = key_.Open(HKEY_LOCAL_MACHINE, subkey_name_.c_str(), KEY_ALL_ACCESS);
  if (result != ERROR_SUCCESS) {
    ::SetLastError(result);
    PLOG(ERROR) << "Failed opening loaded hive file \"" << hive_file.value()
                << "\"";
  }
}

ScopedUserHive::~ScopedUserHive() {
  key_.Close();
  if (subkey_name_.empty())
    return;
  LONG result = ::RegUnLoadKey(HKEY_LOCAL_MACHINE, subkey_name_.c_str());
  if (result != ERROR_SUCCESS) {
    ::SetLastError(result);
    PLOG(ERROR) << "Failed unloading user hive at \"" << subkey_name_ << "\"";
  }
}

bool OpenUserHive(const wchar_t* sid, base::win::RegKey* user_hive) {
  DCHECK(user_hive);
  LONG result = user_hive->Open(HKEY_USERS, sid, KEY_ALL_ACCESS);
  if (result == ERROR_SUCCESS)
    return true;
  if (result == ERROR_FILE_NOT_FOUND) {
    VLOG(1) << "Hive is not loaded for user \"" << sid << "\"";
    return false;
  }
  ::SetLastError(result);
  PLOG(ERROR) << "Failed opening hive for user \"" << sid << "\"";
  return false;
}

}  // namespace

void VisitUserHives(const HiveVisitor& visitor) {
  constexpr wchar_t kProfileListKey[] =
      L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\ProfileList";

  // Privileges required to load a registry hive file.
  ScopedTokenPrivilege se_backup_name_privilege(SE_BACKUP_NAME);
  ScopedTokenPrivilege se_restore_name_privilege(SE_RESTORE_NAME);

  for (base::win::RegistryKeyIterator iter(HKEY_LOCAL_MACHINE, kProfileListKey);
       iter.Valid(); ++iter) {
    const wchar_t* sid = iter.Name();
    // First try to access the user hive pre-mounted by the OS.
    VLOG(1) << "Checking for pre-loaded hive for local account \"" << sid
            << "\"";
    base::win::RegKey key;
    if (OpenUserHive(sid, &key)) {
      VLOG(1) << "Found loaded hive for sid \"" << sid << "\"";
      if (!visitor.Run(sid, &key))
        break;
      continue;
    }

    // Read the path to the profile directory to load the hive manually.
    std::wstring profile_key_name(kProfileListKey);
    profile_key_name.append(1, L'\\').append(sid);
    LONG result =
        key.Open(HKEY_LOCAL_MACHINE, profile_key_name.c_str(), KEY_QUERY_VALUE);
    if (result != ERROR_SUCCESS) {
      ::SetLastError(result);
      PLOG(ERROR) << "Failed opening profile key \"" << profile_key_name
                  << "\"";
      continue;
    }
    std::wstring image_path;
    result = key.ReadValue(L"ProfileImagePath", &image_path);
    if (result != ERROR_SUCCESS) {
      ::SetLastError(result);
      PLOG(ERROR) << "Failed reading ProfileImagePath value of \""
                  << profile_key_name << "\"";
    }
    key.Close();
    if (image_path.empty())
      continue;

    base::FilePath hive_file(
        base::FilePath(image_path).Append(FILE_PATH_LITERAL("ntuser.dat")));
    VLOG(1) << "Falling back to opening \"" << hive_file.value() << "\"";
    if (!base::PathExists(hive_file)) {
      VPLOG(1) << "Hive file not found or inaccessible \"" << hive_file.value()
               << "\"";
      continue;
    }
    ScopedUserHive user_hive(hive_file);
    if (user_hive.valid()) {
      VLOG(1) << "Loaded and opened hive for sid \"" << sid << "\"";
      if (!visitor.Run(sid, user_hive.key()))
        break;
    }
  }
}

}  // namespace installer
