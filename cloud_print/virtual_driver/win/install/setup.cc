// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <setupapi.h>  // Must be included after windows.h
#include <winspool.h>
#include <stddef.h>

#include <iomanip>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/win/windows_version.h"
#include "cloud_print/common/win/cloud_print_utils.h"
#include "cloud_print/common/win/install_utils.h"
#include "cloud_print/virtual_driver/win/install/grit/virtual_driver_setup_resources.h"
#include "cloud_print/virtual_driver/win/virtual_driver_consts.h"
#include "cloud_print/virtual_driver/win/virtual_driver_helpers.h"

#include <strsafe.h>  // Must be after base headers to avoid deprecation
                      // warnings.

namespace cloud_print {

namespace {

const wchar_t kNameValue[] = L"GCP Virtual Driver";
const wchar_t kUninstallId[] = L"{74AA24E0-AC50-4B28-BA46-9CF05467C9B7}";
const wchar_t kGcpUrl[] = L"https://www.google.com/cloudprint";

const wchar_t kInfFileName[] = L"gcp_driver.inf";

const char kDelete[] = "delete";
const char kInstallSwitch[] = "install";
const char kRegisterSwitch[] = "register";
const char kUninstallSwitch[] = "uninstall";
const char kUnregisterSwitch[] = "unregister";

base::FilePath GetSystemPath(const base::string16& binary) {
  base::FilePath path;
  if (!base::PathService::Get(base::DIR_SYSTEM, &path)) {
    LOG(ERROR) << "Unable to get system path.";
    return path;
  }
  return path.Append(binary);
}

base::FilePath GetNativeSystemPath(const base::string16& binary) {
  if (!IsSystem64Bit())
    return GetSystemPath(binary);
  base::FilePath path;
  // Sysnative will bypass filesystem redirection and give us
  // the location of the 64bit system32 from a 32 bit process.
  if (!base::PathService::Get(base::DIR_WINDOWS, &path)) {
    LOG(ERROR) << "Unable to get windows path.";
    return path;
  }
  return path.Append(L"sysnative").Append(binary);
}

void SpoolerServiceCommand(const char* command) {
  base::FilePath net_path = GetNativeSystemPath(L"net");
  if (net_path.empty())
    return;
  base::CommandLine command_line(net_path);
  command_line.AppendArg(command);
  command_line.AppendArg("spooler");
  command_line.AppendArg("/y");

  base::LaunchOptions options;
  options.wait = true;
  options.start_hidden = true;
  VLOG(0) << command_line.GetCommandLineString();
  base::LaunchProcess(command_line, options);
}

HRESULT RegisterPortMonitor(bool install, const base::FilePath& install_path) {
  DCHECK(install || install_path.empty());
  base::FilePath target_path = GetNativeSystemPath(GetPortMonitorDllName());
  if (target_path.empty()) {
    LOG(ERROR) << "Unable to get port monitor target path.";
    return HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND);
  }
  if (install) {
    base::FilePath source_path = install_path.Append(GetPortMonitorDllName());
    if (!base::CopyFile(source_path, target_path)) {
      LOG(ERROR) << "Unable copy port monitor dll from " << source_path.value()
                 << " to " << target_path.value();
      return GetLastHResult();
    }
  } else if (!base::PathExists(target_path)) {
    // Already removed.  Just "succeed" silently.
    return S_OK;
  }

  base::FilePath regsvr32_path = GetNativeSystemPath(L"regsvr32.exe");
  if (regsvr32_path.empty()) {
    LOG(ERROR) << "Can't find regsvr32.exe.";
    return HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND);
  }

  base::CommandLine command_line(regsvr32_path);
  command_line.AppendArg("/s");
  if (!install) {
    command_line.AppendArg("/u");
  }

  // Use system32 path here because otherwise ::AddMonitor would fail.
  command_line.AppendArgPath(GetSystemPath(GetPortMonitorDllName()));

  base::LaunchOptions options;
  options.wait = true;

  base::Process regsvr32_process =
      base::LaunchProcess(command_line.GetCommandLineString(), options);
  if (!regsvr32_process.IsValid()) {
    LOG(ERROR) << "Unable to launch regsvr32.exe.";
    return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
  }

  DWORD exit_code = S_OK;
  if (install) {
    if (!GetExitCodeProcess(regsvr32_process.Handle(), &exit_code)) {
      LOG(ERROR) << "Unable to get regsvr32.exe exit code.";
      return GetLastHResult();
    }
    if (exit_code != 0) {
      LOG(ERROR) << "Regsvr32.exe failed with " << exit_code;
      return HRESULT_FROM_WIN32(exit_code);
    }
  } else {
    if (!base::DeleteFile(target_path, false)) {
      SpoolerServiceCommand("stop");
      bool deleted = base::DeleteFile(target_path, false);
      SpoolerServiceCommand("start");

      if (!deleted) {
        LOG(ERROR) << "Unable to delete " << target_path.value();
        return HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED);
      }
    }
  }
  return S_OK;
}

HRESULT InstallDriver(const base::FilePath& install_path) {
  DWORD size = MAX_PATH * 10;
  wchar_t package_path[MAX_PATH * 10] = {0};

  base::FilePath inf_file = install_path.Append(kInfFileName);
  base::string16 driver_name = LoadLocalString(IDS_DRIVER_NAME);

  HRESULT result = UploadPrinterDriverPackage(
      NULL, inf_file.value().c_str(), NULL,
      UPDP_SILENT_UPLOAD | UPDP_UPLOAD_ALWAYS, NULL, package_path, &size);
  if (FAILED(result)) {
    LOG(WARNING)
        << "Uploading the printer driver package to the driver cache silently "
        << "failed. Will retry with user UI. HRESULT=0x" << std::setbase(16)
        << result;

    result = UploadPrinterDriverPackage(
        NULL, inf_file.value().c_str(), NULL, UPDP_UPLOAD_ALWAYS,
        GetForegroundWindow(), package_path, &size);

    if (FAILED(result)) {
      LOG(WARNING)
          << "Uploading the printer driver package to the driver cache failed"
          << "with user UI. Aborting.";
      return result;
    }
  }

  result = InstallPrinterDriverFromPackage(
      NULL, package_path, driver_name.c_str(), NULL, IPDFP_COPY_ALL_FILES);
  if (FAILED(result)) {
    LOG(ERROR) << "Installing the printer driver failed.";
  }
  return result;
}

HRESULT UninstallDriver(const base::FilePath& install_path) {
  base::FilePath inf_file = install_path.Append(kInfFileName);
  int tries = 3;

  while (!DeletePrinterDriverPackage(NULL, inf_file.value().c_str(), NULL)) {
    if (GetLastError() == ERROR_UNKNOWN_PRINTER_DRIVER) {
      LOG(WARNING) << "Print driver is already uninstalled.";
      return S_OK;
    }
    // After deleting the printer it can take a few seconds before
    // the driver is free for deletion.  Retry a few times before giving up.
    LOG(WARNING) << "Attempt to delete printer driver failed.  Retrying.";
    tries--;
    Sleep(2000);
  }
  if (tries <= 0) {
    HRESULT result = GetLastHResult();
    LOG(ERROR) << "Unable to delete printer driver.";
    return result;
  }
  return S_OK;
}

HRESULT InstallPrinter(void) {
  PRINTER_INFO_2 printer_info = {0};

  // None of the print API structures likes constant strings even though they
  // don't modify the string.  const_casting is the cleanest option.
  base::string16 driver_name = LoadLocalString(IDS_DRIVER_NAME);
  printer_info.pDriverName = const_cast<LPWSTR>(driver_name.c_str());
  printer_info.pPrinterName = const_cast<LPWSTR>(driver_name.c_str());
  printer_info.pComment = const_cast<LPWSTR>(driver_name.c_str());
  printer_info.pLocation = const_cast<LPWSTR>(kGcpUrl);
  base::string16 port_name;
  printer_info.pPortName = const_cast<LPWSTR>(kPortName);
  printer_info.Attributes = PRINTER_ATTRIBUTE_DIRECT | PRINTER_ATTRIBUTE_LOCAL;
  printer_info.pPrintProcessor = const_cast<LPWSTR>(L"winprint");
  HANDLE handle = AddPrinter(NULL, 2, reinterpret_cast<BYTE*>(&printer_info));
  if (handle == NULL) {
    HRESULT result = GetLastHResult();
    LOG(ERROR) << "Unable to add printer";
    return result;
  }
  ClosePrinter(handle);
  return S_OK;
}

HRESULT UninstallPrinter(void) {
  HANDLE handle = NULL;
  PRINTER_DEFAULTS printer_defaults = {0};
  printer_defaults.DesiredAccess = PRINTER_ALL_ACCESS;
  base::string16 driver_name = LoadLocalString(IDS_DRIVER_NAME);
  if (!OpenPrinter(const_cast<LPWSTR>(driver_name.c_str()), &handle,
                   &printer_defaults)) {
    // If we can't open the printer, it was probably already removed.
    LOG(WARNING) << "Unable to open printer";
    return S_OK;
  }
  if (!DeletePrinter(handle)) {
    HRESULT result = GetLastHResult();
    LOG(ERROR) << "Unable to delete printer";
    ClosePrinter(handle);
    return result;
  }
  ClosePrinter(handle);
  return S_OK;
}

bool IsOSSupported() {
  // We don't support Vista or older.
  return base::win::GetVersion() >= base::win::Version::WIN7;
}

HRESULT RegisterVirtualDriver(const base::FilePath& install_path) {
  HRESULT result = S_OK;

  DCHECK(base::DirectoryExists(install_path));
  if (!IsOSSupported()) {
    LOG(ERROR) << "Requires Windows 7 or later.";
    return HRESULT_FROM_WIN32(ERROR_OLD_WIN_VERSION);
  }

  result = InstallDriver(install_path);
  if (FAILED(result)) {
    LOG(ERROR) << "Unable to install driver.";
    return result;
  }

  result = RegisterPortMonitor(true, install_path);
  if (FAILED(result)) {
    LOG(ERROR) << "Unable to register port monitor.";
    return result;
  }

  result = InstallPrinter();
  if (FAILED(result) &&
      result != HRESULT_FROM_WIN32(ERROR_PRINTER_ALREADY_EXISTS)) {
    LOG(ERROR) << "Unable to install printer.";
    return result;
  }
  return S_OK;
}

HRESULT TryUnregisterVirtualDriver(const base::FilePath& install_path) {
  HRESULT result = S_OK;
  result = UninstallPrinter();
  if (FAILED(result)) {
    LOG(ERROR) << "Unable to delete printer.";
    return result;
  }
  result = UninstallDriver(install_path);
  if (FAILED(result)) {
    LOG(ERROR) << "Unable to remove driver.";
    return result;
  }
  // The second argument is ignored if the first is false.
  result = RegisterPortMonitor(false, base::FilePath());
  if (FAILED(result)) {
    LOG(ERROR) << "Unable to remove port monitor.";
    return result;
  }
  return S_OK;
}

HRESULT UnregisterVirtualDriver(const base::FilePath& install_path) {
  HRESULT hr = S_FALSE;
  for (int i = 0; i < 2; ++i) {
    hr = TryUnregisterVirtualDriver(install_path);
    if (SUCCEEDED(hr)) {
      break;
    }
    // Restart spooler and try again.
    SpoolerServiceCommand("stop");
    SpoolerServiceCommand("start");
  }
  return hr;
}

HRESULT DoUninstall(const base::FilePath& install_path) {
  DeleteGoogleUpdateKeys(kGoogleUpdateProductId);
  HRESULT result = UnregisterVirtualDriver(install_path);
  if (FAILED(result))
    return result;
  DeleteUninstallKey(kUninstallId);
  DeleteProgramDir(kDelete);
  return S_OK;
}

HRESULT DoUnregister(const base::FilePath& install_path) {
  return UnregisterVirtualDriver(install_path);
}

HRESULT DoRegister(const base::FilePath& install_path) {
  HRESULT result = UnregisterVirtualDriver(install_path);
  if (FAILED(result))
    return result;
  return RegisterVirtualDriver(install_path);
}

HRESULT DoDelete(const base::FilePath& install_path) {
  if (install_path.value().empty())
    return E_INVALIDARG;
  if (!base::DirectoryExists(install_path))
    return S_FALSE;
  Sleep(5000);  // Give parent some time to exit.
  return base::DeleteFile(install_path, true) ? S_OK : E_FAIL;
}

HRESULT DoInstall(const base::FilePath& install_path) {
  HRESULT result = UnregisterVirtualDriver(install_path);
  if (FAILED(result)) {
    LOG(ERROR) << "Unable to unregister.";
    return result;
  }
  base::FilePath old_install_path = GetInstallLocation(kUninstallId);
  if (!old_install_path.value().empty() && install_path != old_install_path) {
    if (base::DirectoryExists(old_install_path))
      base::DeleteFile(old_install_path, true);
  }
  CreateUninstallKey(kUninstallId, LoadLocalString(IDS_DRIVER_NAME),
                     kUninstallSwitch);
  result = RegisterVirtualDriver(install_path);
  if (FAILED(result))
    return result;
  SetGoogleUpdateKeys(kGoogleUpdateProductId, kNameValue);
  return result;
}

HRESULT ExecuteCommands() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  base::FilePath exe_path;
  if (!base::PathService::Get(base::DIR_EXE, &exe_path) ||
      !base::DirectoryExists(exe_path)) {
    return HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND);
  }

  if (command_line.HasSwitch(kDelete)) {
    return DoDelete(command_line.GetSwitchValuePath(kDelete));
  } else if (command_line.HasSwitch(kUninstallSwitch)) {
    return DoUninstall(exe_path);
  } else if (command_line.HasSwitch(kInstallSwitch)) {
    return DoInstall(exe_path);
  } else if (command_line.HasSwitch(kUnregisterSwitch)) {
    return DoUnregister(exe_path);
  } else if (command_line.HasSwitch(kRegisterSwitch)) {
    return DoRegister(exe_path);
  }

  return E_INVALIDARG;
}

}  // namespace

}  // namespace cloud_print

int WINAPI WinMain(__in HINSTANCE hInstance,
                   __in HINSTANCE hPrevInstance,
                   __in LPSTR lpCmdLine,
                   __in int nCmdShow) {
  base::AtExitManager at_exit_manager;
  base::CommandLine::Init(0, nullptr);

  HRESULT retval = cloud_print::ExecuteCommands();

  if (retval == HRESULT_FROM_WIN32(ERROR_BAD_DRIVER)) {
    cloud_print::SetGoogleUpdateError(
        cloud_print::kGoogleUpdateProductId,
        cloud_print::LoadLocalString(IDS_ERROR_NO_XPS));
  } else if (FAILED(retval)) {
    cloud_print::SetGoogleUpdateError(cloud_print::kGoogleUpdateProductId,
                                      retval);
  }

  VLOG(0) << cloud_print::GetErrorMessage(retval) << " HRESULT=0x"
          << std::setbase(16) << retval;

  // Installer is silent by default as required by Google Update.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch("verbose")) {
    cloud_print::DisplayWindowsMessage(
        nullptr, retval, cloud_print::LoadLocalString(IDS_DRIVER_NAME));
  }
  return retval;
}
