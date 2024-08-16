// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

// NOTE: This code is a legacy utility API for partners to check whether
//       Chrome can be installed and launched. Recent updates are being made
//       to add new functionality. These updates use code from Chromium, the old
//       coded against the win32 api directly. If you have an itch to shave a
//       yak, feel free to re-write the old code too.

#include "chrome/installer/gcapi/gcapi.h"

#include <windows.h>

#include <versionhelpers.h>

#include <sddl.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#define STRSAFE_NO_DEPRECATE
#include <objbase.h>

#include <strsafe.h>
#include <tlhelp32.h>
#include <wrl/client.h>

#include <algorithm>
#include <cstdlib>
#include <iterator>
#include <limits>
#include <set>
#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/process/launch.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/win/registry.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/scoped_handle.h"
#include "base/win/wmi.h"
#include "chrome/installer/gcapi/gcapi_reactivation.h"
#include "chrome/installer/gcapi/google_update_util.h"
#include "chrome/installer/launcher_support/chrome_launcher_support.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/util_constants.h"
#include "chrome/updater/app/server/win/updater_legacy_idl.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"

using base::Time;
using base::win::RegKey;
using base::win::ScopedCOMInitializer;
using base::win::ScopedHandle;
using Microsoft::WRL::ComPtr;

namespace {

const wchar_t kGCAPITempKey[] = L"Software\\Google\\GCAPITemp";

const wchar_t kChromeRegVersion[] = L"pv";
const wchar_t kNoChromeOfferUntil[] =
    L"SOFTWARE\\Google\\No Chrome Offer Until";

const wchar_t kC1FPendingKey[] = L"Software\\Google\\Common\\Rlz\\Events\\C";
const wchar_t kC1FSentKey[] =
    L"Software\\Google\\Common\\Rlz\\StatefulEvents\\C";
const wchar_t kC1FKey[] = L"C1F";

const wchar_t kRelaunchBrandcodeValue[] = L"RelaunchBrandcode";
const wchar_t kRelaunchAllowedAfterValue[] = L"RelaunchAllowedAfter";

// Prefix used to match the window class for Chrome windows.
const wchar_t kChromeWindowClassPrefix[] = L"Chrome_WidgetWin_";

// Return the company name specified in the file version info resource.
bool GetCompanyName(const wchar_t* filename, wchar_t* buffer, DWORD out_len) {
  wchar_t file_version_info[8192];
  DWORD handle = 0;
  DWORD buffer_size = 0;

  buffer_size = ::GetFileVersionInfoSize(filename, &handle);
  // Cannot stats the file or our buffer size is too small (very unlikely).
  if (buffer_size == 0 || buffer_size > _countof(file_version_info))
    return false;

  buffer_size = _countof(file_version_info);
  memset(file_version_info, 0, buffer_size);
  if (!::GetFileVersionInfo(filename, handle, buffer_size, file_version_info))
    return false;

  DWORD data_len = 0;
  LPVOID data = nullptr;
  // Retrieve the language and codepage code if exists.
  buffer_size = 0;
  if (!::VerQueryValue(file_version_info, TEXT("\\VarFileInfo\\Translation"),
                       reinterpret_cast<LPVOID*>(&data),
                       reinterpret_cast<UINT*>(&data_len)))
    return false;
  if (data_len != 4)
    return false;

  wchar_t info_name[256];
  DWORD lang = 0;
  // Formulate the string to retrieve the company name of the specific
  // language codepage.
  memcpy(&lang, data, 4);
  ::StringCchPrintf(info_name, _countof(info_name),
                    L"\\StringFileInfo\\%02X%02X%02X%02X\\CompanyName",
                    (lang & 0xff00) >> 8, (lang & 0xff),
                    (lang & 0xff000000) >> 24, (lang & 0xff0000) >> 16);

  data_len = 0;
  if (!::VerQueryValue(file_version_info, info_name,
                       reinterpret_cast<LPVOID*>(&data),
                       reinterpret_cast<UINT*>(&data_len)))
    return false;
  if (data_len <= 0 || data_len >= (out_len / sizeof(wchar_t)))
    return false;

  memset(buffer, 0, out_len);
  ::StringCchCopyN(buffer, (out_len / sizeof(wchar_t)),
                   reinterpret_cast<const wchar_t*>(data), data_len);
  return true;
}

// Offsets the current date by |months|. |months| must be between 0 and 12.
// The returned date is in the YYYYMMDD format.
DWORD FormatDateOffsetByMonths(int months) {
  DCHECK(months >= 0 && months <= 12);

  SYSTEMTIME now;
  GetLocalTime(&now);
  now.wMonth += months;
  if (now.wMonth > 12) {
    now.wMonth -= 12;
    now.wYear += 1;
  }

  return now.wYear * 10000 + now.wMonth * 100 + now.wDay;
}

// Return true if we can re-offer Chrome; false, otherwise.
// Each partner can only offer Chrome once every six months.
bool CanReOfferChrome(BOOL set_flag) {
  wchar_t filename[MAX_PATH + 1];
  wchar_t company[MAX_PATH];

  // If we cannot retrieve the version info of the executable or company
  // name, we allow the Chrome to be offered because there is no past
  // history to be found.
  if (::GetModuleFileName(nullptr, filename, MAX_PATH) == 0)
    return true;
  if (!GetCompanyName(filename, company, sizeof(company)))
    return true;

  bool can_re_offer = true;
  DWORD disposition = 0;
  HKEY key = nullptr;
  if (::RegCreateKeyEx(HKEY_LOCAL_MACHINE, kNoChromeOfferUntil, 0, nullptr,
                       REG_OPTION_NON_VOLATILE, KEY_READ | KEY_WRITE, nullptr,
                       &key, &disposition) == ERROR_SUCCESS) {
    // Get today's date, and format it as YYYYMMDD numeric value.
    DWORD today = FormatDateOffsetByMonths(0);

    // Cannot re-offer, if the timer already exists and is not expired yet.
    DWORD value_type = REG_DWORD;
    DWORD value_data = 0;
    DWORD value_length = sizeof(DWORD);
    if (::RegQueryValueEx(key, company, 0, &value_type,
                          reinterpret_cast<LPBYTE>(&value_data),
                          &value_length) == ERROR_SUCCESS &&
        REG_DWORD == value_type && value_data > today) {
      // The time has not expired, we cannot offer Chrome.
      can_re_offer = false;
    } else {
      // Delete the old or invalid value.
      ::RegDeleteValue(key, company);
      if (set_flag) {
        // Set expiration date for offer as six months from today,
        // represented as a YYYYMMDD numeric value.
        DWORD value = FormatDateOffsetByMonths(6);
        ::RegSetValueEx(key, company, 0, REG_DWORD, (LPBYTE)&value,
                        sizeof(DWORD));
      }
    }

    ::RegCloseKey(key);
  }

  return can_re_offer;
}

bool IsChromeInstalled(HKEY root_key) {
  RegKey key;
  return key.Open(root_key, gcapi_internals::kChromeRegClientsKey,
                  KEY_READ | KEY_WOW64_32KEY) == ERROR_SUCCESS &&
         key.HasValue(kChromeRegVersion);
}

// Returns true if the |subkey| in |root| has the kC1FKey entry set to 1.
bool RegKeyHasC1F(HKEY root, const wchar_t* subkey) {
  RegKey key;
  DWORD value;
  return key.Open(root, subkey, KEY_READ | KEY_WOW64_32KEY) == ERROR_SUCCESS &&
         key.ReadValueDW(kC1FKey, &value) == ERROR_SUCCESS &&
         value == static_cast<DWORD>(1);
}

bool IsC1FSent() {
  // The C1F RLZ key can either be in HKCU or in HKLM (the HKLM RLZ key is made
  // readable to all-users via rlz_lib::CreateMachineState()) and can either be
  // in sent or pending state. Return true if there is a match for any of these
  // 4 states.
  return RegKeyHasC1F(HKEY_CURRENT_USER, kC1FSentKey) ||
         RegKeyHasC1F(HKEY_CURRENT_USER, kC1FPendingKey) ||
         RegKeyHasC1F(HKEY_LOCAL_MACHINE, kC1FSentKey) ||
         RegKeyHasC1F(HKEY_LOCAL_MACHINE, kC1FPendingKey);
}

bool IsWindowsVersionSupported() {
  return IsWindows7OrGreater();
}

// Note this function should not be called on old Windows versions where these
// Windows API are not available. We always invoke this function after checking
// that current OS is Vista or later.
bool VerifyAdminGroup() {
  SID_IDENTIFIER_AUTHORITY NtAuthority = {SECURITY_NT_AUTHORITY};
  PSID Group;
  BOOL check = ::AllocateAndInitializeSid(
      &NtAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0,
      0, 0, 0, 0, 0, &Group);
  if (check) {
    if (!::CheckTokenMembership(nullptr, Group, &check))
      check = FALSE;
  }
  ::FreeSid(Group);
  return (check == TRUE);
}

bool VerifyHKLMAccess() {
  wchar_t str[] = L"test";
  bool result = false;
  DWORD disposition = 0;
  HKEY key = nullptr;

  if (::RegCreateKeyEx(HKEY_LOCAL_MACHINE, kGCAPITempKey, 0, nullptr,
                       REG_OPTION_NON_VOLATILE,
                       KEY_READ | KEY_WRITE | KEY_WOW64_32KEY, nullptr, &key,
                       &disposition) == ERROR_SUCCESS) {
    if (::RegSetValueEx(key, str, 0, REG_SZ, (LPBYTE)str,
                        (DWORD)lstrlen(str)) == ERROR_SUCCESS) {
      result = true;
      RegDeleteValue(key, str);
    }

    RegCloseKey(key);

    //  If we create the main key, delete the entire key.
    if (disposition == REG_CREATED_NEW_KEY)
      RegDeleteKey(HKEY_LOCAL_MACHINE, kGCAPITempKey);
  }

  return result;
}

bool IsRunningElevated() {
  // This method should be called only for Vista or later.
  if (!IsWindowsVersionSupported() || !VerifyAdminGroup())
    return false;

  HANDLE process_token;
  if (!::OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &process_token))
    return false;

  TOKEN_ELEVATION_TYPE elevation_type = TokenElevationTypeDefault;
  DWORD size_returned = 0;
  if (!::GetTokenInformation(process_token, TokenElevationType, &elevation_type,
                             sizeof(elevation_type), &size_returned)) {
    ::CloseHandle(process_token);
    return false;
  }

  ::CloseHandle(process_token);
  return (elevation_type == TokenElevationTypeFull);
}

bool GetUserIdForProcess(size_t pid, wchar_t** user_sid) {
  HANDLE process_handle =
      ::OpenProcess(PROCESS_QUERY_INFORMATION, TRUE, static_cast<DWORD>(pid));
  if (process_handle == nullptr)
    return false;

  HANDLE process_token;
  bool result = false;
  if (::OpenProcessToken(process_handle, TOKEN_QUERY, &process_token)) {
    DWORD size = 0;
    ::GetTokenInformation(process_token, TokenUser, nullptr, 0, &size);
    if (::GetLastError() == ERROR_INSUFFICIENT_BUFFER ||
        ::GetLastError() == ERROR_SUCCESS) {
      DWORD actual_size = 0;
      BYTE* token_user = new BYTE[size];
      if ((::GetTokenInformation(process_token, TokenUser, token_user, size,
                                 &actual_size)) &&
          (actual_size <= size)) {
        PSID sid = reinterpret_cast<TOKEN_USER*>(token_user)->User.Sid;
        if (::ConvertSidToStringSid(sid, user_sid))
          result = true;
      }
      delete[] token_user;
    }
    ::CloseHandle(process_token);
  }
  ::CloseHandle(process_handle);
  return result;
}

struct SetWindowPosParams {
  int x;
  int y;
  int width;
  int height;
  DWORD flags;
  HWND window_insert_after;
  bool success;
  std::set<HWND> shunted_hwnds;
};

BOOL CALLBACK ChromeWindowEnumProc(HWND hwnd, LPARAM lparam) {
  wchar_t window_class[MAX_PATH] = {};
  SetWindowPosParams* params = reinterpret_cast<SetWindowPosParams*>(lparam);

  if (!params->shunted_hwnds.count(hwnd) &&
      ::GetClassName(hwnd, window_class, std::size(window_class)) &&
      base::StartsWith(window_class, kChromeWindowClassPrefix,
                       base::CompareCase::INSENSITIVE_ASCII) &&
      ::SetWindowPos(hwnd, params->window_insert_after, params->x, params->y,
                     params->width, params->height, params->flags)) {
    params->shunted_hwnds.insert(hwnd);
    params->success = true;
  }

  // Return TRUE to ensure we hit all possible top-level Chrome windows as per
  // http://msdn.microsoft.com/en-us/library/windows/desktop/ms633498.aspx
  return TRUE;
}

// Returns true and populates |chrome_exe_path| with the path to chrome.exe if
// a valid installation can be found.
bool GetGoogleChromePath(base::FilePath* chrome_exe_path) {
  HKEY install_key = HKEY_LOCAL_MACHINE;
  if (!IsChromeInstalled(install_key)) {
    install_key = HKEY_CURRENT_USER;
    if (!IsChromeInstalled(install_key)) {
      return false;
    }
  }

  // Now grab the uninstall string from the appropriate ClientState key
  // and use that as the base for a path to chrome.exe.
  *chrome_exe_path = chrome_launcher_support::GetChromePathForInstallationLevel(
      install_key == HKEY_LOCAL_MACHINE
          ? chrome_launcher_support::SYSTEM_LEVEL_INSTALLATION
          : chrome_launcher_support::USER_LEVEL_INSTALLATION,
      false /* is_sxs */);
  return !chrome_exe_path->empty();
}

}  // namespace

BOOL __stdcall GoogleChromeCompatibilityCheck(BOOL set_flag,
                                              int shell_mode,
                                              DWORD* reasons) {
  DWORD local_reasons = 0;

  bool is_windows_version_supported = IsWindowsVersionSupported();
  // System requirements?
  if (!is_windows_version_supported)
    local_reasons |= GCCC_ERROR_OSNOTSUPPORTED;

  if (IsChromeInstalled(HKEY_LOCAL_MACHINE))
    local_reasons |= GCCC_ERROR_SYSTEMLEVELALREADYPRESENT;

  if (IsChromeInstalled(HKEY_CURRENT_USER))
    local_reasons |= GCCC_ERROR_USERLEVELALREADYPRESENT;

  if (shell_mode == GCAPI_INVOKED_UAC_ELEVATION) {
    // Only check that we have HKLM write permissions if we specify that
    // GCAPI is being invoked from an elevated shell, or in admin mode
    if (!VerifyHKLMAccess()) {
      local_reasons |= GCCC_ERROR_ACCESSDENIED;
    } else if (is_windows_version_supported && !VerifyAdminGroup()) {
      // For Vista or later check for elevation since even for admin user we
      // could be running in non-elevated mode. We require integrity level High.
      local_reasons |= GCCC_ERROR_INTEGRITYLEVEL;
    }
  }

  // Then only check whether we can re-offer, if everything else is OK.
  if (local_reasons == 0 && !CanReOfferChrome(set_flag))
    local_reasons |= GCCC_ERROR_ALREADYOFFERED;

  // Done. Copy/return results.
  if (reasons != nullptr)
    *reasons = local_reasons;

  return (local_reasons == 0);
}

BOOL __stdcall LaunchGoogleChrome() {
  base::FilePath chrome_exe_path;
  if (!GetGoogleChromePath(&chrome_exe_path))
    return false;

  ScopedCOMInitializer com_initializer;
  if (::CoInitializeSecurity(nullptr, -1, nullptr, nullptr,
                             RPC_C_AUTHN_LEVEL_PKT_PRIVACY,
                             RPC_C_IMP_LEVEL_IDENTIFY, nullptr,
                             EOAC_DYNAMIC_CLOAKING, nullptr) != S_OK) {
    return false;
  }

  bool impersonation_success = false;
  absl::Cleanup revert_to_self = [&] {
    if (impersonation_success) {
      ::RevertToSelf();
    }
  };
  if (IsRunningElevated()) {
    wchar_t* curr_proc_sid;
    if (!GetUserIdForProcess(GetCurrentProcessId(), &curr_proc_sid)) {
      return false;
    }

    DWORD pid = 0;
    ::GetWindowThreadProcessId(::GetShellWindow(), &pid);
    if (pid <= 0) {
      ::LocalFree(curr_proc_sid);
      return false;
    }

    wchar_t* exp_proc_sid;
    if (GetUserIdForProcess(pid, &exp_proc_sid)) {
      if (_wcsicmp(curr_proc_sid, exp_proc_sid) == 0) {
        ScopedHandle process_handle(::OpenProcess(
            PROCESS_DUP_HANDLE | PROCESS_QUERY_INFORMATION, TRUE, pid));
        if (process_handle.IsValid()) {
          HANDLE process_token = nullptr;
          HANDLE user_token = nullptr;
          if (::OpenProcessToken(process_handle.Get(),
                                 TOKEN_DUPLICATE | TOKEN_QUERY,
                                 &process_token) &&
              ::DuplicateTokenEx(process_token,
                                 TOKEN_IMPERSONATE | TOKEN_QUERY |
                                     TOKEN_ASSIGN_PRIMARY | TOKEN_DUPLICATE,
                                 nullptr, SecurityImpersonation, TokenPrimary,
                                 &user_token) &&
              (::ImpersonateLoggedOnUser(user_token) != 0)) {
            impersonation_success = true;
          }
          if (user_token)
            ::CloseHandle(user_token);
          if (process_token)
            ::CloseHandle(process_token);
        }
      }
      ::LocalFree(exp_proc_sid);
    }

    ::LocalFree(curr_proc_sid);
    if (!impersonation_success) {
      return false;
    }
  }

  base::CommandLine chrome_command(chrome_exe_path);

  // Chrome queries for the SxS IIDs first, with a fallback to the legacy IID,
  // to make sure that marshaling loads the proxy/stub from the correct (HKLM)
  // hive.
  // If Omaha's process launcher does not work, Omaha may not be installed at
  // system level. Try just running Chrome instead.
  ComPtr<IUnknown> unknown;
  ComPtr<IProcessLauncher> ipl;
  return (SUCCEEDED(::CoCreateInstance(__uuidof(ProcessLauncherClass), nullptr,
                                       CLSCTX_LOCAL_SERVER,
                                       IID_PPV_ARGS(&unknown))) &&
          (SUCCEEDED(unknown.CopyTo(__uuidof(IProcessLauncherSystem),
                                    IID_PPV_ARGS_Helper(&ipl))) ||
           SUCCEEDED(unknown.As(&ipl))) &&
          SUCCEEDED(ipl->LaunchCmdLine(
              chrome_command.GetCommandLineString().c_str()))) ||
         base::LaunchProcess(chrome_command.GetCommandLineString(),
                             base::LaunchOptions())
             .IsValid();
}

BOOL __stdcall LaunchGoogleChromeWithDimensions(int x,
                                                int y,
                                                int width,
                                                int height,
                                                bool in_background) {
  if (in_background) {
    base::FilePath chrome_exe_path;
    if (!GetGoogleChromePath(&chrome_exe_path))
      return false;

    // When launching in the background, use WMI to ensure that chrome.exe is
    // is not our child process. This prevents it from pushing itself to
    // foreground.
    base::CommandLine chrome_command(chrome_exe_path);

    ScopedCOMInitializer com_initializer;
    if (!base::win::WmiLaunchProcess(chrome_command.GetCommandLineString(),
                                     nullptr)) {
      // For some reason WMI failed. Try and launch the old fashioned way,
      // knowing that visual glitches will occur when the window pops up.
      if (!LaunchGoogleChrome())
        return false;
    }

  } else {
    if (!LaunchGoogleChrome())
      return false;
  }

  HWND hwnd_insert_after = in_background ? HWND_BOTTOM : nullptr;
  DWORD set_window_flags = in_background ? SWP_NOACTIVATE : SWP_NOZORDER;

  if (x == -1 && y == -1)
    set_window_flags |= SWP_NOMOVE;

  if (width == -1 && height == -1)
    set_window_flags |= SWP_NOSIZE;

  SetWindowPosParams enum_params = {
      x, y, width, height, set_window_flags, hwnd_insert_after, false};

  // Chrome may have been launched, but the window may not have appeared
  // yet. Wait for it to appear for 10 seconds, but exit if it takes longer
  // than that.
  int ms_elapsed = 0;
  int timeout = 10000;
  bool found_window = false;
  while (ms_elapsed < timeout) {
    // Enum all top-level windows looking for Chrome windows.
    ::EnumWindows(ChromeWindowEnumProc, reinterpret_cast<LPARAM>(&enum_params));

    // Give it five more seconds after finding the first window until we stop
    // shoving new windows into the background.
    if (!found_window && enum_params.success) {
      found_window = true;
      timeout = ms_elapsed + 5000;
    }

    Sleep(10);
    ms_elapsed += 10;
  }

  return found_window;
}

BOOL __stdcall LaunchGoogleChromeInBackground() {
  return LaunchGoogleChromeWithDimensions(-1, -1, -1, -1, true);
}

int __stdcall GoogleChromeDaysSinceLastRun() {
  int days_since_last_run = std::numeric_limits<int>::max();

  if (IsChromeInstalled(HKEY_LOCAL_MACHINE) ||
      IsChromeInstalled(HKEY_CURRENT_USER)) {
    RegKey client_state(HKEY_CURRENT_USER,
                        gcapi_internals::kChromeRegClientStateKey,
                        KEY_QUERY_VALUE | KEY_WOW64_32KEY);
    if (client_state.Valid()) {
      std::wstring last_run;
      int64_t last_run_value = 0;
      if (client_state.ReadValue(google_update::kRegLastRunTimeField,
                                 &last_run) == ERROR_SUCCESS &&
          base::StringToInt64(last_run, &last_run_value)) {
        Time last_run_time = Time::FromInternalValue(last_run_value);
        base::TimeDelta difference = Time::NowFromSystemTime() - last_run_time;

        // We can end up with negative numbers here, given changes in system
        // clock time or due to base::TimeDelta's int64_t -> int truncation.
        int new_days_since_last_run = difference.InDays();
        if (new_days_since_last_run >= 0 &&
            new_days_since_last_run < days_since_last_run) {
          days_since_last_run = new_days_since_last_run;
        }
      }
    }
  }

  if (days_since_last_run == std::numeric_limits<int>::max()) {
    days_since_last_run = -1;
  }

  return days_since_last_run;
}

BOOL __stdcall CanOfferReactivation(const wchar_t* brand_code,
                                    int shell_mode,
                                    DWORD* error_code) {
  DCHECK(error_code);

  if (!brand_code) {
    if (error_code)
      *error_code = REACTIVATE_ERROR_INVALID_INPUT;
    return FALSE;
  }

  int days_since_last_run = GoogleChromeDaysSinceLastRun();
  if (days_since_last_run >= 0 &&
      days_since_last_run < kReactivationMinDaysDormant) {
    if (error_code)
      *error_code = REACTIVATE_ERROR_NOTDORMANT;
    return FALSE;
  }

  // Only run the code below when this function is invoked from a standard,
  // non-elevated cmd shell.  This is because this section of code looks at
  // values in HKEY_CURRENT_USER, and we only want to look at the logged-in
  // user's HKCU, not the admin user's HKCU.
  if (shell_mode == GCAPI_INVOKED_STANDARD_SHELL) {
    if (!IsChromeInstalled(HKEY_LOCAL_MACHINE) &&
        !IsChromeInstalled(HKEY_CURRENT_USER)) {
      if (error_code)
        *error_code = REACTIVATE_ERROR_NOTINSTALLED;
      return FALSE;
    }

    if (HasBeenReactivated()) {
      if (error_code)
        *error_code = REACTIVATE_ERROR_ALREADY_REACTIVATED;
      return FALSE;
    }
  }

  return TRUE;
}

BOOL __stdcall ReactivateChrome(const wchar_t* brand_code,
                                int shell_mode,
                                DWORD* error_code) {
  if (!CanOfferReactivation(brand_code, shell_mode, error_code)) {
    return FALSE;
  }

  if (SetReactivationBrandCode(brand_code, shell_mode)) {
    return TRUE;
  }

  if (error_code) {
    *error_code = REACTIVATE_ERROR_REACTIVATION_FAILED;
  }
  return FALSE;
}

BOOL __stdcall CanOfferRelaunch(const wchar_t** partner_brandcode_list,
                                int partner_brandcode_list_length,
                                int shell_mode,
                                DWORD* error_code) {
  DCHECK(error_code);

  if (!partner_brandcode_list || partner_brandcode_list_length <= 0) {
    if (error_code)
      *error_code = RELAUNCH_ERROR_INVALID_INPUT;
    return FALSE;
  }

  // These conditions need to be satisfied for relaunch:
  // a) Chrome should be installed;
  if (!IsChromeInstalled(HKEY_LOCAL_MACHINE) &&
      (shell_mode != GCAPI_INVOKED_STANDARD_SHELL ||
       !IsChromeInstalled(HKEY_CURRENT_USER))) {
    if (error_code)
      *error_code = RELAUNCH_ERROR_NOTINSTALLED;
    return FALSE;
  }

  // b) the installed brandcode should belong to that partner (in
  // brandcode_list);
  std::wstring installed_brandcode;
  bool valid_brandcode = false;
  if (gcapi_internals::GetBrand(&installed_brandcode)) {
    for (int i = 0; i < partner_brandcode_list_length; ++i) {
      if (!_wcsicmp(installed_brandcode.c_str(), partner_brandcode_list[i])) {
        valid_brandcode = true;
        break;
      }
    }
  }

  if (!valid_brandcode) {
    if (error_code)
      *error_code = RELAUNCH_ERROR_INVALID_PARTNER;
    return FALSE;
  }

  // c) C1F ping should not have been sent;
  if (IsC1FSent()) {
    if (error_code)
      *error_code = RELAUNCH_ERROR_PINGS_SENT;
    return FALSE;
  }

  // d) a minimum period (30 days) must have passed since Chrome was last used;
  int days_since_last_run = GoogleChromeDaysSinceLastRun();
  if (days_since_last_run >= 0 &&
      days_since_last_run < kRelaunchMinDaysDormant) {
    if (error_code)
      *error_code = RELAUNCH_ERROR_NOTDORMANT;
    return FALSE;
  }

  // e) a minimum period (6 months) must have passed since the previous
  // relaunch offer for the current user;
  RegKey key;
  DWORD min_relaunch_date;
  if (key.Open(HKEY_CURRENT_USER, gcapi_internals::kChromeRegClientStateKey,
               KEY_QUERY_VALUE | KEY_WOW64_32KEY) == ERROR_SUCCESS &&
      key.ReadValueDW(kRelaunchAllowedAfterValue, &min_relaunch_date) ==
          ERROR_SUCCESS &&
      FormatDateOffsetByMonths(0) < min_relaunch_date) {
    if (error_code)
      *error_code = RELAUNCH_ERROR_ALREADY_RELAUNCHED;
    return FALSE;
  }

  return TRUE;
}

BOOL __stdcall SetRelaunchOffered(const wchar_t** partner_brandcode_list,
                                  int partner_brandcode_list_length,
                                  const wchar_t* relaunch_brandcode,
                                  int shell_mode,
                                  DWORD* error_code) {
  if (!CanOfferRelaunch(partner_brandcode_list, partner_brandcode_list_length,
                        shell_mode, error_code))
    return FALSE;

  // Store the relaunched brand code and the minimum date for relaunch (6 months
  // from now).
  RegKey key;
  if (key.Create(HKEY_CURRENT_USER, gcapi_internals::kChromeRegClientStateKey,
                 KEY_SET_VALUE | KEY_WOW64_32KEY) != ERROR_SUCCESS ||
      key.WriteValue(kRelaunchBrandcodeValue, relaunch_brandcode) !=
          ERROR_SUCCESS ||
      key.WriteValue(kRelaunchAllowedAfterValue, FormatDateOffsetByMonths(6)) !=
          ERROR_SUCCESS) {
    if (error_code)
      *error_code = RELAUNCH_ERROR_RELAUNCH_FAILED;
    return FALSE;
  }

  return TRUE;
}
