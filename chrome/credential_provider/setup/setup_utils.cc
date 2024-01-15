// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/setup/setup_utils.h"

#include <Windows.h>

#include <string>

#include "base/files/file_util.h"
#include "base/json/json_string_value_serializer.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"
#include "chrome/common/chrome_version.h"
#include "chrome/credential_provider/common/gcp_strings.h"
#include "chrome/credential_provider/gaiacp/gaia_resources.h"
#include "chrome/credential_provider/gaiacp/gcp_utils.h"
#include "chrome/credential_provider/gaiacp/logging.h"
#include "chrome/credential_provider/gaiacp/reg_utils.h"
#include "chrome/credential_provider/setup/gcpw_files.h"

namespace credential_provider {

namespace switches {

const char kParentHandle[] = "parent-handle";
const char kInstallPath[] = "install-path";
const char kUninstall[] = "uninstall";

const char kEnableStats[] = "enable-stats";
const char kDisableStats[] = "disable-stats";

const char kInstallerData[] = "installerdata";

const char kStandaloneInstall[] = "standalone";
}  // namespace switches

namespace {

// Path to the msi json value inside the dictionary which is parsed from
// installer data argument in the command line.
const char kMsiJsonPath[] = "distribution.msi";

// The registry name which is saved to indicate installation source.
const wchar_t kMsiInstall[] = L"msi";

// Parses the json data and returns it as a dictionary. If the json data isn't
// valid, returns std::nullopt.
std::optional<base::Value::Dict> ParseDistributionPreferences(
    const std::string& json_data) {
  JSONStringValueDeserializer json(json_data);
  std::string error;
  std::unique_ptr<base::Value> root(json.Deserialize(nullptr, &error));
  if (!root.get()) {
    LOGFN(WARNING) << "Failed to parse initial prefs file: " << error;
    return std::nullopt;
  }
  if (!root->is_dict()) {
    LOGFN(WARNING) << "Failed to parse installer data file";
    return std::nullopt;
  }
  return std::move(*root).TakeDict();
}

}  // namespace

StandaloneInstallerConfigurator::StandaloneInstallerConfigurator()
    : is_msi_installation_(false) {}

StandaloneInstallerConfigurator::~StandaloneInstallerConfigurator() {}

// static
StandaloneInstallerConfigurator**
StandaloneInstallerConfigurator::GetInstanceStorage() {
  static StandaloneInstallerConfigurator* instance =
      new StandaloneInstallerConfigurator();

  return &instance;
}

// static
StandaloneInstallerConfigurator* StandaloneInstallerConfigurator::Get() {
  return *GetInstanceStorage();
}

// Sets the installer source for GCPW. When installed through MSI,
// contains installer data file name as argument.
void StandaloneInstallerConfigurator::ConfigureInstallationType(
    const base::CommandLine& cmdline) {
  // There are following scenarios for installations:
  // First time install from MSI
  // First time install from EXE
  // MSIs before this kMsiInstall registry gets auto-updated
  // MSIs with kMsiInstall registry gets auto-updated
  // EXEs with kMsiInstall registry gets auto-updated

  // |kStandaloneInstall| indicates fresh installation.
  if (cmdline.HasSwitch(switches::kStandaloneInstall)) {
    base::Value* is_msi = nullptr;
    if (cmdline.HasSwitch(switches::kInstallerData)) {
      base::FilePath prefs_path(
          cmdline.GetSwitchValuePath(switches::kInstallerData));

      if (InitializeFromInstallerData(prefs_path))
        is_msi = installer_data_dictionary_.FindByDottedPath(kMsiJsonPath);
    }

    is_msi_installation_ = false;
    if (is_msi && is_msi->is_bool() && is_msi->GetBool()) {
      is_msi_installation_ = true;
    }
  } else {
    // Honor the registry if it is found, otherwise fall back to MSI
    // installation.
    is_msi_installation_ =
        GetUpdaterClientsAppPathFlagOrDefault(kMsiInstall, 1);
  }

  HRESULT hr =
      SetUpdaterClientsAppPathFlag(kMsiInstall, is_msi_installation_ ? 1 : 0);
  if (FAILED(hr))
    LOGFN(ERROR) << "SetGlobalFlag failed" << putHR(hr);
}

std::wstring StandaloneInstallerConfigurator::GetCurrentDate() {
  static const wchar_t kDateFormat[] = L"yyyyMMdd";
  wchar_t date_str[std::size(kDateFormat)] = {0};
  int len = GetDateFormatW(LOCALE_INVARIANT, 0, nullptr, kDateFormat, date_str,
                           std::size(date_str));
  if (len) {
    --len;  // Subtract terminating \0.
  } else {
    LOGFN(ERROR) << "GetDateFormat failed";
    return L"";
  }

  return std::wstring(date_str, len);
}

bool StandaloneInstallerConfigurator::IsStandaloneInstallation() const {
  return !is_msi_installation_;
}

HRESULT StandaloneInstallerConfigurator::AddUninstallKey(
    const base::FilePath& install_path) {
  LOGFN(VERBOSE);

  if (is_msi_installation_)
    return S_OK;

  std::wstring uninstall_reg = kRegUninstall;
  uninstall_reg.append(L"\\");
  uninstall_reg.append(kRegUninstallProduct);

  base::win::RegKey key;
  LONG status =
      key.Create(HKEY_LOCAL_MACHINE, uninstall_reg.c_str(), KEY_SET_VALUE);
  if (status != ERROR_SUCCESS) {
    HRESULT hr = HRESULT_FROM_WIN32(status);
    LOGFN(ERROR) << "Unable to create " << uninstall_reg << " hr=" << putHR(hr);
    return hr;
  }

  base::CommandLine uninstall_string(
      install_path.Append(kCredentialProviderSetupExe));
  uninstall_string.AppendSwitch(switches::kUninstall);

  status = key.WriteValue(kRegUninstallString,
                          uninstall_string.GetCommandLineString().c_str());
  if (status != ERROR_SUCCESS) {
    HRESULT hr = HRESULT_FROM_WIN32(status);
    LOGFN(ERROR) << "Unable to write " << kRegUninstallString
                 << " hr=" << putHR(hr);
    return hr;
  }

  status = key.WriteValue(kRegUninstallDisplayName,
                          GetStringResource(IDS_PROJNAME_BASE).c_str());
  if (status != ERROR_SUCCESS) {
    HRESULT hr = HRESULT_FROM_WIN32(status);
    LOGFN(ERROR) << "Unable to write " << kRegUninstallDisplayName
                 << " hr=" << putHR(hr);
    return hr;
  }

  status = key.WriteValue(kRegInstallLocation, install_path.value().c_str());
  if (status != ERROR_SUCCESS) {
    HRESULT hr = HRESULT_FROM_WIN32(status);
    LOGFN(ERROR) << "Unable to write " << kRegInstallLocation
                 << " hr=" << putHR(hr);
    return hr;
  }

  status = key.WriteValue(
      kRegDisplayIcon,
      (install_path.Append(kCredentialProviderSetupExe).value() + L",0")
          .c_str());
  if (status != ERROR_SUCCESS) {
    HRESULT hr = HRESULT_FROM_WIN32(status);
    LOGFN(ERROR) << "Unable to write " << kRegDisplayIcon
                 << " hr=" << putHR(hr);
    return hr;
  }

  status = key.WriteValue(kRegNoModify, 1);
  if (status != ERROR_SUCCESS) {
    HRESULT hr = HRESULT_FROM_WIN32(status);
    LOGFN(ERROR) << "Unable to write " << kRegNoModify << " hr=" << putHR(hr);
    return hr;
  }

  status = key.WriteValue(kRegNoRepair, 1);
  if (status != ERROR_SUCCESS) {
    HRESULT hr = HRESULT_FROM_WIN32(status);
    LOGFN(ERROR) << "Unable to write " << kRegNoRepair << " hr=" << putHR(hr);
    return hr;
  }

  status = key.WriteValue(kRegPublisherName, kRegPublisher);
  if (status != ERROR_SUCCESS) {
    HRESULT hr = HRESULT_FROM_WIN32(status);
    LOGFN(ERROR) << "Unable to write " << kRegPublisherName
                 << " hr=" << putHR(hr);
    return hr;
  }

  status = key.WriteValue(kRegInstallDate, GetCurrentDate().c_str());
  if (status != ERROR_SUCCESS) {
    HRESULT hr = HRESULT_FROM_WIN32(status);
    LOGFN(ERROR) << "Unable to write " << kRegInstallDate
                 << " hr=" << putHR(hr);
    return hr;
  }

  base::Version version(CHROME_VERSION_STRING);

  status = key.WriteValue(kRegVersion,
                          base::ASCIIToWide(version.GetString()).c_str());
  if (status != ERROR_SUCCESS) {
    HRESULT hr = HRESULT_FROM_WIN32(status);
    LOGFN(ERROR) << "Unable to write " << kRegVersion << " hr=" << putHR(hr);
    return hr;
  }

  status = key.WriteValue(kRegDisplayVersion,
                          base::ASCIIToWide(version.GetString()).c_str());
  if (status != ERROR_SUCCESS) {
    HRESULT hr = HRESULT_FROM_WIN32(status);
    LOGFN(ERROR) << "Unable to write " << kRegDisplayVersion
                 << " hr=" << putHR(hr);
    return hr;
  }

  const std::vector<uint32_t>& version_components = version.components();
  if (version_components.size() == 4) {
    status = key.WriteValue(kRegVersionMajor,
                            static_cast<DWORD>(version_components[2]));
    if (status != ERROR_SUCCESS) {
      HRESULT hr = HRESULT_FROM_WIN32(status);
      LOGFN(ERROR) << "Unable to write " << kRegVersionMajor
                   << " hr=" << putHR(hr);
      return hr;
    }

    status = key.WriteValue(kRegVersionMinor,
                            static_cast<DWORD>(version_components[3]));
    if (status != ERROR_SUCCESS) {
      HRESULT hr = HRESULT_FROM_WIN32(status);
      LOGFN(ERROR) << "Unable to write " << kRegVersionMinor
                   << " hr=" << putHR(hr);
      return hr;
    }
  }

  return S_OK;
}

HRESULT StandaloneInstallerConfigurator::RemoveUninstallKey() {
  LOGFN(VERBOSE);
  base::win::RegKey key;

  LONG status = key.Create(HKEY_LOCAL_MACHINE, kRegUninstall, KEY_SET_VALUE);
  if (status != ERROR_SUCCESS) {
    HRESULT hr = HRESULT_FROM_WIN32(status);
    LOGFN(ERROR) << "Unable to create " << kRegUninstall << " hr=" << putHR(hr);
    return hr;
  }

  status = key.DeleteKey(kRegUninstallProduct);
  if (status != ERROR_SUCCESS) {
    HRESULT hr = HRESULT_FROM_WIN32(status);
    LOGFN(ERROR) << "Unable to delete " << kRegUninstallProduct
                 << " hr=" << putHR(hr);
    return hr;
  }
  return S_OK;
}

bool StandaloneInstallerConfigurator::InitializeFromInstallerData(
    base::FilePath prefs_path) {
  std::string json_data;
  if (base::PathExists(prefs_path) &&
      !base::ReadFileToString(prefs_path, &json_data)) {
    LOGFN(ERROR) << "Failed to read preferences from " << prefs_path.value();
    return false;
  }

  if (json_data.empty()) {
    LOGFN(WARNING) << "Installer data is empty!";
    return false;
  }

  std::optional<base::Value::Dict> prefs =
      ParseDistributionPreferences(json_data);
  if (!prefs) {
    LOGFN(WARNING) << "Installer data isn't formatted correctly";
    return false;
  }

  installer_data_dictionary_ = std::move(prefs).value();

  return true;
}

}  // namespace credential_provider
