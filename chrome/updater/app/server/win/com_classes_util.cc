// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/app/server/win/com_classes_util.h"

#include <optional>
#include <string>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/types/expected_macros.h"
#include "base/version.h"
#include "base/win/windows_types.h"
#include "chrome/updater/registration_data.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/win_util.h"

namespace updater {

namespace {

// Maximum string length for COM strings.
constexpr size_t kMaxStringLen = 0x4000;  // 16KB.

// Maximum string length for `language` strings.
constexpr size_t kMaxLanguageStringLen = 10;

}  // namespace

HRESULT IsCOMCallerAllowed() {
  if (!IsSystemInstall()) {
    return S_OK;
  }

  ASSIGN_OR_RETURN(const bool result, IsCOMCallerAdmin(), [](HRESULT error) {
    LOG(ERROR) << "IsCOMCallerAdmin failed: " << std::hex << error;
    return error;
  });

  return result ? S_OK : E_ACCESSDENIED;
}

std::optional<std::string> ValidateStringEmptyNotOk(const wchar_t* value,
                                                    size_t max_length) {
  std::string value_s;
  return value && base::WideToUTF8(value, wcslen(value), &value_s) &&
                 !value_s.empty() && (value_s.length() <= max_length)
             ? std::make_optional(value_s)
             : std::nullopt;
}

std::optional<std::string> ValidateStringEmptyOk(const wchar_t* value,
                                                 size_t max_length) {
  std::string value_s;
  return !value ? std::make_optional(value_s)
         : base::WideToUTF8(value, wcslen(value), &value_s) &&
                 (value_s.length() <= max_length)
             ? std::make_optional(value_s)
             : std::nullopt;
}

std::optional<std::string> ValidateAppId(const wchar_t* app_id) {
  return ValidateStringEmptyNotOk(app_id, kMaxStringLen);
}

std::optional<std::string> ValidateCommandId(const wchar_t* command_id) {
  return ValidateStringEmptyNotOk(command_id, kMaxStringLen);
}

std::optional<std::string> ValidateBrandCode(const wchar_t* brand_code) {
  return ValidateStringEmptyOk(brand_code, kMaxStringLen);
}

std::optional<base::FilePath> ValidateBrandPath(const wchar_t* brand_path) {
  const std::optional<std::string> brand_path_s =
      ValidateStringEmptyOk(brand_path, kMaxStringLen);
  return brand_path_s ? std::make_optional(
                            base::FilePath(base::UTF8ToWide(*brand_path_s)))
                      : std::nullopt;
}

std::optional<std::string> ValidateAP(const wchar_t* ap) {
  return ValidateStringEmptyOk(ap, kMaxStringLen);
}

std::optional<base::Version> ValidateVersion(const wchar_t* version) {
  const std::optional<std::string> version_s =
      ValidateStringEmptyNotOk(version, kMaxStringLen);
  if (!version_s) {
    return std::nullopt;
  }
  const base::Version version_v = base::Version(*version_s);
  return version_v.IsValid() ? std::make_optional(version_v) : std::nullopt;
}

std::optional<base::FilePath> ValidateExistenceCheckerPath(
    const wchar_t* existence_checker_path) {
  const std::optional<std::string> existence_checker_path_s =
      ValidateStringEmptyOk(existence_checker_path, kMaxStringLen);
  return existence_checker_path_s
             ? std::make_optional(
                   base::FilePath(base::UTF8ToWide(*existence_checker_path_s)))
             : std::nullopt;
}

std::optional<base::FilePath> ValidateInstallerPath(
    const wchar_t* installer_path) {
  const std::optional<std::string> installer_path_s =
      ValidateStringEmptyNotOk(installer_path, kMaxStringLen);
  return installer_path_s ? std::make_optional(base::FilePath(
                                base::UTF8ToWide(*installer_path_s)))
                          : std::nullopt;
}

std::optional<std::string> ValidateInstallArgs(const wchar_t* install_args) {
  return ValidateStringEmptyOk(install_args, kMaxStringLen);
}

std::optional<std::string> ValidateInstallSettings(
    const wchar_t* install_settings) {
  return ValidateStringEmptyOk(install_settings, kMaxStringLen);
}

std::optional<std::string> ValidateClientInstallData(
    const wchar_t* client_install_data) {
  return ValidateStringEmptyOk(client_install_data, kMaxStringLen);
}

std::optional<std::string> ValidateInstallDataIndex(
    const wchar_t* install_data_index) {
  return ValidateStringEmptyOk(install_data_index, kMaxStringLen);
}

std::optional<std::string> ValidateInstallId(const wchar_t* install_id) {
  return ValidateStringEmptyOk(install_id, kMaxStringLen);
}

std::optional<std::string> ValidateLanguage(const wchar_t* language) {
  return ValidateStringEmptyOk(language, kMaxLanguageStringLen);
}

std::optional<RegistrationRequest> ValidateRegistrationRequest(
    const wchar_t* app_id,
    const wchar_t* brand_code,
    const wchar_t* brand_path,
    const wchar_t* ap,
    const wchar_t* version,
    const wchar_t* existence_checker_path,
    const wchar_t* install_id) {
  const std::optional<std::string> app_id_validated = ValidateAppId(app_id);
  if (!app_id_validated) {
    return {};
  }
  const std::optional<std::string> brand_code_validated =
      ValidateBrandCode(brand_code);
  if (!brand_code_validated) {
    return {};
  }
  const std::optional<base::FilePath> brand_path_validated =
      ValidateBrandPath(brand_path);
  if (!brand_path_validated) {
    return {};
  }
  const std::optional<std::string> ap_validated = ValidateAP(ap);
  if (!ap_validated) {
    return {};
  }
  const std::optional<base::Version> version_validated =
      ValidateVersion(version);
  if (!version_validated) {
    return {};
  }
  const std::optional<base::FilePath> existence_checker_path_validated =
      ValidateExistenceCheckerPath(existence_checker_path);
  if (!existence_checker_path_validated) {
    return {};
  }
  const std::optional<std::string> install_id_validated =
      ValidateInstallId(install_id);
  if (!install_id_validated) {
    return {};
  }

  RegistrationRequest request;
  request.app_id = *app_id_validated;
  request.brand_code = *brand_code_validated;
  request.brand_path = *brand_path_validated;
  request.ap = *ap_validated;
  request.version = *version_validated;
  request.existence_checker_path = *existence_checker_path_validated;
  request.install_id = *install_id_validated;
  return request;
}

}  // namespace updater
