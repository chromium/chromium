// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/os/registry.h"

#include <memory>

#include "base/logging.h"
#include "base/win/registry.h"
#include "chrome/chrome_cleaner/os/nt_internals.h"
#include "chrome/chrome_cleaner/os/system_util.h"
#include "chrome/chrome_cleaner/strings/string_util.h"

namespace chrome_cleaner {
namespace {

// Mask to pull WOW64 access flags out of REGSAM access.
const REGSAM kWow64AccessMask = KEY_WOW64_32KEY | KEY_WOW64_64KEY;

const wchar_t* HKeyToString(HKEY hkey) {
  if (hkey == HKEY_LOCAL_MACHINE)
    return L"HKLM";
  else if (hkey == HKEY_CURRENT_USER)
    return L"HKCU";
  else if (hkey == HKEY_CLASSES_ROOT)
    return L"HKCR";
  else
    return L"<unknown>";
}

}  // namespace

bool IsPredefinedRegistryHandle(HANDLE key) {
  return key == nullptr || key == INVALID_HANDLE_VALUE ||
         key == HKEY_CLASSES_ROOT || key == HKEY_CURRENT_CONFIG ||
         key == HKEY_CURRENT_USER || key == HKEY_LOCAL_MACHINE ||
         key == HKEY_USERS;
}

bool GetNativeKeyPath(const base::win::RegKey& key,
                      std::wstring* native_key_path) {
  // This function uses a native API to determine the key path seen by the
  // kernel. See:
  // https://msdn.microsoft.com/en-us/library/windows/hardware/ff553373(v=vs.85).aspx
  DCHECK(key.Valid());
  DCHECK(native_key_path);

  DWORD bytes = 0;
  NTSTATUS result = NtQueryKey(key.Handle(), KeyNameInformation, 0, 0, &bytes);
  if (result != STATUS_BUFFER_TOO_SMALL) {
    LOG(ERROR) << "Cannot query registry key (hr = " << result << ").";
    return false;
  }

  std::unique_ptr<char[]> buffer(new char[bytes]);
  result =
      NtQueryKey(key.Handle(), KeyNameInformation, buffer.get(), bytes, &bytes);
  if (result != STATUS_SUCCESS) {
    LOG(ERROR) << "Cannot query registry key (hr = " << result << ").";
    return false;
  }

  _KEY_NAME_INFORMATION* key_name =
      reinterpret_cast<_KEY_NAME_INFORMATION*>(buffer.get());
  // The |NameLength| size is in bytes.
  *native_key_path =
      std::wstring(key_name->Name, key_name->NameLength / sizeof(wchar_t));
  return true;
}

// Doesn't reuse RegKeyPath() that takes 3 params because rootkey_ shouldn't be
// DCHECK'ed.
RegKeyPath::RegKeyPath() : rootkey_(nullptr), wow64access_(0) {}

RegKeyPath::RegKeyPath(HKEY rootkey, const std::wstring& subkey)
    : RegKeyPath(rootkey, subkey, 0) {
  DCHECK(IsPredefinedRegistryHandle(rootkey));
}

RegKeyPath::RegKeyPath(HKEY rootkey,
                       const std::wstring& subkey,
                       REGSAM wow64access)
    : rootkey_(rootkey), subkey_(subkey), wow64access_(wow64access) {
  DCHECK_NE(static_cast<HKEY>(nullptr), rootkey_);
  DCHECK(IsPredefinedRegistryHandle(rootkey));
  DCHECK(wow64access_ == KEY_WOW64_32KEY || wow64access_ == KEY_WOW64_64KEY ||
         wow64access_ == 0);
}

bool RegKeyPath::Exists() const {
  DCHECK_NE(static_cast<HKEY>(nullptr), rootkey_);
  base::win::RegKey check_key(rootkey_, subkey_.c_str(),
                              READ_CONTROL | wow64access_);
  return check_key.Valid();
}

bool RegKeyPath::HasValue(const wchar_t* value_name) const {
  DCHECK(value_name);
  base::win::RegKey key;
  if (!Open(KEY_READ, &key))
    return false;
  return key.HasValue(value_name);
}

bool RegKeyPath::Open(REGSAM access, base::win::RegKey* key) const {
  DCHECK_NE(static_cast<HKEY>(nullptr), rootkey_);
  DCHECK(key);
  // Check if the key doesn't exist and avoid creating it.
  if (!Exists())
    return false;
  return Create(access, key);
}

bool RegKeyPath::Create(REGSAM access, base::win::RegKey* key) const {
  DCHECK_NE(static_cast<HKEY>(nullptr), rootkey_);
  DCHECK_EQ(static_cast<REGSAM>(0), access & kWow64AccessMask);
  DCHECK(key);
  // We have to replicate the Open vs Create logic from the RegKey constructor
  // here as RegKey is non-copyable.
  access |= wow64access_;
  LONG key_result = ERROR_SUCCESS;
  if (access & (KEY_SET_VALUE | KEY_CREATE_SUB_KEY | KEY_CREATE_LINK))
    key_result = key->Create(rootkey_, subkey_.c_str(), access);
  else
    key_result = key->Open(rootkey_, subkey_.c_str(), access);
  VPLOG_IF(1, key_result != ERROR_SUCCESS)
      << "Failed to open key for update: " << subkey_
      << ", under root: " << HKeyToString(rootkey_)
      << ", with access: " << access << ", result: " << key_result << " "
      << logging::SystemErrorCodeToString(key_result);
  if (!key->Valid()) {
    key->Close();
    return false;
  }
  return true;
}

std::wstring RegKeyPath::FullPath() const {
  std::wstring path = HKeyToString(rootkey());
  if (wow64access() == KEY_WOW64_32KEY)
    path += L":32";
  else if (wow64access() == KEY_WOW64_64KEY)
    path += L":64";
  path += L'\\' + subkey();
  return path;
}

bool RegKeyPath::GetNativeFullPath(std::wstring* native_path) const {
  base::win::RegKey key;
  if (!Open(KEY_READ, &key))
    return false;
  return GetNativeKeyPath(key, native_path);
}

bool RegKeyPath::operator==(const RegKeyPath& other) const {
  return rootkey_ == other.rootkey_ &&
         WStringEqualsCaseInsensitive(subkey_, other.subkey_) &&
         wow64access_ == other.wow64access_;
}

bool RegKeyPath::IsEquivalent(const RegKeyPath& other) const {
  // For consistent behavior, stop immediately if either of the key paths is
  // not valid.
  std::wstring key_path;
  std::wstring other_key_path;
  if (!GetNativeFullPath(&key_path) ||
      !other.GetNativeFullPath(&other_key_path)) {
    return false;
  }

  return WStringEqualsCaseInsensitive(key_path, other_key_path);
}

bool RegKeyPath::operator<(const RegKeyPath& other) const {
  if (rootkey_ != other.rootkey_)
    return rootkey_ < other.rootkey_;
  if (wow64access_ != other.wow64access_)
    return wow64access_ < other.wow64access_;
  return _wcsicmp(subkey_.c_str(), other.subkey_.c_str()) < 0;
}

// static.
std::set<RegKeyPath> RegKeyPath::FindExisting(const std::vector<HKEY>& rootkeys,
                                              const wchar_t* subkey) {
  DCHECK(subkey);
  std::set<RegKeyPath> result;
  for (const HKEY root : rootkeys) {
    const RegKeyPath key32(root, subkey, KEY_WOW64_32KEY);
    const RegKeyPath key64(root, subkey, KEY_WOW64_64KEY);
    if (!IsX64Architecture() || key32.IsEquivalent(key64)) {
      // The key is not redirected; store it without an explicit view.
      const RegKeyPath key(root, subkey);
      if (key.Exists())
        result.insert(key);
    } else {
      // The key is being redirected and may exist independently in the 32 and
      // 64-bit views of the registry.
      if (key32.Exists())
        result.insert(key32);
      if (key64.Exists())
        result.insert(key64);
    }
  }
  return result;
}

}  // namespace chrome_cleaner
