// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/browser/win/registry_settings_client.h"

#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/win/registry.h"
#include "base/win/shlwapi.h"

namespace {

// Returns corresponding HKEY according to the RegistryHive, return nullopt if
// hive is missing or unsupported.
std::optional<HKEY> ConvertHiveToHKey(
    const std::optional<device_signals::RegistryHive> hive) {
  if (hive == std::nullopt) {
    return std::nullopt;
  }

  switch (hive.value()) {
    case device_signals::RegistryHive::kHkeyClassesRoot:
      return HKEY_CLASSES_ROOT;
    case device_signals::RegistryHive::kHkeyLocalMachine:
      return HKEY_LOCAL_MACHINE;
    case device_signals::RegistryHive::kHkeyCurrentUser:
      return HKEY_CURRENT_USER;
  }
}

std::vector<device_signals::SettingsItem> GetSettingsItems(
    const std::vector<device_signals::GetSettingsOptions>& options) {
  std::vector<device_signals::SettingsItem> collected_items;

  for (const auto& option : options) {
    device_signals::SettingsItem collected_item;
    collected_item.path = option.path;
    collected_item.key = option.key;
    collected_item.hive = option.hive;

    std::optional<HKEY> hive_hkey = ConvertHiveToHKey(option.hive);
    if (hive_hkey == std::nullopt) {
      collected_item.presence = device_signals::PresenceValue::kNotFound;
      collected_items.push_back(collected_item);
      continue;
    }
    const std::wstring request_key_wide = base::SysUTF8ToWide(option.key);
    base::win::RegKey registry_settings_key(
        hive_hkey.value(), base::SysUTF8ToWide(option.path).c_str(), KEY_READ);

    DWORD type = REG_SZ;
    DWORD size = 0;
    // Check presence, type and size of registry.
    LONG result = registry_settings_key.ReadValue(request_key_wide.c_str(),
                                                  nullptr, &size, &type);
    if (result == ERROR_ACCESS_DENIED) {
      collected_item.presence = device_signals::PresenceValue::kAccessDenied;
      collected_items.push_back(collected_item);
      continue;
    } else if (result != ERROR_SUCCESS) {
      collected_item.presence = device_signals::PresenceValue::kNotFound;
      collected_items.push_back(collected_item);
      continue;
    } else if (!option.get_value) {
      // Skip registry value collection and just return registry presence.
    } else if (type == REG_DWORD ||
               (type == REG_BINARY && size == sizeof(DWORD))) {
      DWORD out_value_dword = 0;
      if (registry_settings_key.ReadValueDW(
              request_key_wide.c_str(), &out_value_dword) == ERROR_SUCCESS) {
        // DWORD is unsigned long.
        collected_item.setting_json_value =
            base::StringPrintf("%lu", out_value_dword);
      }
    } else if (type == REG_QWORD ||
               (type == REG_BINARY && size == sizeof(int64_t))) {
      int64_t out_value_qword = 0;
      if (registry_settings_key.ReadInt64(request_key_wide.c_str(),
                                          &out_value_qword) == ERROR_SUCCESS) {
        // QWORD is long long.
        collected_item.setting_json_value =
            base::StringPrintf("%lld", out_value_qword);
      }
    } else if (type == REG_SZ) {
      // Handle the REG_SZ type, note this does not include REG_MULTI_SZ or
      // REG_EXPAND_SZ.
      std::wstring out_value_sz;
      std::string out_value_json;
      if (registry_settings_key.ReadValue(request_key_wide.c_str(),
                                          &out_value_sz) == ERROR_SUCCESS) {
        base::JSONWriter::Write(
            base::ValueView(base::SysWideToUTF8(out_value_sz)),
            &out_value_json);
        collected_item.setting_json_value = out_value_json;
      }
    }
    collected_item.presence = device_signals::PresenceValue::kFound;
    collected_items.push_back(collected_item);
  }

  return collected_items;
}

}  // namespace

namespace device_signals {

RegistrySettingsClient::RegistrySettingsClient() = default;

RegistrySettingsClient::~RegistrySettingsClient() = default;

void RegistrySettingsClient::GetSettings(
    const std::vector<GetSettingsOptions>& options,
    GetSettingsSignalsCallback callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&GetSettingsItems, options), std::move(callback));
}
}  // namespace device_signals
