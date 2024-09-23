// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/common/platform_utils.h"

#include <windows.h>

#include <optional>

#include "base/base_paths_win.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/process/process.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"
#include "base/win/win_util.h"
#include "components/device_signals/core/common/common_types.h"
#include "components/device_signals/core/common/signals_constants.h"

namespace device_signals {

namespace {

constexpr wchar_t kCSAgentRegPath[] =
    L"SYSTEM\\CurrentControlSet\\services\\CSAgent\\Sim";

// CU is the registry value containing the customer ID.
constexpr wchar_t kCSCURegKey[] = L"CU";

// AG is the registry value containing the agent ID.
constexpr wchar_t kCSAGRegKey[] = L"AG";

std::optional<std::string> GetHexStringRegValue(
    const base::win::RegKey& key,
    const std::wstring& reg_key_name) {
  DWORD type = REG_NONE;
  DWORD size = 0;
  auto res = key.ReadValue(reg_key_name.c_str(), nullptr, &size, &type);
  if (res == ERROR_SUCCESS && type == REG_BINARY) {
    std::vector<uint8_t> raw_bytes(size);
    res = key.ReadValue(reg_key_name.c_str(), raw_bytes.data(), &size, &type);

    if (res == ERROR_SUCCESS) {
      // Converting the values to lowercase specifically for CrowdStrike as
      // some of their APIs only accept the lowercase version.
      return base::ToLowerASCII(base::HexEncode(raw_bytes));
    }
  }

  return std::nullopt;
}

}  // namespace

bool ResolvePath(const base::FilePath& file_path,
                 base::FilePath* resolved_file_path) {
  auto expanded_path_wstring =
      base::win::ExpandEnvironmentVariables(file_path.value());
  if (!expanded_path_wstring) {
    return false;
  }

  auto expanded_file_path = base::FilePath(expanded_path_wstring.value());
  if (!base::PathExists(expanded_file_path)) {
    return false;
  }
  *resolved_file_path = base::MakeAbsoluteFilePath(expanded_file_path);
  return true;
}

std::optional<base::FilePath> GetProcessExePath(base::ProcessId pid) {
  base::Process process(
      ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid));
  if (!process.IsValid()) {
    return std::nullopt;
  }

  DWORD path_len = MAX_PATH;
  wchar_t path_string[MAX_PATH];
  if (!::QueryFullProcessImageName(process.Handle(), 0, path_string,
                                   &path_len)) {
    return std::nullopt;
  }

  return base::FilePath(path_string);
}

std::optional<CrowdStrikeSignals> GetCrowdStrikeSignals() {
  base::win::RegKey key;
  auto result = key.Open(HKEY_LOCAL_MACHINE, kCSAgentRegPath,
                         KEY_QUERY_VALUE | KEY_WOW64_64KEY);

  if (result == ERROR_SUCCESS && key.Valid()) {
    base::Value::Dict crowdstrike_info;

    auto customer_id = GetHexStringRegValue(key, kCSCURegKey);
    if (customer_id) {
      crowdstrike_info.Set(names::kCustomerId, customer_id.value());
    }

    auto agent_id = GetHexStringRegValue(key, kCSAGRegKey);
    if (agent_id) {
      crowdstrike_info.Set(names::kAgentId, agent_id.value());
    }

    if (customer_id || agent_id) {
      return CrowdStrikeSignals{customer_id.value_or(std::string()),
                                agent_id.value_or(std::string())};
    }
  }

  return std::nullopt;
}

base::FilePath GetCrowdStrikeZtaFilePath() {
  static constexpr base::FilePath::CharType kZtaFilePathSuffix[] =
      FILE_PATH_LITERAL("CrowdStrike\\ZeroTrustAssessment\\data.zta");

  base::FilePath app_data_dir;
  if (!base::PathService::Get(base::DIR_COMMON_APP_DATA, &app_data_dir)) {
    // Returning the empty path when failing.
    return app_data_dir;
  }
  return app_data_dir.Append(kZtaFilePathSuffix);
}

}  // namespace device_signals
