// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/registry_test_data.h"

#include <windows.h>

#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/win/registry.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::win::RegKey;

RegistryTestData::RegistryTestData() : root_key_(nullptr) {}

RegistryTestData::~RegistryTestData() = default;

bool RegistryTestData::Initialize(HKEY root_key, const wchar_t* base_path) {
  if (root_key_) {
    return false;
  }

  registry_override_manager_.OverrideRegistry(root_key);
  if (::testing::Test::HasFatalFailure()) {
    return false;
  }
  LONG result = ERROR_SUCCESS;

  root_key_ = root_key;
  base_path_.assign(base_path);

  // Create our data.
  empty_key_path_ = base::StrCat({base_path_, L"\\EmptyKey"});
  non_empty_key_path_ = base::StrCat({base_path_, L"\\NonEmptyKey"});

  RegKey key;

  result = key.Create(root_key_, empty_key_path_.c_str(), KEY_QUERY_VALUE);
  if (result == ERROR_SUCCESS) {
    result = key.Create(root_key_, non_empty_key_path_.c_str(), KEY_WRITE);
  }
  if (result == ERROR_SUCCESS) {
    result = key.WriteValue(nullptr, non_empty_key_path_.c_str());
  }
  if (result == ERROR_SUCCESS) {
    result = key.CreateKey(L"SubKey", KEY_WRITE);
  }
  if (result == ERROR_SUCCESS) {
    result = key.WriteValue(L"SomeValue", 1UL);
  }
  DLOG_IF(DFATAL, result != ERROR_SUCCESS)
      << "Failed to create test registry data based at " << base_path_
      << ", result: " << result;

  return result == ERROR_SUCCESS;
}

void RegistryTestData::ExpectMatchesNonEmptyKey(HKEY root_key,
                                                const wchar_t* path) {
  RegKey key;

  EXPECT_EQ(ERROR_SUCCESS, key.Open(root_key, path, KEY_READ));
  std::wstring str_value;
  EXPECT_EQ(ERROR_SUCCESS, key.ReadValue(nullptr, &str_value));
  EXPECT_EQ(non_empty_key_path_, str_value);
  EXPECT_EQ(ERROR_SUCCESS, key.OpenKey(L"Subkey", KEY_READ));
  DWORD dw_value = 0;
  EXPECT_EQ(ERROR_SUCCESS, key.ReadValueDW(L"SomeValue", &dw_value));
  EXPECT_EQ(1UL, dw_value);
}

// static
void RegistryTestData::ExpectEmptyKey(HKEY root_key, const wchar_t* path) {
  DWORD num_subkeys = 0;
  DWORD num_values = 0;
  RegKey key;
  EXPECT_EQ(ERROR_SUCCESS, key.Open(root_key, path, KEY_READ));
  EXPECT_EQ(ERROR_SUCCESS,
            RegQueryInfoKey(key.Handle(), nullptr, nullptr, nullptr,
                            &num_subkeys, nullptr, nullptr, &num_values,
                            nullptr, nullptr, nullptr, nullptr));
  EXPECT_EQ(0UL, num_subkeys);
  EXPECT_EQ(0UL, num_values);
}
