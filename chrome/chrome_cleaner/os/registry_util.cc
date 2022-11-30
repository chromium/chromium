// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/os/registry_util.h"

#include <stdint.h>

#include <map>
#include <string>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"
#include "chrome/chrome_cleaner/os/disk_util.h"
#include "chrome/chrome_cleaner/os/registry.h"
#include "chrome/chrome_cleaner/os/system_util.h"
#include "chrome/chrome_cleaner/strings/string_util.h"
#include "components/chrome_cleaner/public/constants/constants.h"

namespace chrome_cleaner {

namespace {

// The maximal number of tries to read a registry key before failing.
const unsigned int kMaxRegistryReadIterations = 5;

// Initial size of the buffer that holds the name of a value, in characters.
const size_t kValueNameBufferSize = 256;

// The delimiter used within registry key path.
const wchar_t kRegistrySubkeyDelimiter = L'\\';

// The number of extra bytes to add in the buffer used to read registry strings.
const size_t kNumExtraBytesForRegistryStrings = 3;

// Split the pattern into path components. For example, with the pattern
// 'ab??/x*/abc', |head| receives the component 'ab??' and |rest| receives the
// remaining components 'x*/abc'.
void ExtractHeadingSubkeyComponent(const std::wstring& pattern,
                                   const wchar_t escape_char,
                                   std::wstring* head,
                                   std::wstring* rest) {
  DCHECK(head);
  DCHECK(rest);

  for (size_t offset = 0; offset < pattern.size(); ++offset) {
    if (pattern[offset] == escape_char) {
      // Skip the escape character.
      ++offset;
    }
    if (pattern[offset] == kRegistrySubkeyDelimiter) {
      *head = pattern.substr(0, offset);
      *rest = pattern.substr(offset + 1);
      return;
    }
  }

  *head = pattern;
  *rest = L"";
}

// Retrieve matching registry keys against a pattern with wild-cards. This
// algorithm matches recursively all registry keys for the given pattern:
// |hkey|\|key_path|\|pattern|. For each recursive invocation, the algorithm
// removes the heading key path component of the pattern, enumerates matching
// subkeys and moves the component to the |key_path| part. |path_masks| receives
// the wow64access masks for each existing path.
void CollectMatchingRegistryPathsRecursive(
    HKEY hkey,
    const std::wstring& key_path,
    const std::wstring& pattern,
    const wchar_t escape_char,
    REGSAM wow64access,
    std::map<std::wstring, REGSAM>* path_masks) {
  DCHECK(path_masks);

  if (pattern.empty()) {
    if (RegKeyPath(hkey, key_path.c_str(), wow64access).Exists())
      (*path_masks)[key_path] |= wow64access;
    return;
  }

  // Extract the first key_path component of the pattern.
  std::wstring subkey_pattern;
  std::wstring remaining_pattern;
  ExtractHeadingSubkeyComponent(pattern, escape_char, &subkey_pattern,
                                &remaining_pattern);

  std::wstring current_prefix;
  if (!key_path.empty())
    current_prefix = key_path + kRegistrySubkeyDelimiter;

  // If there is no wild-card into the first component, append it and
  // continue with the following components.
  if (!NameContainsWildcards(subkey_pattern)) {
    CollectMatchingRegistryPathsRecursive(hkey, current_prefix + subkey_pattern,
                                          remaining_pattern, escape_char,
                                          wow64access, path_masks);
    return;
  }

  // If the first component contains a wild-card, enumerate the registry key
  // and continue matching on registry keys that match the pattern.
  base::win::RegistryKeyIterator subkeys_it(hkey, key_path.c_str(),
                                            wow64access);
  for (; subkeys_it.Valid(); ++subkeys_it) {
    std::wstring subkey_name = subkeys_it.Name();
    if (WStringWildcardMatchInsensitive(subkey_name, subkey_pattern,
                                        escape_char)) {
      CollectMatchingRegistryPathsRecursive(hkey, current_prefix + subkey_name,
                                            remaining_pattern, escape_char,
                                            wow64access, path_masks);
    }
  }
}

}  // namespace

const wchar_t kUninstallerKeyPath[] =
    L"software\\microsoft\\windows\\currentversion\\uninstall";

const wchar_t kChromePoliciesKeyPath[] = L"software\\policies\\google\\chrome";

const wchar_t kChromePoliciesForcelistKeyPath[] =
    L"software\\policies\\google\\chrome\\ExtensionInstallForcelist";
const wchar_t kChromePoliciesWhitelistKeyPathDeprecated[] =
    L"software\\policies\\google\\chrome\\ExtensionInstallWhitelist";
const wchar_t kChromePoliciesAllowlistKeyPath[] =
    L"software\\policies\\google\\chrome\\ExtensionInstallAllowlist";

const wchar_t kChromiumPoliciesForcelistKeyPath[] =
    L"software\\policies\\chromium\\ExtensionInstallForcelist";
const wchar_t kChromiumPoliciesWhitelistKeyPathDeprecated[] =
    L"software\\policies\\chromium\\ExtensionInstallWhitelist";
const wchar_t kChromiumPoliciesAllowlistKeyPath[] =
    L"software\\policies\\chromium\\ExtensionInstallAllowlist";

std::wstring RegistryValueTypeToString(DWORD value_type) {
  switch (value_type) {
    case REG_BINARY:
      return L"REG_BINARY";
    case REG_DWORD:
      return L"REG_DWORD";
    case REG_DWORD_BIG_ENDIAN:
      return L"REG_DWORD_BIG_ENDIAN";
    case REG_EXPAND_SZ:
      return L"REG_EXPAND_SZ";
    case REG_LINK:
      return L"REG_LINK";
    case REG_MULTI_SZ:
      return L"REG_MULTI_SZ";
    case REG_NONE:
      return L"REG_NONE";
    case REG_QWORD:
      return L"REG_QWORD";
    case REG_SZ:
      return L"REG_SZ";
    default:
      LOG(WARNING) << "Unknown registry value type (" << value_type << ").";
      return base::NumberToWString(value_type);
  }
}

void CollectMatchingRegistryNames(const base::win::RegKey& key,
                                  const std::wstring& pattern,
                                  const wchar_t escape_char,
                                  std::vector<std::wstring>* names) {
  DCHECK(names);

  // If there is no wild-card, return the pattern as-is.
  if (!NameContainsWildcards(pattern)) {
    names->push_back(pattern);
    return;
  }

  // Enumerates value names under the registry key |key|.
  DWORD index = 0;
  std::vector<wchar_t> value_name(kValueNameBufferSize);
  while (true) {
    for (unsigned int iteration = 0; iteration < kMaxRegistryReadIterations;
         ++iteration) {
      DWORD value_name_size = static_cast<DWORD>(value_name.size());
      LONG res_enum =
          ::RegEnumValue(key.Handle(), index, &value_name[0], &value_name_size,
                         nullptr, nullptr, nullptr, nullptr);
      if (res_enum == ERROR_NO_MORE_ITEMS) {
        return;
      } else if (res_enum == ERROR_MORE_DATA &&
                 value_name_size > static_cast<DWORD>(value_name.size())) {
        value_name.resize(value_name_size);
        // Try an other iteration.
        continue;
      } else if (res_enum != ERROR_SUCCESS) {
        DLOG(ERROR) << "Received error code " << res_enum
                    << " while enumerating value names from key.";
        return;
      } else {
        // Check whether the value matches the given pattern.
        if (NameMatchesPattern(&value_name[0], pattern, escape_char))
          names->push_back(&value_name[0]);
        break;
      }
    }

    // Move to the next registry value name.
    ++index;
  }
}

void CollectMatchingRegistryPaths(HKEY hkey,
                                  const std::wstring& pattern,
                                  const wchar_t escape_char,
                                  std::vector<RegKeyPath>* key_paths) {
  DCHECK(key_paths);
  // We can query for key reflection, but not redirection. To avoid many special
  // cases here about which keys are remapped, we scan the Win32 and Win64 space
  // independently and remove duplicates after the fact.
  std::map<std::wstring, REGSAM> key_path_masks;
  if (!NameContainsWildcards(pattern)) {
    // If there is no wild-card, just check whether the key exists.
    if (RegKeyPath(hkey, pattern.c_str(), KEY_WOW64_32KEY).Exists())
      key_path_masks[pattern] |= KEY_WOW64_32KEY;
    if (RegKeyPath(hkey, pattern.c_str(), KEY_WOW64_64KEY).Exists())
      key_path_masks[pattern] |= KEY_WOW64_64KEY;
  } else {
    // Recursively scan both the 32 and 64-bit view of the registry.
    CollectMatchingRegistryPathsRecursive(hkey, L"", pattern, escape_char,
                                          KEY_WOW64_32KEY, &key_path_masks);
    if (IsX64Architecture()) {
      CollectMatchingRegistryPathsRecursive(hkey, L"", pattern, escape_char,
                                            KEY_WOW64_64KEY, &key_path_masks);
    }
  }

  // Remove duplicates where no key remapping was performed.
  for (const auto& it : key_path_masks) {
    if (it.second == (KEY_WOW64_32KEY | KEY_WOW64_64KEY)) {
      const RegKeyPath path32(hkey, it.first.c_str(), KEY_WOW64_32KEY);
      const RegKeyPath path64(hkey, it.first.c_str(), KEY_WOW64_64KEY);
      if (path32.IsEquivalent(path64)) {
        key_paths->emplace_back(hkey, it.first.c_str(), KEY_WOW64_32KEY);
      } else {
        key_paths->push_back(path32);
        key_paths->push_back(path64);
      }
    } else {
      key_paths->emplace_back(hkey, it.first.c_str(), it.second);
    }
  }
}

bool ReadRegistryValue(const base::win::RegKey& reg_key,
                       const wchar_t* value_name,
                       std::wstring* content,
                       uint32_t* content_type,
                       RegistryError* error) {
  DWORD content_bytes = 0;
  // Always keep more bytes in the buffer so we can 1) start with a valid
  // buffer to call with a 0 size request, and 2) make room to insert a
  // potentially missing null wchar_t, and 3) make the size even when it's odd.
  std::vector<BYTE> buffer(content_bytes + kNumExtraBytesForRegistryStrings);
  DWORD type = REG_NONE;

  unsigned int iteration = 0;
  while (true) {
    // Fail after trying to read the value too many times.
    if (iteration++ >= kMaxRegistryReadIterations) {
      if (error)
        *error = RegistryError::UNEXPECTED_ERROR;
      return false;
    }

    DWORD result =
        reg_key.ReadValue(value_name, &buffer[0], &content_bytes, &type);
    if (result == ERROR_SUCCESS)
      break;
    if (result == ERROR_FILE_NOT_FOUND) {
      if (error)
        *error = RegistryError::VALUE_NOT_FOUND;
      return false;
    }
    if (result != ERROR_MORE_DATA) {
      DLOG(ERROR) << "Unexpected result from RegQueryValueEx: " << result
                  << ", value = '" << value_name << "'.";
      if (error)
        *error = RegistryError::UNEXPECTED_ERROR;
      return false;
    }

    // Not enough space for the registry key content. Resize the buffer and
    // try again. Add kNumExtraBytesForRegistryStrings in case we need to
    // complete/add a null terminating wchar_t.
    DCHECK_LT(buffer.size(), content_bytes + kNumExtraBytesForRegistryStrings);
    buffer.resize(content_bytes + kNumExtraBytesForRegistryStrings);
  }

  // Accept empty content.
  if (content_bytes == 0) {
    if (content)
      *content = std::wstring();
    if (content_type)
      *content_type = type;
    if (error)
      *error = RegistryError::SUCCESS;
    return true;
  }

  if (content) {
    // For non string types, simply convert the value to a string.
    if (type != REG_SZ && type != REG_EXPAND_SZ && type != REG_MULTI_SZ) {
      const std::wstring::value_type* wide_buffer =
          reinterpret_cast<std::wstring::value_type*>(&buffer[0]);
      GetRegistryValueAsString(wide_buffer, content_bytes, type, content);
    } else {
      // We may need to fix the null termination of the string read from the
      // registry, make sure we have enough room.
      DCHECK_GE(buffer.size(),
                content_bytes + kNumExtraBytesForRegistryStrings);

      // This can happen if other apps wrote the value as binary. There are no
      // strict rules for writing strings as binaries in the registry. We have
      // seen wide char strings returned with a single byte for the null
      // terminating char, which must be two bytes for wchar_t.
      if (content_bytes % 2)
        buffer[content_bytes++] = '\0';

      // Also make sure a full null terminating wchar_t has been added. It's not
      // always the case either.
      DCHECK_GT(content_bytes, 1UL);
      if (buffer[content_bytes - 1] || buffer[content_bytes - 2]) {
        buffer[content_bytes++] = '\0';
        buffer[content_bytes++] = '\0';
      }
      DCHECK_LE(content_bytes, buffer.size());

      // Returns the content of the registry value.
      const std::wstring::value_type* wide_buffer =
          reinterpret_cast<std::wstring::value_type*>(&buffer[0]);
      *content = std::wstring(wide_buffer, content_bytes / 2 - 1);
    }
  }
  if (content_type)
    *content_type = type;
  if (error)
    *error = RegistryError::SUCCESS;
  return true;
}

bool ReadRegistryValue(const RegKeyPath& key_path,
                       const wchar_t* value_name,
                       std::wstring* content,
                       uint32_t* content_type,
                       RegistryError* error) {
  DCHECK(value_name);
  base::win::RegKey reg_key;
  if (!key_path.Open(KEY_QUERY_VALUE, &reg_key)) {
    DLOG(ERROR) << "Failed to open registry key: " << key_path.FullPath();
    if (error)
      *error = RegistryError::FAILED_TO_OPEN_KEY;
    return false;
  }
  // ReadRegistryValue() already logs to DLOG(ERROR), so no need to log here.
  return ReadRegistryValue(reg_key, value_name, content, content_type, error);
}

void GetRegistryValueAsString(const wchar_t* raw_content,
                              size_t raw_content_bytes,
                              DWORD value_type,
                              std::wstring* content) {
  DCHECK(raw_content);
  DCHECK(content);

  if (value_type == REG_SZ || value_type == REG_EXPAND_SZ ||
      value_type == REG_MULTI_SZ) {
    *content = raw_content;
  } else if (value_type == REG_DWORD) {
    DWORD dword_value = *reinterpret_cast<const DWORD*>(raw_content);
    *content = base::StringPrintf(L"%08x", dword_value);
  } else {
    // The content displayed by this fallback is a sequence of bytes in
    // little-endian, which give strange display for numeric values (i.e
    // 01000000 instead of 00000001)
    *content = base::ASCIIToWide(base::HexEncode(
        reinterpret_cast<const char*>(raw_content), raw_content_bytes));
  }
}

}  // namespace chrome_cleaner
