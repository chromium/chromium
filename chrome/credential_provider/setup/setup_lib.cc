// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/credential_provider/setup/setup_lib.h"

#include <shlobj.h>

#include <iomanip>
#include <string>

#include "base/command_line.h"
#include "base/file_version_info.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/scoped_native_library.h"
#include "base/strings/string_number_conversions.h"
#include "base/win/atl.h"
#include "base/win/registry.h"
#include "base/win/scoped_handle.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "chrome/credential_provider/common/gcp_strings.h"
#include "chrome/credential_provider/extension/extension_strings.h"
#include "chrome/credential_provider/extension/extension_utils.h"
#include "chrome/credential_provider/gaiacp/gcp_utils.h"
#include "chrome/credential_provider/gaiacp/logging.h"
#include "chrome/credential_provider/gaiacp/reg_utils.h"
#include "chrome/credential_provider/setup/gcpw_files.h"
#include "chrome/credential_provider/setup/setup_utils.h"
#include "chrome/installer/util/delete_after_reboot_helper.h"

namespace credential_provider {

namespace {

// Creates the directory where GCP is to be installed.
base::FilePath CreateInstallDirectory() {
  base::FilePath dest_path = GetInstallDirectory();

  if (dest_path.empty()) {
    return dest_path;
  }

  if (!base::CreateDirectory(dest_path)) {
    HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "base::CreateDirectory hr=" << putHR(hr);
    return base::FilePath();
  }

  return dest_path;
}

// Copies the files specified in |names| from |src_path| to |dest_path| creating
// any intermedidate subdirectories of |dest_path| as needed.  Both |src_path|
// and |dest_path| are full paths.  |names| are paths relative to |src_path|
// and are copied to the same relative path under |dest_path|.
HRESULT InstallFiles(const base::FilePath& src_path,
                     const base::FilePath& dest_path,
                     const base::FilePath::StringType names[],
                     size_t length) {
  for (size_t i = 0; i < length; ++i) {
    base::FilePath src = src_path.Append(names[i]);
    base::FilePath dest = dest_path.Append(names[i]);

    // Make sure parent of destination file exists.
    if (!base::CreateDirectory(dest.DirName())) {
      HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
      LOGFN(ERROR) << "CreateDirectory hr=" << putHR(hr)
                   << " name=" << names[i];
      return hr;
    }

    if (!base::CopyFile(src, dest)) {
      HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
      LOGFN(ERROR) << "CopyFile hr=" << putHR(hr) << " name=" << names[i];
      return hr;
    }
    LOGFN(INFO) << "Installed name=" << names[i];
  }

  return S_OK;
}

// Registers the named DLLs by calling the DllRegisterServer entrypoint.  The
// DLLs are specified with the |names| argument which are paths relative to
// |dest_path|.  |fakes| is non-null during unit tests to install fakes into
// the loaded DLL.
HRESULT RegisterDlls(const base::FilePath& dest_path,
                     const base::FilePath::StringType names[],
                     size_t length,
                     FakesForTesting* fakes) {
  bool has_failures = false;

  for (size_t i = 0; i < length; ++i) {
    base::ScopedNativeLibrary library(dest_path.Append(names[i]));

    if (fakes) {
      SetFakesForTestingFn set_fakes_for_testing_fn =
          reinterpret_cast<SetFakesForTestingFn>(
              library.GetFunctionPointer("SetFakesForTesting"));
      if (set_fakes_for_testing_fn)
        (*set_fakes_for_testing_fn)(fakes);
    }

    FARPROC register_server_fn = reinterpret_cast<FARPROC>(
        library.GetFunctionPointer("DllRegisterServer"));
    HRESULT hr = S_OK;

    if (register_server_fn) {
      hr = static_cast<HRESULT>((*register_server_fn)());
      LOGFN(VERBOSE) << "Registered name=" << names[i] << " hr=" << putHR(hr);
    } else {
      LOGFN(ERROR) << "Failed to register name=" << names[i];
      hr = E_NOTIMPL;
    }
    has_failures |= FAILED(hr);
  }

  return has_failures ? E_UNEXPECTED : S_OK;
}

// Unregisters the named DLLs by calling the DllUneegisterServer entrypoint.
// The DLLs are specified with the |names| argument which are paths relative to
// |dest_path|.  |fakes| is non-null during unit tests to install fakes into
// the loaded DLL.
HRESULT UnregisterDlls(const base::FilePath& dest_path,
                       const base::FilePath::StringType names[],
                       size_t length,
                       FakesForTesting* fakes) {
  bool has_failures = false;

  for (size_t i = 0; i < length; ++i) {
    base::ScopedNativeLibrary library(dest_path.Append(names[i]));

    if (fakes) {
      SetFakesForTestingFn pmfn = reinterpret_cast<SetFakesForTestingFn>(
          library.GetFunctionPointer("SetFakesForTesting"));
      if (pmfn)
        (*pmfn)(fakes);
    }

    FARPROC pfn = reinterpret_cast<FARPROC>(
        library.GetFunctionPointer("DllUnregisterServer"));
    HRESULT hr = pfn ? static_cast<HRESULT>((*pfn)()) : E_UNEXPECTED;
    LOGFN(VERBOSE) << "Unregistered name=" << names[i] << " hr=" << putHR(hr);
    has_failures |= FAILED(hr);
  }

  return has_failures ? E_UNEXPECTED : S_OK;
}

}  // namespace

HRESULT DoInstall(const base::FilePath& installer_path,
                  const std::wstring& product_version,
                  FakesForTesting* fakes) {
  const base::FilePath gcp_path = CreateInstallDirectory();
  if (gcp_path.empty())
    return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);

  base::FilePath dest_path = gcp_path.Append(product_version);
  LOGFN(VERBOSE) << "Install to: " << dest_path;

  // Make sure nothing under the destination directory is pending delete
  // after reboot, so that files installed now won't get deleted later.
  if (!RemoveFromMovesPendingReboot(dest_path)) {
    HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "RemoveFromMovesPendingReboot hr=" << putHR(hr);
    return hr;
  }

  base::FilePath src_path = installer_path.DirName();
  auto install_files =
      credential_provider::GCPWFiles::Get()->GetEffectiveInstallFiles();
  HRESULT hr = InstallFiles(src_path, dest_path, install_files.data(),
                            install_files.size());
  if (FAILED(hr))
    return hr;

  auto register_dlls =
      credential_provider::GCPWFiles::Get()->GetRegistrationFiles();
  hr = RegisterDlls(dest_path, register_dlls.data(), register_dlls.size(),
                    fakes);
  if (FAILED(hr))
    return hr;

  // If all is good, try to delete all other versions on best effort basis.
  if (SUCCEEDED(hr))
    DeleteVersionsExcept(gcp_path, product_version);

  base::FilePath setup_exe_path = dest_path.Append(kCredentialProviderSetupExe);
  hr = WriteUninstallRegistryValues(setup_exe_path);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "WriteUninstallRegistryValues failed hr=" << putHR(hr);
    // Uninstall registry values are written for MSI wrapper. Failing to write
    // them will only impact uninstalling through uninstall shortcuts on
    // Windows. There is still a workaround to uninstall by calling
    // "gcp_setup.exe --uninstall" from a terminal. So, ignoring the failure in
    // this case until we support rollback of installation that fails mid-way
    // through.
  }

  hr = WriteCredentialProviderRegistryValues(dest_path);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "WriteCredentialProviderRegistryValues failed hr="
                 << putHR(hr);
  }

  if (extension::IsGCPWExtensionEnabled()) {
    DWORD error_code = extension::InstallGCPWExtension(
        dest_path.Append(kCredentialProviderExtensionExe));
    if (error_code != ERROR_SUCCESS) {
      LOGFN(ERROR) << "InstallGCPWExtension failed win32=" << error_code;
      return HRESULT_FROM_WIN32(error_code);
    }
  }

  return S_OK;
}

HRESULT DoUninstall(const base::FilePath& installer_path,
                    const base::FilePath& dest_path,
                    FakesForTesting* fakes) {
  bool has_failures = false;

  auto register_dlls =
      credential_provider::GCPWFiles::Get()->GetRegistrationFiles();
  // Do all actions best effort and keep going.
  has_failures |= FAILED(UnregisterDlls(dest_path, register_dlls.data(),
                                        register_dlls.size(), fakes));

  // If the DLLs are unregistered, Credential Provider will not be loaded by
  // Winlogon. Therefore, it is safe to delete the startup sentinel file at this
  // time.
  if (!has_failures)
    DeleteStartupSentinel();

  has_failures |=
      FAILED(HRESULT_FROM_WIN32(extension::UninstallGCPWExtension()));

  // Delete all files in the destination directory.  This directory does not
  // contain any configuration files or anything else user generated.
  if (!base::DeletePathRecursively(dest_path)) {
    has_failures = true;
    ScheduleDirectoryForDeletion(dest_path);
  }

  // |dest_path| is of the form %ProgramFile%\Google\GCP\VERSION.  Now try to
  // delete the parent directory if possible.
  if (base::IsDirectoryEmpty(dest_path.DirName()))
    has_failures |= !base::DeleteFile(dest_path.DirName());

  StandaloneInstallerConfigurator* installer_config =
      StandaloneInstallerConfigurator::Get();
  if (installer_config->IsStandaloneInstallation()) {
    has_failures |= FAILED(HRESULT_FROM_WIN32(
        StandaloneInstallerConfigurator::Get()->RemoveUninstallKey()));
  }

  // TODO(rogerta): ask user to reboot if anything went wrong during uninstall.

  return has_failures ? E_UNEXPECTED : S_OK;
}

HRESULT RelaunchUninstaller(const base::FilePath& installer_path) {
  base::FilePath temp_path;
  if (!base::CreateNewTempDirectory(FILE_PATH_LITERAL("gcp"), &temp_path)) {
    HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "CreateNewTempDirectory hr=" << putHR(hr);
    return hr;
  }

  base::FilePath new_installer_path =
      temp_path.Append(installer_path.BaseName());

  if (!base::CopyFile(installer_path, new_installer_path)) {
    HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "Could not copy uninstaller hr=" << putHR(hr) << " '"
                 << installer_path << "' --> '" << new_installer_path << "'";
    return hr;
  }

  base::win::ScopedHandle::Handle this_process_handle_handle;
  if (!::DuplicateHandle(GetCurrentProcess(), GetCurrentProcess(),
                         GetCurrentProcess(), &this_process_handle_handle, 0,
                         TRUE,  // Inheritable.
                         DUPLICATE_SAME_ACCESS)) {
    HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "Error duplicating parent process handle.";
    return hr;
  }
  base::win::ScopedHandle this_process_handle(this_process_handle_handle);

  base::CommandLine cmdline(new_installer_path);
  cmdline.AppendSwitch(switches::kUninstall);
  cmdline.AppendSwitchPath(switches::kInstallPath, installer_path.DirName());
  cmdline.AppendSwitchNative(switches::kParentHandle,
                             base::NumberToWString(base::win::HandleToUint32(
                                 this_process_handle_handle)));

  LOGFN(VERBOSE) << "Cmd: " << cmdline.GetCommandLineString();

  base::LaunchOptions options;
  options.handles_to_inherit.push_back(this_process_handle_handle);
  options.current_directory = temp_path;
  base::Process process(base::LaunchProcess(cmdline, options));

  return process.IsValid() ? S_OK : E_FAIL;
}

int EnableStatsCollection(const base::CommandLine& cmdline) {
  DCHECK(cmdline.HasSwitch(switches::kEnableStats) ||
         cmdline.HasSwitch(switches::kDisableStats));

  bool enable = !cmdline.HasSwitch(switches::kDisableStats);

  base::win::RegKey key;
  LONG status = key.Create(HKEY_LOCAL_MACHINE, kRegUpdaterClientStateAppPath,
                           KEY_SET_VALUE | KEY_WOW64_32KEY);
  if (status != ERROR_SUCCESS) {
    LOGFN(ERROR) << "Unable to open omaha key=" << kRegUpdaterClientStateAppPath
                 << " status=" << status;
  } else {
    status = key.WriteValue(kRegUsageStatsName, enable ? 1 : 0);
    if (status != ERROR_SUCCESS) {
      LOGFN(ERROR) << "Unable to write " << kRegUsageStatsName
                   << " value status=" << status;
    }
  }

  return status == ERROR_SUCCESS ? 0 : -1;
}

HRESULT WriteUninstallRegistryValues(const base::FilePath& setup_exe) {
  base::win::RegKey key;
  LONG status = key.Create(HKEY_LOCAL_MACHINE, kRegUpdaterClientStateAppPath,
                           KEY_SET_VALUE | KEY_WOW64_32KEY);
  if (status != ERROR_SUCCESS) {
    HRESULT hr = HRESULT_FROM_WIN32(status);
    LOGFN(ERROR) << "Unable to open " << kRegUpdaterClientStateAppPath
                 << " hr=" << putHR(hr);
    return hr;
  } else {
    status =
        key.WriteValue(kRegUninstallStringField, setup_exe.value().c_str());
    if (status != ERROR_SUCCESS) {
      HRESULT hr = HRESULT_FROM_WIN32(status);
      LOGFN(ERROR) << "Unable to write " << kRegUninstallStringField
                   << " hr=" << putHR(hr);
      return hr;
    }

    base::CommandLine uninstall_arguments(base::CommandLine::NO_PROGRAM);
    uninstall_arguments.AppendSwitch(switches::kUninstall);

    status = key.WriteValue(kRegUninstallArgumentsField,
                            uninstall_arguments.GetCommandLineString().c_str());
    if (status != ERROR_SUCCESS) {
      HRESULT hr = HRESULT_FROM_WIN32(status);
      LOGFN(ERROR) << "Unable to write " << kRegUninstallArgumentsField
                   << " hr=" << putHR(hr);
      return hr;
    }
  }

  return HRESULT_FROM_WIN32(status);
}

HRESULT WriteCredentialProviderRegistryValues(
    const base::FilePath& install_path) {
  HRESULT hr =
      StandaloneInstallerConfigurator::Get()->AddUninstallKey(install_path);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "AddUninstallKey  hr=" << putHR(hr);
    return hr;
  }

  base::win::RegKey key;
  LONG status = key.Create(HKEY_LOCAL_MACHINE, kGcpRootKeyName, KEY_SET_VALUE);
  if (status != ERROR_SUCCESS) {
    hr = HRESULT_FROM_WIN32(status);
    LOGFN(ERROR) << "Unable to create " << kGcpRootKeyName
                 << " hr=" << putHR(hr);
    return hr;
  }

  return S_OK;
}

}  // namespace credential_provider
