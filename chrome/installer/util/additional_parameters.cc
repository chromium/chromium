// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/additional_parameters.h"

#include <windows.h>

#include "base/check.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/win/registry.h"
#include "chrome/install_static/install_details.h"

namespace installer {

namespace {

constexpr wchar_t kRegValueAp[] = L"ap";
constexpr base::WStringPiece kFullSuffix = L"-full";

// Returns null if the value was not found or otherwise could not be read.
absl::optional<std::wstring> ReadAdditionalParameters() {
  absl::optional<std::wstring> result;
  base::win::RegKey key;

  if (key.Open(install_static::IsSystemInstall() ? HKEY_LOCAL_MACHINE
                                                 : HKEY_CURRENT_USER,
               install_static::GetClientStateKeyPath().c_str(),
               KEY_WOW64_32KEY | KEY_QUERY_VALUE) == ERROR_SUCCESS) {
    result.emplace();
    if (key.ReadValue(kRegValueAp, &result.value()) != ERROR_SUCCESS)
      result.reset();
  }
  return result;
}

// Writes `value` to the "ap" value in the registry, or deletes the "ap" value
// if `value` is null. Returns false and sets the Windows last-error code on
// failure; otherwise, returns true.
bool WriteAdditionalParameters(const absl::optional<std::wstring>& value) {
  base::win::RegKey key;
  LONG result = ERROR_SUCCESS;

  if (!value) {
    // Delete the value if it exists.
    result = key.Open(install_static::IsSystemInstall() ? HKEY_LOCAL_MACHINE
                                                        : HKEY_CURRENT_USER,
                      install_static::GetClientStateKeyPath().c_str(),
                      KEY_WOW64_32KEY | KEY_SET_VALUE);
    if (result == ERROR_SUCCESS)
      result = key.DeleteValue(kRegValueAp);
    // Report success if the value was deleted or if either it or the key didn't
    // exist to start with.
    if (result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND ||
        result == ERROR_PATH_NOT_FOUND) {
      return true;
    }
    ::SetLastError(result);
    return false;
  }

  // Write the value to the key.
  result = key.Create(install_static::IsSystemInstall() ? HKEY_LOCAL_MACHINE
                                                        : HKEY_CURRENT_USER,
                      install_static::GetClientStateKeyPath().c_str(),
                      KEY_WOW64_32KEY | KEY_SET_VALUE);
  if (result == ERROR_SUCCESS)
    result = key.WriteValue(kRegValueAp, value->c_str());
  if (result == ERROR_SUCCESS)
    return true;
  ::SetLastError(result);
  return false;
}

bool HasFullSuffix(const absl::optional<std::wstring>& value) {
  return value ? base::EndsWith(*value, kFullSuffix) : false;
}

}  // namespace

AdditionalParameters::AdditionalParameters()
    : value_(ReadAdditionalParameters()) {}

AdditionalParameters::~AdditionalParameters() = default;

const wchar_t* AdditionalParameters::value() const {
  return value_ ? value_->c_str() : L"";
}

wchar_t AdditionalParameters::GetStatsDefault() const {
  if (!value_)
    return 0;

  static constexpr base::WStringPiece kStatsdef = L"-statsdef_";
  base::WStringPiece value(*value_);
  auto pos = value.find(kStatsdef);
  if (pos == base::WStringPiece::npos)
    return 0;
  pos += kStatsdef.size();
  return pos < value.size() ? value[pos] : 0;
}

bool AdditionalParameters::SetFullSuffix(bool set_full_suffix) {
  if (HasFullSuffix(value_) == set_full_suffix)
    return false;  // Nothing to do.
  if (set_full_suffix) {
    if (!value_) {
      value_ = std::wstring(kFullSuffix);
    } else {
      value_->append(kFullSuffix.data(), kFullSuffix.size());
    }
  } else {
    DCHECK(value_);
    const auto value_size = value_->size();
    if (value_size == kFullSuffix.size()) {
      value_.reset();
    } else {
      value_->resize(value_size - kFullSuffix.size());
    }
  }
  return true;
}

bool AdditionalParameters::Commit() {
  return WriteAdditionalParameters(value_);
}

}  // namespace installer
