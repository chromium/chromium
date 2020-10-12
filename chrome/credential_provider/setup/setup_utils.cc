// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/setup/setup_utils.h"

#include <Windows.h>

#include <string>

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

const char kStandaloneInstall[] = "standalone";
}  // namespace switches

StandaloneInstallerConfigurator::StandaloneInstallerConfigurator()
    : is_standalone_installation_(false) {}

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

// Sets the installer source for GCPW. When installed through standalone
// installer, |kStandaloneInstall| switch is present in the commandline
// arguments.
void StandaloneInstallerConfigurator::ConfigureInstallationType(
    const base::CommandLine& cmdline) {
  base::string16 standalone_install16 =
      base::UTF8ToUTF16(switches::kStandaloneInstall);
  if (cmdline.HasSwitch(switches::kStandaloneInstall)) {
    is_standalone_installation_ = true;
    HRESULT hr = SetUpdaterClientsAppPathFlag(standalone_install16, 1);
    if (FAILED(hr))
      LOGFN(ERROR) << "SetGlobalFlag failed" << putHR(hr);
  } else if (GetUpdaterClientsAppPathFlagOrDefault(standalone_install16, 0)) {
    is_standalone_installation_ = true;
  }
}

base::string16 StandaloneInstallerConfigurator::GetCurrentDate() {
  static const wchar_t kDateFormat[] = L"yyyyMMdd";
  wchar_t date_str[base::size(kDateFormat)] = {0};
  int len = GetDateFormatW(LOCALE_INVARIANT, 0, nullptr, kDateFormat, date_str,
                           base::size(date_str));
  if (len) {
    --len;  // Subtract terminating \0.
  } else {
    LOGFN(ERROR) << "GetDateFormat failed";
    return L"";
  }

  return base::string16(date_str, len);
}

bool StandaloneInstallerConfigurator::IsStandaloneInstallation() const {
  return is_standalone_installation_;
}

HRESULT StandaloneInstallerConfigurator::AddUninstallKey(
    const base::FilePath& install_path) {
  LOGFN(VERBOSE);

  if (!is_standalone_installation_)
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
                          base::ASCIIToUTF16(version.GetString()).c_str());
  if (status != ERROR_SUCCESS) {
    HRESULT hr = HRESULT_FROM_WIN32(status);
    LOGFN(ERROR) << "Unable to write " << kRegVersion << " hr=" << putHR(hr);
    return hr;
  }

  status = key.WriteValue(kRegDisplayVersion,
                          base::ASCIIToUTF16(version.GetString()).c_str());
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

}  // namespace credential_provider
