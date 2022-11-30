// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_OS_REGISTRY_UTIL_H_
#define CHROME_CHROME_CLEANER_OS_REGISTRY_UTIL_H_

#include <windows.h>

#include <stdint.h>

#include <string>
#include <vector>

namespace base {
namespace win {
class RegKey;
}  // namespace win
}  // namespace base

namespace chrome_cleaner {

class RegKeyPath;

// Possible error values returned by ReadRegistryValue().
enum class RegistryError : char {
  SUCCESS,
  FAILED_TO_OPEN_KEY,
  VALUE_NOT_FOUND,
  UNEXPECTED_ERROR,
};

namespace internal {

// This represents a registry value. It is used to log registry values through
// the logging service.
//
// This struct is related to the RegistryValue proto message that is sent in
// reports. They are kept separate because the data manipulated in the
// cleaner/reporter isn't necessarily the same that we want to transmit to
// Google. Another reason is that protos store strings as UTF-8, whereas the
// functions in base to manipulate user data obtained via the Windows API use
// 16-bits strings.
struct RegistryValue {
  std::wstring key_path;
  std::wstring value_name;
  std::wstring data;
};

}  // namespace internal

// The key for program uninstallers.
extern const wchar_t kUninstallerKeyPath[];

// The key for Chrome policies.
extern const wchar_t kChromePoliciesKeyPath[];

// The keys for the Chrome policy forcelist, whitelist and allowlist.
// Whitelist has been deprecated in favor of allowlist, but is still reported
// by the cleaner for compatibility with older versions of Chrome.
extern const wchar_t kChromePoliciesForcelistKeyPath[];
extern const wchar_t kChromePoliciesWhitelistKeyPathDeprecated[];
extern const wchar_t kChromePoliciesAllowlistKeyPath[];

// The keys for the Chromium policy forcelist, whitelist and allowlist.
// Whitelist has been deprecated in favor of allowlist, but is still reported
// by the cleaner for compatibility with older versions of Chromium.
extern const wchar_t kChromiumPoliciesForcelistKeyPath[];
extern const wchar_t kChromiumPoliciesWhitelistKeyPathDeprecated[];
extern const wchar_t kChromiumPoliciesAllowlistKeyPath[];

// Returns a string representation of the registry value type.
std::wstring RegistryValueTypeToString(DWORD value_type);

// Enumerates matching value names from a registry key against a given pattern
// with wild-cards.
void CollectMatchingRegistryNames(const base::win::RegKey& key,
                                  const std::wstring& pattern,
                                  const wchar_t escape_char,
                                  std::vector<std::wstring>* names);

// Enumerates matching key paths from a registry key against a given pattern
// with wild-cards. Returns a vector of fully qualified RegPath (i.e. wow64).
void CollectMatchingRegistryPaths(HKEY hkey,
                                  const std::wstring& pattern,
                                  const wchar_t escape_char,
                                  std::vector<RegKeyPath>* key_paths);

// Read a registry value of type REG_SZ, REG_EXPAND_SZ or REG_MULTI_SZ.  On
// success, |content| receives the content of the value and |content_type| the
// type of the value. On failure, return false and, if |error| is not NULL, set
// |error| to indicate the type of error code.
bool ReadRegistryValue(const base::win::RegKey& reg_key,
                       const wchar_t* value_name,
                       std::wstring* content,
                       uint32_t* content_type,
                       RegistryError* error);

// Convenience overload of ReadRegistryValue() that takes a RegKeyPath instead
// of base::win::RegKey. On failure, return false and, if |error| is not NULL,
// set |error| to indicate the type of error code.
bool ReadRegistryValue(const RegKeyPath& key_path,
                       const wchar_t* value_name,
                       std::wstring* content,
                       uint32_t* content_type,
                       RegistryError* error);

// Return a string representation of a potentially non-string registry type
// value. For string types, |raw_content| is simply copied into |content|, so
// caller must make sure that |raw_content| is properly null terminated. Note
// that |raw_content_bytes| is not the number of wchar_t in raw_content but the
// number of bytes.
void GetRegistryValueAsString(const wchar_t* raw_content,
                              size_t raw_content_bytes,
                              DWORD value_type,
                              std::wstring* content);

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_OS_REGISTRY_UTIL_H_
