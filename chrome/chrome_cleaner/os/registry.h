// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_OS_REGISTRY_H_
#define CHROME_CHROME_CLEANER_OS_REGISTRY_H_

#include <windows.h>

#include <set>
#include <string>
#include <vector>

namespace base {
namespace win {
class RegKey;
}  // namespace win
}  // namespace base

namespace chrome_cleaner {

bool GetNativeKeyPath(const base::win::RegKey& key,
                      std::wstring* native_key_path);

// Returns true for predefined handles, such as NULL, INVALID_HANDLE_VALUE, and
// for predefined registry root keys, such as HKEY_CLASSES_ROOT.
bool IsPredefinedRegistryHandle(HANDLE key);

// Utility class to store a registry key path. Unlike a RegKey, this class is
// copy/moveable.
// It stores |rootkey|, which must be a predefined registry root key.
// TODO(veranika): to make the requirement of a predefined registry root key
// explicit, change the constructors to take the PredefinedHandle enum instead
// of the broader HKEY.
class RegKeyPath {
 public:
  RegKeyPath();
  RegKeyPath(HKEY rootkey, const std::wstring& subkey);

  // Create a path with an explicit wow64 view. Permissible values are
  // KEY_WOW64_32KEY, KEY_WOW64_64KEY and 0 (default based target architecture).
  RegKeyPath(HKEY rootkey, const std::wstring& subkey, REGSAM wow64access);

  HKEY rootkey() const { return rootkey_; }
  const std::wstring& subkey() const { return subkey_; }
  REGSAM wow64access() const { return wow64access_; }

  // Return whether the key exists (without creating it).
  bool Exists() const;

  // Return whether a value name exists for a key (without creating it).
  bool HasValue(const wchar_t* value_name) const;

  // Open an already existing registry key. Returns false on failure.
  bool Open(REGSAM access, base::win::RegKey* key) const;

  // Like Open, but will create the key if it does not exist yet.
  bool Create(REGSAM access, base::win::RegKey* key) const;

  // Return the full path as a string. Intended for logging purposes only.
  std::wstring FullPath() const;

  // Return the native full path as a string.
  bool GetNativeFullPath(std::wstring* native_path) const;

  // Test whether two paths are exactly identical. This returns false if the
  // same key is addressed with different Wow64 access bits. Behaviour is
  // unaffected by whether the key path exists or not.
  bool operator==(const RegKeyPath& other) const;

  // Test whether two paths are equivalent. If the same key path is addressed
  // with different Wow64 access bits, the last write timestamp is used to
  // deduce whether the same key is being referenced. Return false if at least
  // one of the two keys does not exist.
  bool IsEquivalent(const RegKeyPath& other) const;

  // Required for some STL containers and operations.
  bool operator<(const RegKeyPath& other) const;

  // Return all keys for the given subkey under all roots listed. The result set
  // contains only existing keys and may explicitly specify 32 or 64bit views
  // where WoW redirection is enabled.
  static std::set<RegKeyPath> FindExisting(const std::vector<HKEY>& rootkeys,
                                           const wchar_t* subkey);

 private:
  HKEY rootkey_;
  std::wstring subkey_;
  REGSAM wow64access_;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_OS_REGISTRY_H_
