// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/per_install_values.h"

#include <optional>
#include <string>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "base/win/registry.h"
#include "base/win/windows_types.h"
#include "build/branding_buildflags.h"
#include "chrome/install_static/install_details.h"
#include "chrome/install_static/install_util.h"

namespace installer {

PerInstallValue::PerInstallValue(std::wstring_view name)
#if BUILDFLAG(USE_GOOGLE_UPDATE_INTEGRATION)
    : root_(install_static::IsSystemInstall() ? HKEY_LOCAL_MACHINE
                                              : HKEY_CURRENT_USER),
#else
    : root_(HKEY_CURRENT_USER),
#endif
      key_path_((install_static::IsSystemInstall()
                     ? install_static::GetClientStateMediumKeyPath()
                     : install_static::GetClientStateKeyPath())
                    .append(L"\\PerInstallValues")),
      value_name_(name) {
}

PerInstallValue::~PerInstallValue() = default;

void PerInstallValue::Set(const base::Value& value) {
  base::win::RegKey key;
  if (key.Create(root_, key_path_.c_str(), KEY_WOW64_32KEY | KEY_SET_VALUE) !=
      ERROR_SUCCESS) {
    return;
  }

  std::string value_string;
  if (!base::JSONWriter::Write(value, &value_string)) {
    return;
  }

  key.WriteValue(value_name_.c_str(), base::UTF8ToWide(value_string).c_str());
}

std::optional<base::Value> PerInstallValue::Get() {
  std::wstring value_string;
  if (base::win::RegKey(root_, key_path_.c_str(),
                        KEY_WOW64_32KEY | KEY_QUERY_VALUE)
          .ReadValue(value_name_.c_str(), &value_string) != ERROR_SUCCESS) {
    return {};
  }

  return base::JSONReader::Read(base::WideToUTF8(value_string));
}

void PerInstallValue::Delete() {
  base::win::RegKey key;
  if (key.Open(root_, key_path_.c_str(),
               KEY_WOW64_32KEY | KEY_QUERY_VALUE | KEY_SET_VALUE) !=
      ERROR_SUCCESS) {
    return;
  }

  key.DeleteValue(value_name_.c_str());
  if (!key.GetValueCount().value_or(1)) {
    key.DeleteKey(L"");
  }
}

}  // namespace installer
