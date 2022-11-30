// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/test/test_registry_util.h"

#include "base/check_op.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_cleaner {

namespace {
constexpr unsigned int kInitialRegistryValueSize = 256;
}  // namespace

void ExpectRegistryFootprint(const PUPData::PUP& pup,
                             const RegKeyPath& key_path,
                             const wchar_t* value_name,
                             const wchar_t* value_substring,
                             RegistryMatchRule rule) {
  DCHECK(value_name);
  DCHECK(value_substring);
  for (const auto& footprint : pup.expanded_registry_footprints) {
    bool same_path = footprint.key_path == key_path ||
                     footprint.key_path.IsEquivalent(key_path);
    if (same_path && footprint.value_name == value_name &&
        footprint.value_substring == value_substring &&
        footprint.rule == rule) {
      return;
    }
  }
  ADD_FAILURE() << "Expected registry not found: " << key_path.FullPath()
                << "' value_name='" << value_name << "' value_substring='"
                << value_substring << "' rule=" << rule;
}

void ExpectRegistryFootprintAbsent(const PUPData::PUP& pup,
                                   const RegKeyPath& key_path,
                                   const wchar_t* value_name,
                                   const wchar_t* value_substring,
                                   RegistryMatchRule rule) {
  DCHECK(value_name);
  DCHECK(value_substring);
  for (const auto& footprint : pup.expanded_registry_footprints) {
    if (footprint.key_path == key_path && footprint.value_name == value_name &&
        footprint.value_substring == value_substring &&
        footprint.rule == rule) {
      ADD_FAILURE() << "Unexpected registry found: " << key_path.FullPath()
                    << "' value_name='" << value_name << "' value_substring='"
                    << value_substring << "' rule=" << rule;
      return;
    }
  }
}

ScopedRegistryValue::ScopedRegistryValue(HKEY rootkey,
                                         const wchar_t* subkey,
                                         REGSAM access,
                                         const wchar_t* value_name,
                                         const wchar_t* content,
                                         uint32_t value_type)
    : value_name_(value_name) {
  DCHECK(subkey);
  DCHECK(value_name);
  DCHECK(content);

  LONG result = key_.Create(rootkey, subkey, access);
  DCHECK_EQ(result, ERROR_SUCCESS);
  DCHECK(key_.Valid());

  old_value_.resize(kInitialRegistryValueSize);

  // Save the previous registry value content.
  if (key_.HasValue(value_name)) {
    has_value_ = true;
    old_value_size_ = old_value_.size();
    while (true) {
      result = key_.ReadValue(value_name, &old_value_[0], &old_value_size_,
                              &old_value_type_);
      if (result == ERROR_SUCCESS)
        break;
      CHECK_EQ(result, ERROR_MORE_DATA);
      DCHECK_GT(old_value_size_, old_value_.size());
      old_value_.resize(old_value_size_);
    }
  }

  // Store the new registry value content.
  result = key_.WriteValue(value_name, content,
                           ::wcslen(content) * sizeof(wchar_t), value_type);
  DCHECK_EQ(result, ERROR_SUCCESS);
}

ScopedRegistryValue::~ScopedRegistryValue() {
  DCHECK(key_.Valid());
  if (has_value_) {
    LONG result = key_.WriteValue(value_name_, &old_value_[0], old_value_size_,
                                  old_value_type_);
    DCHECK_EQ(result, ERROR_SUCCESS);
  } else {
    if (key_.HasValue(value_name_)) {
      LONG result = key_.DeleteValue(value_name_);
      DCHECK_EQ(result, ERROR_SUCCESS);
    }
  }
}

ScopedTempRegistryKey::ScopedTempRegistryKey(HKEY key,
                                             const wchar_t* key_path,
                                             REGSAM access) {
  DCHECK(key_path);
  key_.Create(key, key_path, access);
}

ScopedTempRegistryKey::~ScopedTempRegistryKey() {
  if (Valid())
    key_.DeleteKey(L"");
}

}  // namespace chrome_cleaner
