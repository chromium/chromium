// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_APP_SERVER_WIN_COM_CLASSES_UTIL_H_
#define CHROME_UPDATER_APP_SERVER_WIN_COM_CLASSES_UTIL_H_

#include <optional>
#include <string>

#include "base/win/windows_types.h"

namespace base {

class FilePath;
class Version;

}  // namespace base

namespace updater {

struct RegistrationRequest;

// Returns S_OK if user install, or if the COM caller is admin. Error otherwise.
HRESULT IsCOMCallerAllowed();

std::optional<std::string> ValidateStringEmptyNotOk(const wchar_t* value,
                                                    size_t max_length);
std::optional<std::string> ValidateStringEmptyOk(const wchar_t* value,
                                                 size_t max_length);
std::optional<std::string> ValidateAppId(const wchar_t* app_id);
std::optional<std::string> ValidateCommandId(const wchar_t* command_id);
std::optional<std::string> ValidateBrandCode(const wchar_t* brand_code);
std::optional<base::FilePath> ValidateBrandPath(const wchar_t* brand_path);
std::optional<std::string> ValidateAP(const wchar_t* ap);
std::optional<base::Version> ValidateVersion(const wchar_t* version);
std::optional<base::FilePath> ValidateExistenceCheckerPath(
    const wchar_t* existence_checker_path);
std::optional<base::FilePath> ValidateInstallerPath(
    const wchar_t* installer_path);
std::optional<std::string> ValidateInstallArgs(const wchar_t* install_args);
std::optional<std::string> ValidateInstallSettings(
    const wchar_t* install_settings);
std::optional<std::string> ValidateClientInstallData(
    const wchar_t* client_install_data);
std::optional<std::string> ValidateInstallDataIndex(
    const wchar_t* install_data_index);
std::optional<std::string> ValidateInstallId(const wchar_t* install_id);
std::optional<std::string> ValidateLanguage(const wchar_t* language);
std::optional<RegistrationRequest> ValidateRegistrationRequest(
    const wchar_t* app_id,
    const wchar_t* brand_code,
    const wchar_t* brand_path,
    const wchar_t* ap,
    const wchar_t* version,
    const wchar_t* existence_checker_path,
    const wchar_t* install_id);

}  // namespace updater

#endif  // CHROME_UPDATER_APP_SERVER_WIN_COM_CLASSES_UTIL_H_
