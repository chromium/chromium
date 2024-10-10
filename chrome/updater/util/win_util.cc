// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/updater/util/win_util.h"

#include <windows.h>

#include <aclapi.h>
#include <combaseapi.h>
#include <objidl.h>
#include <regstr.h>
#include <shellapi.h>
#include <shlobj.h>
#include <winhttp.h>
#include <wrl/client.h>
#include <wtsapi32.h>

#include <cstdlib>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/base_paths_win.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/debug/alias.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback_helpers.h"
#include "base/functional/function_ref.h"
#include "base/logging.h"
#include "base/memory/free_deleter.h"
#include "base/path_service.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/process/process_iterator.h"
#include "base/ranges/algorithm.h"
#include "base/scoped_native_library.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/system/sys_info.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "base/win/atl.h"
#include "base/win/elevation_util.h"
#include "base/win/registry.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_handle.h"
#include "base/win/scoped_localalloc.h"
#include "base/win/scoped_process_information.h"
#include "base/win/scoped_variant.h"
#include "base/win/startup_information.h"
#include "base/win/win_util.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/registration_data.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/updater_version.h"
#include "chrome/updater/util/util.h"
#include "chrome/updater/win/scoped_handle.h"
#include "chrome/updater/win/user_info.h"
#include "chrome/updater/win/win_constants.h"
#include "chrome/windows_services/service_program/scoped_client_impersonation.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"

namespace updater {

namespace {

HResultOr<bool> IsUserRunningSplitToken() {
  HANDLE token = NULL;
  if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &token)) {
    return base::unexpected(HRESULTFromLastError());
  }
  base::win::ScopedHandle token_holder(token);
  TOKEN_ELEVATION_TYPE elevation_type = TokenElevationTypeDefault;
  DWORD size_returned = 0;
  if (!::GetTokenInformation(token_holder.Get(), TokenElevationType,
                             &elevation_type, sizeof(elevation_type),
                             &size_returned)) {
    return base::unexpected(HRESULTFromLastError());
  }
  bool is_split_token = elevation_type == TokenElevationTypeFull ||
                        elevation_type == TokenElevationTypeLimited;
  CHECK(is_split_token || elevation_type == TokenElevationTypeDefault);
  return base::ok(is_split_token);
}

HRESULT GetSidIntegrityLevel(PSID sid, MANDATORY_LEVEL* level) {
  if (!::IsValidSid(sid)) {
    return E_FAIL;
  }
  SID_IDENTIFIER_AUTHORITY* authority = ::GetSidIdentifierAuthority(sid);
  if (!authority) {
    return E_FAIL;
  }
  constexpr SID_IDENTIFIER_AUTHORITY kMandatoryLabelAuth =
      SECURITY_MANDATORY_LABEL_AUTHORITY;
  if (std::memcmp(authority, &kMandatoryLabelAuth,
                  sizeof(SID_IDENTIFIER_AUTHORITY))) {
    return E_FAIL;
  }
  PUCHAR count = ::GetSidSubAuthorityCount(sid);
  if (!count || *count != 1) {
    return E_FAIL;
  }
  DWORD* rid = ::GetSidSubAuthority(sid, 0);
  if (!rid) {
    return E_FAIL;
  }
  if ((*rid & 0xFFF) != 0 || *rid > SECURITY_MANDATORY_PROTECTED_PROCESS_RID) {
    return E_FAIL;
  }
  *level = static_cast<MANDATORY_LEVEL>(*rid >> 12);
  return S_OK;
}

// Gets the mandatory integrity level of a process.
HRESULT GetProcessIntegrityLevel(DWORD process_id, MANDATORY_LEVEL* level) {
  HANDLE process = ::OpenProcess(PROCESS_QUERY_INFORMATION, false, process_id);
  if (!process) {
    return HRESULTFromLastError();
  }
  base::win::ScopedHandle process_holder(process);
  HANDLE token = NULL;
  if (!::OpenProcessToken(process_holder.Get(),
                          TOKEN_QUERY | TOKEN_QUERY_SOURCE, &token)) {
    return HRESULTFromLastError();
  }
  base::win::ScopedHandle token_holder(token);
  DWORD label_size = 0;
  if (::GetTokenInformation(token_holder.Get(), TokenIntegrityLevel, nullptr, 0,
                            &label_size) ||
      ::GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
    return E_FAIL;
  }
  std::unique_ptr<TOKEN_MANDATORY_LABEL, base::FreeDeleter> label(
      static_cast<TOKEN_MANDATORY_LABEL*>(std::malloc(label_size)));
  if (!::GetTokenInformation(token_holder.Get(), TokenIntegrityLevel,
                             label.get(), label_size, &label_size)) {
    return HRESULTFromLastError();
  }
  return GetSidIntegrityLevel(label->Label.Sid, level);
}

bool IsExplorerRunningAtMediumOrLower() {
  base::NamedProcessIterator iter(L"EXPLORER.EXE", nullptr);
  while (const base::ProcessEntry* process_entry = iter.NextProcessEntry()) {
    MANDATORY_LEVEL level = MandatoryLevelUntrusted;
    if (SUCCEEDED(GetProcessIntegrityLevel(process_entry->pid(), &level)) &&
        level <= MandatoryLevelMedium) {
      return true;
    }
  }
  return false;
}

// Creates a WS_POPUP | WS_VISIBLE with zero
// size, of the STATIC WNDCLASS. It uses the default running EXE module
// handle for creation.
//
// A visible centered foreground window is needed as the parent in Windows 7 and
// above, to allow the UAC prompt to come up in the foreground, centered.
// Otherwise, the elevation prompt will be minimized on the taskbar. A zero size
// window works. A plain vanilla WS_POPUP allows the window to be free of
// adornments. WS_EX_TOOLWINDOW prevents the task bar from showing the
// zero-sized window.
//
// Returns NULL on failure. Call ::GetLastError() to get extended error
// information on failure.
HWND CreateForegroundParentWindowForUAC() {
  CWindow foreground_parent;
  if (foreground_parent.Create(L"STATIC", NULL, NULL, NULL,
                               WS_POPUP | WS_VISIBLE, WS_EX_TOOLWINDOW)) {
    foreground_parent.CenterWindow(NULL);
    ::SetForegroundWindow(foreground_parent);
  }
  return foreground_parent.Detach();
}

// Compares the OS, service pack, and build numbers using `::VerifyVersionInfo`,
// in accordance with `type_mask` and `oper`.
bool CompareOSVersionsInternal(const OSVERSIONINFOEX& os,
                               DWORD type_mask,
                               BYTE oper) {
  CHECK(type_mask);
  CHECK(oper);

  ULONGLONG cond_mask = 0;
  cond_mask = ::VerSetConditionMask(cond_mask, VER_MAJORVERSION, oper);
  cond_mask = ::VerSetConditionMask(cond_mask, VER_MINORVERSION, oper);
  cond_mask = ::VerSetConditionMask(cond_mask, VER_SERVICEPACKMAJOR, oper);
  cond_mask = ::VerSetConditionMask(cond_mask, VER_SERVICEPACKMINOR, oper);
  cond_mask = ::VerSetConditionMask(cond_mask, VER_BUILDNUMBER, oper);

  // `::VerifyVersionInfo` could return `FALSE` due to an error other than
  // `ERROR_OLD_WIN_VERSION`. We do not handle that case here.
  // https://msdn.microsoft.com/ms725492.
  OSVERSIONINFOEX os_in = os;
  return ::VerifyVersionInfo(&os_in, type_mask, cond_mask);
}

std::optional<int> DaynumFromDWORD(DWORD value) {
  const int daynum = static_cast<int>(value);

  // When daynum is positive, it is the number of days since January 1, 2007.
  // It's reasonable to only accept value between 3000 (maps to Mar 20, 2015)
  // and 50000 (maps to Nov 24, 2143).
  // -1 is special value for first install.
  return daynum == -1 || (daynum >= 3000 && daynum <= 50000)
             ? std::make_optional(daynum)
             : std::nullopt;
}

}  // namespace

NamedObjectAttributes::NamedObjectAttributes(const std::wstring& name,
                                             const CSecurityDesc& sd)
    : name(name), sa(CSecurityAttributes(sd)) {}
NamedObjectAttributes::~NamedObjectAttributes() = default;

HRESULT HRESULTFromLastError() {
  const auto error_code = ::GetLastError();
  return (error_code != NO_ERROR) ? HRESULT_FROM_WIN32(error_code) : E_FAIL;
}

NamedObjectAttributes GetNamedObjectAttributes(const wchar_t* base_name,
                                               UpdaterScope scope) {
  CHECK(base_name);

  switch (scope) {
    case UpdaterScope::kUser: {
      std::wstring user_sid;
      GetProcessUser(nullptr, nullptr, &user_sid);
      return {
          base::StrCat({kGlobalPrefix, base_name, user_sid}),
          GetCurrentUserDefaultSecurityDescriptor().value_or(CSecurityDesc())};
    }
    case UpdaterScope::kSystem:
      // Grant access to administrators and system.
      return {base::StrCat({kGlobalPrefix, base_name}),
              GetAdminDaclSecurityDescriptor(GENERIC_ALL)};
  }
}

std::optional<CSecurityDesc> GetCurrentUserDefaultSecurityDescriptor() {
  CAccessToken token;
  if (!token.GetProcessToken(TOKEN_QUERY)) {
    return std::nullopt;
  }

  CSecurityDesc security_desc;
  CSid sid_owner;
  if (!token.GetOwner(&sid_owner)) {
    return std::nullopt;
  }

  security_desc.SetOwner(sid_owner);
  CSid sid_group;
  if (!token.GetPrimaryGroup(&sid_group)) {
    return std::nullopt;
  }

  security_desc.SetGroup(sid_group);

  CDacl dacl;
  if (!token.GetDefaultDacl(&dacl)) {
    return std::nullopt;
  }

  CSid sid_user;
  if (!token.GetUser(&sid_user)) {
    return std::nullopt;
  }
  if (!dacl.AddAllowedAce(sid_user, GENERIC_ALL)) {
    return std::nullopt;
  }

  security_desc.SetDacl(dacl);

  return security_desc;
}

CSecurityDesc GetAdminDaclSecurityDescriptor(ACCESS_MASK accessmask) {
  CSecurityDesc sd;
  CDacl dacl;
  dacl.AddAllowedAce(Sids::System(), accessmask);
  dacl.AddAllowedAce(Sids::Admins(), accessmask);

  sd.SetOwner(Sids::Admins());
  sd.SetGroup(Sids::Admins());
  sd.SetDacl(dacl);
  sd.MakeAbsolute();
  return sd;
}

std::wstring GetAppClientsKey(const std::string& app_id) {
  return GetAppClientsKey(base::ASCIIToWide(app_id));
}

std::wstring GetAppClientsKey(const std::wstring& app_id) {
  return base::StrCat({CLIENTS_KEY, app_id});
}

std::wstring GetAppClientStateKey(const std::string& app_id) {
  return GetAppClientStateKey(base::ASCIIToWide(app_id));
}

std::wstring GetAppClientStateKey(const std::wstring& app_id) {
  return base::StrCat({CLIENT_STATE_KEY, app_id});
}

std::wstring GetAppCohortKey(const std::string& app_id) {
  return GetAppCohortKey(base::ASCIIToWide(app_id));
}

std::wstring GetAppCohortKey(const std::wstring& app_id) {
  return base::StrCat({GetAppClientStateKey(app_id), L"\\", kRegKeyCohort});
}

std::wstring GetAppCommandKey(const std::wstring& app_id,
                              const std::wstring& command_id) {
  return base::StrCat(
      {GetAppClientsKey(app_id), L"\\", kRegKeyCommands, L"\\", command_id});
}

std::string GetAppAPValue(UpdaterScope scope, const std::string& app_id) {
  base::win::RegKey client_state_key;
  if (client_state_key.Open(
          UpdaterScopeToHKeyRoot(scope),
          GetAppClientStateKey(base::ASCIIToWide(app_id)).c_str(),
          Wow6432(KEY_READ)) == ERROR_SUCCESS) {
    std::wstring ap;
    if (client_state_key.ReadValue(kRegValueAP, &ap) == ERROR_SUCCESS) {
      return base::WideToASCII(ap);
    }
  }
  return {};
}

std::wstring GetRegistryKeyClientsUpdater() {
  return GetAppClientsKey(kUpdaterAppId);
}

std::wstring GetRegistryKeyClientStateUpdater() {
  return GetAppClientStateKey(kUpdaterAppId);
}

bool SetRegistryKey(HKEY root,
                    const std::wstring& key,
                    const std::wstring& name,
                    const std::wstring& value) {
  base::win::RegKey rkey;
  LONG result = rkey.Create(root, key.c_str(), Wow6432(KEY_WRITE));
  if (result != ERROR_SUCCESS) {
    VLOG(1) << "Failed to open (" << root << ") " << key << ": " << result;
    return false;
  }
  result = rkey.WriteValue(name.c_str(), value.c_str());
  if (result != ERROR_SUCCESS) {
    VLOG(1) << "Failed to write (" << root << ") " << key << " @ " << name
            << ": " << result;
    return false;
  }
  return result == ERROR_SUCCESS;
}

bool SetEulaAccepted(UpdaterScope scope, bool eula_accepted) {
  const HKEY root = UpdaterScopeToHKeyRoot(scope);
  return eula_accepted
             ? DeleteRegValue(root, UPDATER_KEY, L"eulaaccepted")
             : base::win::RegKey(root, UPDATER_KEY, Wow6432(KEY_WRITE))
                       .WriteValue(L"eulaaccepted", 0ul) == ERROR_SUCCESS;
}

HResultOr<bool> IsTokenAdmin(HANDLE token) {
  SID_IDENTIFIER_AUTHORITY nt_authority = SECURITY_NT_AUTHORITY;
  PSID administrators_group = nullptr;
  if (!::AllocateAndInitializeSid(&nt_authority, 2, SECURITY_BUILTIN_DOMAIN_RID,
                                  DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0,
                                  &administrators_group)) {
    return base::unexpected(HRESULTFromLastError());
  }
  absl::Cleanup free_sid = [&] { ::FreeSid(administrators_group); };
  BOOL is_member = false;
  if (!::CheckTokenMembership(token, administrators_group, &is_member)) {
    return base::unexpected(HRESULTFromLastError());
  }
  return base::ok(is_member);
}

HResultOr<bool> IsUserAdmin() {
  return IsTokenAdmin(NULL);
}

HResultOr<bool> IsUserNonElevatedAdmin() {
  HANDLE token = NULL;
  if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_READ, &token)) {
    return base::unexpected(HRESULTFromLastError());
  }
  bool is_user_non_elevated_admin = false;
  base::win::ScopedHandle token_holder(token);
  TOKEN_ELEVATION_TYPE elevation_type = TokenElevationTypeDefault;
  DWORD size_returned = 0;
  if (::GetTokenInformation(token_holder.Get(), TokenElevationType,
                            &elevation_type, sizeof(elevation_type),
                            &size_returned)) {
    if (elevation_type == TokenElevationTypeLimited) {
      is_user_non_elevated_admin = true;
    }
  }
  return base::ok(is_user_non_elevated_admin);
}

HResultOr<bool> IsCOMCallerAdmin() {
  ScopedClientImpersonation impersonate_client;
  if (!impersonate_client.is_valid()) {
    // RPC_E_CALL_COMPLETE indicates that the caller is in-proc.
    if (impersonate_client.result() != RPC_E_CALL_COMPLETE) {
      return base::unexpected(impersonate_client.result());
    }
    return base::ok(::IsUserAnAdmin());
  }

  HResultOr<ScopedKernelHANDLE> token = []() -> decltype(token) {
    ScopedKernelHANDLE token;
    if (!::OpenThreadToken(::GetCurrentThread(), TOKEN_QUERY, TRUE,
                           ScopedKernelHANDLE::Receiver(token).get())) {
      HRESULT hr = HRESULTFromLastError();
      LOG(ERROR) << "::OpenThreadToken failed: " << std::hex << hr;
      return base::unexpected(hr);
    }
    return token;
  }();

  if (!token.has_value()) {
    return base::unexpected(token.error());
  }

  return IsTokenAdmin(token.value().get()).transform_error([](HRESULT error) {
    CHECK(FAILED(error));
    LOG(ERROR) << "IsTokenAdmin failed: " << std::hex << error;
    return error;
  });
}

bool IsUACOn() {
  // The presence of a split token definitively indicates that UAC is on. But
  // the absence of the token does not necessarily indicate that UAC is off.
  HResultOr<bool> is_split_token = IsUserRunningSplitToken();
  return (is_split_token.has_value() && is_split_token.value()) ||
         IsExplorerRunningAtMediumOrLower();
}

bool IsElevatedWithUACOn() {
  HResultOr<bool> is_user_admin = IsUserAdmin();
  return (!is_user_admin.has_value() || is_user_admin.value()) && IsUACOn();
}

std::string GetUACState() {
  std::string s;

  HResultOr<bool> is_user_admin = IsUserAdmin();
  if (is_user_admin.has_value()) {
    base::StringAppendF(&s, "IsUserAdmin: %d, ", is_user_admin.value());
  }

  HResultOr<bool> is_user_non_elevated_admin = IsUserNonElevatedAdmin();
  if (is_user_non_elevated_admin.has_value()) {
    base::StringAppendF(&s, "IsUserNonElevatedAdmin: %d, ",
                        is_user_non_elevated_admin.value());
  }

  base::StringAppendF(&s, "IsUACOn: %d, IsElevatedWithUACOn: %d, ", IsUACOn(),
                      IsElevatedWithUACOn());

  base::StringAppendF(&s, "LUA: %d", base::win::UserAccountControlIsEnabled());
  return s;
}

std::wstring GetServiceName(bool is_internal_service) {
  std::wstring service_name = base::StrCat(
      {base::ASCIIToWide(PRODUCT_FULLNAME_STRING), L" ",
       is_internal_service ? kWindowsInternalServiceName : kWindowsServiceName,
       L" ", kUpdaterVersionUtf16});
  std::erase_if(service_name, base::IsAsciiWhitespace<wchar_t>);
  return service_name;
}

REGSAM Wow6432(REGSAM access) {
  CHECK(access);

  return KEY_WOW64_32KEY | access;
}

HResultOr<DWORD> ShellExecuteAndWait(const base::FilePath& file_path,
                                     const std::wstring& parameters,
                                     const std::wstring& verb) {
  VLOG(1) << __func__ << ": path: " << file_path
          << ", parameters:" << parameters << ", verb:" << verb;
  CHECK(!file_path.empty());

  const HWND hwnd = CreateForegroundParentWindowForUAC();
  const absl::Cleanup destroy_window = [&] {
    if (hwnd) {
      ::DestroyWindow(hwnd);
    }
  };

  SHELLEXECUTEINFO shell_execute_info = {};
  shell_execute_info.cbSize = sizeof(SHELLEXECUTEINFO);
  shell_execute_info.fMask = SEE_MASK_FLAG_NO_UI | SEE_MASK_NOCLOSEPROCESS |
                             SEE_MASK_NOZONECHECKS | SEE_MASK_NOASYNC;
  shell_execute_info.hProcess = NULL;
  shell_execute_info.hwnd = hwnd;
  shell_execute_info.lpVerb = verb.c_str();
  shell_execute_info.lpFile = file_path.value().c_str();
  shell_execute_info.lpParameters = parameters.c_str();
  shell_execute_info.lpDirectory = NULL;
  shell_execute_info.nShow = SW_SHOW;
  shell_execute_info.hInstApp = NULL;

  if (!::ShellExecuteEx(&shell_execute_info)) {
    const HRESULT hr = HRESULTFromLastError();
    VLOG(1) << __func__ << ": ::ShellExecuteEx failed: " << std::hex << hr;
    return base::unexpected(hr);
  }

  if (!shell_execute_info.hProcess) {
    VLOG(1) << __func__ << ": Started process, PID unknown";
    return base::ok(0);
  }

  const base::Process process(shell_execute_info.hProcess);
  const DWORD pid = process.Pid();
  VLOG(1) << __func__ << ": Started process, PID: " << pid;

  // Allow the spawned process to show windows in the foreground.
  if (!::AllowSetForegroundWindow(pid)) {
    VLOG(1) << __func__
            << ": ::AllowSetForegroundWindow failed: " << ::GetLastError();
  }

  int ret_val = 0;
  if (!process.WaitForExit(&ret_val)) {
    return base::unexpected(HRESULTFromLastError());
  }

  return base::ok(static_cast<DWORD>(ret_val));
}

HResultOr<DWORD> RunElevated(const base::FilePath& file_path,
                             const std::wstring& parameters) {
  return ShellExecuteAndWait(file_path, parameters, L"runas");
}

HRESULT RunDeElevatedCmdLine(const std::wstring& cmd_line) {
  if (!IsElevatedWithUACOn()) {
    auto process = base::LaunchProcess(cmd_line, {});
    return process.IsValid() ? S_OK : HRESULTFromLastError();
  }

  // Custom processing is used here instead of using
  // `base::CommandLine::FromString` because this `cmd_line` string can have a
  // parameter format that is different from what is expected of
  // `base::CommandLine` parameters.
  std::wstring command_format = cmd_line;
  int num_args = 0;
  base::win::ScopedLocalAllocTyped<wchar_t*> argv(
      ::CommandLineToArgvW(&command_format[0], &num_args));
  if (!argv || num_args < 1) {
    LOG(ERROR) << __func__ << "!argv || num_args < 1: " << num_args;
    return E_INVALIDARG;
  }

  return base::win::RunDeElevatedNoWait(
      argv.get()[0],
      base::JoinString(
          [&]() -> std::vector<std::wstring> {
            if (num_args <= 1) {
              return {};
            }

            std::vector<std::wstring> parameters;
            base::ranges::for_each(
                argv.get() + 1, argv.get() + num_args,
                [&](const auto& parameter) {
                  parameters.push_back(
                      base::CommandLine::QuoteForCommandLineToArgvW(parameter));
                });
            return parameters;
          }(),
          L" "));
}

std::optional<base::FilePath> GetGoogleUpdateExePath(UpdaterScope scope) {
  base::FilePath goopdate_base_dir;
  if (!base::PathService::Get(IsSystemInstall(scope)
                                  ? base::DIR_PROGRAM_FILESX86
                                  : base::DIR_LOCAL_APP_DATA,
                              &goopdate_base_dir)) {
    LOG(ERROR) << "Can't retrieve GoogleUpdate base directory.";
    return std::nullopt;
  }

  return goopdate_base_dir.AppendASCII(COMPANY_SHORTNAME_STRING)
      .AppendASCII("Update")
      .Append(kLegacyExeName);
}

HRESULT DisableCOMExceptionHandling() {
  Microsoft::WRL::ComPtr<IGlobalOptions> options;
  HRESULT hr = ::CoCreateInstance(CLSID_GlobalOptions, nullptr,
                                  CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&options));
  if (FAILED(hr)) {
    return hr;
  }
  return hr = options->Set(COMGLB_EXCEPTION_HANDLING,
                           COMGLB_EXCEPTION_DONOT_HANDLE);
}

std::wstring BuildMsiCommandLine(
    const std::wstring& arguments,
    const std::optional<base::FilePath>& installer_data_file,
    const base::FilePath& msi_installer) {
  if (!msi_installer.MatchesExtension(L".msi")) {
    return std::wstring();
  }

  return base::StrCat(
      {L"msiexec ", arguments,
       installer_data_file
           ? base::StrCat(
                 {L" ",
                  base::UTF8ToWide(base::ToUpperASCII(kInstallerDataSwitch)),
                  L"=",
                  base::CommandLine::QuoteForCommandLineToArgvW(
                      installer_data_file->value())})
           : L"",
       L" REBOOT=ReallySuppress /qn /i ",
       base::CommandLine::QuoteForCommandLineToArgvW(msi_installer.value()),
       L" /log ",
       base::CommandLine::QuoteForCommandLineToArgvW(
           msi_installer.AddExtension(L".log").value())});
}

std::wstring BuildExeCommandLine(
    const std::wstring& arguments,
    const std::optional<base::FilePath>& installer_data_file,
    const base::FilePath& exe_installer) {
  if (!exe_installer.MatchesExtension(L".exe")) {
    return std::wstring();
  }

  return base::StrCat(
      {base::CommandLine::QuoteForCommandLineToArgvW(exe_installer.value()),
       L" ", arguments, [&installer_data_file] {
         if (!installer_data_file) {
           return std::wstring();
         }

         base::CommandLine installer_data_args(base::CommandLine::NO_PROGRAM);
         installer_data_args.AppendSwitchPath(kInstallerDataSwitch,
                                              *installer_data_file);
         return base::StrCat({L" ", installer_data_args.GetArgumentsString()});
       }()});
}

bool IsServiceRunning(const std::wstring& service_name) {
  ScopedScHandle scm(::OpenSCManager(nullptr, nullptr, SC_MANAGER_CONNECT));
  if (!scm.IsValid()) {
    LOG(ERROR) << "::OpenSCManager failed. service_name: " << service_name
               << ", error: " << std::hex << HRESULTFromLastError();
    return false;
  }

  ScopedScHandle service(
      ::OpenService(scm.Get(), service_name.c_str(), SERVICE_QUERY_STATUS));
  if (!service.IsValid()) {
    LOG(ERROR) << "::OpenService failed. service_name: " << service_name
               << ", error: " << std::hex << HRESULTFromLastError();
    return false;
  }

  SERVICE_STATUS status = {0};
  if (!::QueryServiceStatus(service.Get(), &status)) {
    LOG(ERROR) << "::QueryServiceStatus failed. service_name: " << service_name
               << ", error: " << std::hex << HRESULTFromLastError();
    return false;
  }

  VLOG(1) << "IsServiceRunning. service_name: " << service_name
          << ", status: " << std::hex << status.dwCurrentState;
  return status.dwCurrentState == SERVICE_RUNNING ||
         status.dwCurrentState == SERVICE_START_PENDING;
}

HKEY UpdaterScopeToHKeyRoot(UpdaterScope scope) {
  return IsSystemInstall(scope) ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
}

std::optional<OSVERSIONINFOEX> GetOSVersion() {
  // `::RtlGetVersion` is being used here instead of `::GetVersionEx`, because
  // the latter function can return the incorrect version if it is shimmed using
  // an app compat shim.
  using RtlGetVersion = LONG(WINAPI*)(OSVERSIONINFOEX*);
  static const RtlGetVersion rtl_get_version = reinterpret_cast<RtlGetVersion>(
      ::GetProcAddress(::GetModuleHandle(L"ntdll.dll"), "RtlGetVersion"));
  if (!rtl_get_version) {
    return std::nullopt;
  }

  OSVERSIONINFOEX os_out = {};
  os_out.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);

  rtl_get_version(&os_out);
  if (!os_out.dwMajorVersion) {
    return std::nullopt;
  }

  return os_out;
}

bool CompareOSVersions(const OSVERSIONINFOEX& os_version, BYTE oper) {
  CHECK(oper);

  constexpr DWORD kOSTypeMask = VER_MAJORVERSION | VER_MINORVERSION |
                                VER_SERVICEPACKMAJOR | VER_SERVICEPACKMINOR;
  constexpr DWORD kBuildTypeMask = VER_BUILDNUMBER;

  // If the OS and the service pack match, return the build number comparison.
  return CompareOSVersionsInternal(os_version, kOSTypeMask, VER_EQUAL)
             ? CompareOSVersionsInternal(os_version, kBuildTypeMask, oper)
             : CompareOSVersionsInternal(os_version, kOSTypeMask, oper);
}

bool EnableSecureDllLoading() {
  static const auto set_default_dll_directories =
      reinterpret_cast<decltype(&::SetDefaultDllDirectories)>(::GetProcAddress(
          ::GetModuleHandle(L"kernel32.dll"), "SetDefaultDllDirectories"));

  if (!set_default_dll_directories) {
    return true;
  }

#if defined(COMPONENT_BUILD)
  const DWORD directory_flags = LOAD_LIBRARY_SEARCH_DEFAULT_DIRS;
#else
  const DWORD directory_flags = LOAD_LIBRARY_SEARCH_SYSTEM32;
#endif

  return set_default_dll_directories(directory_flags);
}

bool EnableProcessHeapMetadataProtection() {
  if (!::HeapSetInformation(NULL, HeapEnableTerminationOnCorruption, nullptr,
                            0)) {
    LOG(ERROR) << __func__
               << ": Failed to enable heap metadata protection: " << std::hex
               << HRESULTFromLastError();
    return false;
  }

  return true;
}

std::optional<base::ScopedTempDir> CreateSecureTempDir() {
  // This function uses `base::CreateNewTempDirectory` and then a
  // `base::ScopedTempDir` as owner, instead of just
  // `base::ScopedTempDir::CreateUniqueTempDir`, because the former allows
  // setting a more recognizable prefix of `COMPANY_SHORTNAME_STRING` on the
  // temp directory.
  base::FilePath temp_dir;
  if (!base::CreateNewTempDirectory(FILE_PATH_LITERAL(COMPANY_SHORTNAME_STRING),
                                    &temp_dir)) {
    return std::nullopt;
  }

  base::ScopedTempDir temp_dir_owner;
  if (temp_dir_owner.Set(temp_dir)) {
    return temp_dir_owner;
  }
  return std::nullopt;
}

base::ScopedClosureRunner SignalShutdownEvent(UpdaterScope scope) {
  NamedObjectAttributes attr = GetNamedObjectAttributes(kShutdownEvent, scope);

  base::win::ScopedHandle shutdown_event_handle(
      ::CreateEvent(&attr.sa, true, false, attr.name.c_str()));
  if (!shutdown_event_handle.IsValid()) {
    VLOG(1) << __func__ << "Could not create the shutdown event: " << std::hex
            << HRESULTFromLastError();
    return {};
  }

  auto shutdown_event =
      std::make_unique<base::WaitableEvent>(std::move(shutdown_event_handle));
  shutdown_event->Signal();
  return base::ScopedClosureRunner(
      base::BindOnce(&base::WaitableEvent::Reset, std::move(shutdown_event)));
}

bool IsShutdownEventSignaled(UpdaterScope scope) {
  NamedObjectAttributes attr = GetNamedObjectAttributes(kShutdownEvent, scope);

  base::win::ScopedHandle event_handle(
      ::OpenEvent(EVENT_ALL_ACCESS, false, attr.name.c_str()));
  if (!event_handle.IsValid()) {
    return false;
  }

  base::WaitableEvent event(std::move(event_handle));
  return event.IsSignaled();
}

void StopProcessesUnderPath(const base::FilePath& path,
                            base::TimeDelta wait_period) {
  // Filters processes running under `path_prefix`.
  class PathPrefixProcessFilter : public base::ProcessFilter {
   public:
    explicit PathPrefixProcessFilter(const base::FilePath& path_prefix)
        : path_prefix_(path_prefix) {}

    bool Includes(const base::ProcessEntry& entry) const override {
      base::Process process(::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,
                                          false, entry.th32ProcessID));
      if (!process.IsValid()) {
        return false;
      }

      DWORD path_len = MAX_PATH;
      wchar_t path_string[MAX_PATH];
      if (!::QueryFullProcessImageName(process.Handle(), 0, path_string,
                                       &path_len)) {
        return false;
      }

      return path_prefix_.IsParent(base::FilePath(path_string));
    }

   private:
    const base::FilePath path_prefix_;
  };

  PathPrefixProcessFilter path_prefix_filter(path);

  base::ProcessIterator iter(&path_prefix_filter);
  base::flat_set<std::wstring> process_names_to_cleanup;
  for (const base::ProcessEntry* entry = iter.NextProcessEntry(); entry;
       entry = iter.NextProcessEntry()) {
    process_names_to_cleanup.insert(entry->exe_file());
  }

  const auto deadline = base::TimeTicks::Now() + wait_period;
  for (const auto& exe_file : process_names_to_cleanup) {
    base::CleanupProcesses(
        exe_file, std::max(deadline - base::TimeTicks::Now(), base::Seconds(0)),
        -1, &path_prefix_filter);
  }
}

std::optional<base::CommandLine> CommandLineForLegacyFormat(
    const std::wstring& cmd_string) {
  int num_args = 0;
  base::win::ScopedLocalAllocTyped<wchar_t*> args(
      ::CommandLineToArgvW(cmd_string.c_str(), &num_args));
  if (!args) {
    return std::nullopt;
  }

  auto is_switch = [](const std::wstring& arg) { return arg[0] == L'-'; };

  auto is_legacy_switch = [](const std::wstring& arg) {
    return arg[0] == L'/';
  };

  // First argument is the program.
  base::CommandLine command_line(base::FilePath{args.get()[0]});

  for (int i = 1; i < num_args; ++i) {
    const std::wstring next_arg = i < num_args - 1 ? args.get()[i + 1] : L"";

    if (is_switch(args.get()[i]) || is_switch(next_arg)) {
      // Won't parse Chromium-style command line.
      return std::nullopt;
    }

    if (!is_legacy_switch(args.get()[i])) {
      // This is a bare argument.
      command_line.AppendArg(base::WideToASCII(args.get()[i]));
      continue;
    }

    const std::string switch_name = base::WideToASCII(&args.get()[i][1]);
    if (switch_name.empty()) {
      VLOG(1) << "Empty switch in command line: [" << cmd_string << "]";
      return std::nullopt;
    }
    if (base::StringPairs switch_value_pairs;
        base::SplitStringIntoKeyValuePairs(switch_name, '=', '\n',
                                           &switch_value_pairs)) {
      command_line.AppendSwitchASCII(switch_value_pairs[0].first,
                                     switch_value_pairs[0].second);
      continue;
    }
    if (is_legacy_switch(next_arg) || next_arg.empty()) {
      command_line.AppendSwitch(switch_name);
    } else {
      // Next argument is the value for this switch.
      command_line.AppendSwitchNative(switch_name, next_arg);
      ++i;
    }
  }

  return command_line;
}

std::optional<base::FilePath> GetInstallDirectory(UpdaterScope scope) {
  base::FilePath app_data_dir;
  if (!base::PathService::Get(IsSystemInstall(scope) ? base::DIR_PROGRAM_FILES
                                                     : base::DIR_LOCAL_APP_DATA,
                              &app_data_dir)) {
    LOG(ERROR) << "Can't retrieve app data directory.";
    return std::nullopt;
  }
  return app_data_dir.AppendASCII(COMPANY_SHORTNAME_STRING)
      .AppendASCII(PRODUCT_FULLNAME_STRING);
}

base::FilePath GetExecutableRelativePath() {
  return base::FilePath::FromASCII(kExecutableName);
}

bool IsGuid(const std::wstring& s) {
  CHECK(!s.empty());

  GUID guid = {0};
  return SUCCEEDED(::IIDFromString(&s[0], &guid));
}

void ForEachRegistryRunValueWithPrefix(
    const std::wstring& prefix,
    base::FunctionRef<void(const std::wstring&)> callback) {
  for (base::win::RegistryValueIterator it(HKEY_CURRENT_USER, REGSTR_PATH_RUN,
                                           KEY_WOW64_32KEY);
       it.Valid(); ++it) {
    const std::wstring run_name = it.Name();
    if (base::StartsWith(run_name, prefix)) {
      callback(run_name);
    }
  }
}

[[nodiscard]] bool DeleteRegValue(HKEY root,
                                  const std::wstring& path,
                                  const std::wstring& value) {
  if (!base::win::RegKey(root, path.c_str(), Wow6432(KEY_QUERY_VALUE))
           .Valid()) {
    return true;
  }

  LONG result = base::win::RegKey(root, path.c_str(), Wow6432(KEY_WRITE))
                    .DeleteValue(value.c_str());
  return result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND;
}

void ForEachServiceWithPrefix(
    const std::wstring& service_name_prefix,
    const std::wstring& display_name_prefix,
    base::FunctionRef<void(const std::wstring&)> callback) {
  for (base::win::RegistryKeyIterator it(HKEY_LOCAL_MACHINE,
                                         L"SYSTEM\\CurrentControlSet\\Services",
                                         KEY_WOW64_32KEY);
       it.Valid(); ++it) {
    const std::wstring service_name = it.Name();
    if (base::StartsWith(service_name, service_name_prefix)) {
      if (display_name_prefix.empty()) {
        callback(service_name);
        continue;
      }

      base::win::RegKey key;
      if (key.Open(HKEY_LOCAL_MACHINE,
                   base::StrCat(
                       {L"SYSTEM\\CurrentControlSet\\Services\\", service_name})
                       .c_str(),
                   Wow6432(KEY_READ)) != ERROR_SUCCESS) {
        continue;
      }

      std::wstring display_name;
      if (key.ReadValue(L"DisplayName", &display_name) != ERROR_SUCCESS) {
        continue;
      }

      const bool display_name_starts_with_prefix =
          base::StartsWith(display_name, display_name_prefix);
      VLOG(1) << __func__ << ": " << service_name
              << " matches: " << service_name_prefix << ": " << display_name
              << ": " << display_name_starts_with_prefix << ": "
              << display_name_prefix;
      if (display_name_starts_with_prefix) {
        callback(service_name);
      }
    }
  }
}

[[nodiscard]] bool DeleteService(const std::wstring& service_name) {
  ScopedScHandle scm(::OpenSCManager(
      nullptr, nullptr, SC_MANAGER_CONNECT | SC_MANAGER_CREATE_SERVICE));
  if (!scm.IsValid()) {
    return false;
  }

  ScopedScHandle service(
      ::OpenService(scm.Get(), service_name.c_str(), DELETE));
  bool is_service_deleted = !service.IsValid();
  if (!is_service_deleted) {
    is_service_deleted =
        ::DeleteService(service.Get())
            ? true
            : ::GetLastError() == ERROR_SERVICE_MARKED_FOR_DELETE;
  }

  if (!DeleteRegValue(HKEY_LOCAL_MACHINE, UPDATER_KEY, service_name)) {
    return false;
  }

  VLOG(1) << __func__ << ": " << service_name << ": " << is_service_deleted;
  return is_service_deleted;
}

bool WrongUser(UpdaterScope scope) {
  return IsSystemInstall(scope) ? !::IsUserAnAdmin()
                                : ::IsUserAnAdmin() && IsUACOn();
}

bool EulaAccepted(const std::vector<std::string>& app_ids) {
  for (const auto& app_id : app_ids) {
    DWORD eula_accepted = 0;
    if (base::win::RegKey(
            HKEY_LOCAL_MACHINE,
            base::StrCat({CLIENT_STATE_MEDIUM_KEY, base::ASCIIToWide(app_id)})
                .c_str(),
            Wow6432(KEY_READ))
                .ReadValueDW(L"eulaaccepted", &eula_accepted) ==
            ERROR_SUCCESS &&
        eula_accepted == 1) {
      return true;
    }
  }
  return false;
}

void LogClsidEntries(REFCLSID clsid) {
  const std::wstring local_server32_reg_path(base::StrCat(
      {base::StrCat({L"Software\\Classes\\CLSID\\", StringFromGuid(clsid)}),
       L"\\LocalServer32"}));
  for (const HKEY root : {HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER}) {
    for (const REGSAM key_flag : {KEY_WOW64_32KEY, KEY_WOW64_64KEY}) {
      base::win::RegKey key;
      if (ERROR_SUCCESS == key.Open(root, local_server32_reg_path.c_str(),
                                    KEY_QUERY_VALUE | key_flag)) {
        std::wstring val;
        if (ERROR_SUCCESS == key.ReadValue(L"", &val)) {
          LOG(ERROR) << __func__ << ": CLSID entry found: "
                     << (root == HKEY_LOCAL_MACHINE ? "HKEY_LOCAL_MACHINE\\"
                                                    : "HKEY_CURRENT_USER\\")
                     << local_server32_reg_path << ": " << val;
        }
      }
    }
  }
}

std::optional<base::FilePath> GetInstallDirectoryX86(UpdaterScope scope) {
  if (!IsSystemInstall(scope)) {
    return GetInstallDirectory(scope);
  }
  base::FilePath install_dir;
  if (!base::PathService::Get(base::DIR_PROGRAM_FILESX86, &install_dir)) {
    LOG(ERROR) << "Can't retrieve directory for DIR_PROGRAM_FILESX86.";
    return std::nullopt;
  }
  return install_dir.AppendASCII(COMPANY_SHORTNAME_STRING)
      .AppendASCII(PRODUCT_FULLNAME_STRING);
}

std::optional<std::wstring> GetRegKeyContents(const std::wstring& reg_key) {
  base::FilePath system_path;
  if (!base::PathService::Get(base::DIR_SYSTEM, &system_path)) {
    return {};
  }

  std::string output;
  if (!base::GetAppOutput(
          base::StrCat({system_path.Append(L"reg.exe").value(), L" query ",
                        base::CommandLine::QuoteForCommandLineToArgvW(reg_key),
                        L" /s"}),
          &output)) {
    return {};
  }
  return base::ASCIIToWide(output);
}

std::wstring GetTextForSystemError(int error) {
  if (static_cast<HRESULT>(error & 0xFFFF0000) ==
      MAKE_HRESULT(SEVERITY_ERROR, FACILITY_WIN32, 0)) {
    error = HRESULT_CODE(error);
  }

  HMODULE source = nullptr;
  DWORD format_options =
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
      FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_MAX_WIDTH_MASK;

  if (error >= WINHTTP_ERROR_BASE && error <= WINHTTP_ERROR_LAST) {
    source = ::GetModuleHandle(_T("winhttp.dll"));
    if (source) {
      format_options |= FORMAT_MESSAGE_FROM_HMODULE;
    }
  }
  wchar_t* system_allocated_buffer = nullptr;
  const DWORD chars_written = ::FormatMessage(
      format_options, source, error, 0,
      reinterpret_cast<wchar_t*>(&system_allocated_buffer), 0, nullptr);
  base::win::ScopedLocalAllocTyped<wchar_t> free_buffer(
      system_allocated_buffer);
  return chars_written > 0
             ? system_allocated_buffer
             : base::ASCIIToWide(base::StringPrintf("%#x", error));
}

bool MigrateLegacyUpdaters(
    UpdaterScope scope,
    base::RepeatingCallback<void(const RegistrationRequest&)>
        register_callback) {
  const HKEY root = UpdaterScopeToHKeyRoot(scope);
  for (base::win::RegistryKeyIterator it(root, CLIENTS_KEY, KEY_WOW64_32KEY);
       it.Valid(); ++it) {
    const std::wstring app_id = it.Name();

    // Skip importing legacy updater.
    if (base::EqualsCaseInsensitiveASCII(app_id, kLegacyGoogleUpdateAppID)) {
      continue;
    }

    base::win::RegKey key;
    if (key.Open(root, GetAppClientsKey(app_id).c_str(), Wow6432(KEY_READ)) !=
        ERROR_SUCCESS) {
      continue;
    }

    RegistrationRequest registration;
    registration.app_id = base::SysWideToUTF8(app_id);
    std::wstring pv;
    if (key.ReadValue(kRegValuePV, &pv) != ERROR_SUCCESS) {
      continue;
    }

    registration.version = base::Version(base::SysWideToUTF8(pv));
    if (!registration.version.IsValid()) {
      continue;
    }

    base::win::RegKey client_state_key;
    if (client_state_key.Open(root, GetAppClientStateKey(app_id).c_str(),
                              Wow6432(KEY_READ)) == ERROR_SUCCESS) {
      std::wstring brand_code;
      if (client_state_key.ReadValue(kRegValueBrandCode, &brand_code) ==
          ERROR_SUCCESS) {
        registration.brand_code = base::SysWideToUTF8(brand_code);
      }

      std::wstring ap;
      if (client_state_key.ReadValue(kRegValueAP, &ap) == ERROR_SUCCESS) {
        registration.ap = base::SysWideToUTF8(ap);
      }

      DWORD date_last_activity = 0;
      if (client_state_key.ReadValueDW(kRegValueDateOfLastActivity,
                                       &date_last_activity) == ERROR_SUCCESS) {
        registration.dla = DaynumFromDWORD(date_last_activity);
      }

      DWORD date_last_rollcall = 0;
      if (client_state_key.ReadValueDW(kRegValueDateOfLastRollcall,
                                       &date_last_rollcall) == ERROR_SUCCESS) {
        registration.dlrc = DaynumFromDWORD(date_last_rollcall);
      }

      DWORD install_date = 0;
      if (client_state_key.ReadValueDW(kRegValueDayOfInstall, &install_date) ==
          ERROR_SUCCESS) {
        registration.install_date = DaynumFromDWORD(install_date);
      }

      base::win::RegKey cohort_key;
      if (cohort_key.Open(root, GetAppCohortKey(app_id).c_str(),
                          Wow6432(KEY_READ)) == ERROR_SUCCESS) {
        std::wstring cohort;
        if (cohort_key.ReadValue(nullptr, &cohort) == ERROR_SUCCESS) {
          registration.cohort = base::SysWideToUTF8(cohort);

          std::wstring cohort_name;
          if (cohort_key.ReadValue(kRegValueCohortName, &cohort_name) ==
              ERROR_SUCCESS) {
            registration.cohort_name = base::SysWideToUTF8(cohort_name);
          }

          std::wstring cohort_hint;
          if (cohort_key.ReadValue(kRegValueCohortHint, &cohort_hint) ==
              ERROR_SUCCESS) {
            registration.cohort_hint = base::SysWideToUTF8(cohort_hint);
          }
          VLOG(2) << "Cohort values: " << registration.cohort << ", "
                  << registration.cohort_name << ", "
                  << registration.cohort_hint;
        }
      }
    }

    register_callback.Run(registration);
  }

  return true;
}

namespace {

struct ScopedWtsConnectStateCloseTraits {
  static WTS_CONNECTSTATE_CLASS* InvalidValue() { return nullptr; }
  static void Free(WTS_CONNECTSTATE_CLASS* memory) { ::WTSFreeMemory(memory); }
};

struct ScopedWtsSessionInfoCloseTraits {
  static PWTS_SESSION_INFO InvalidValue() { return nullptr; }
  static void Free(PWTS_SESSION_INFO memory) { ::WTSFreeMemory(memory); }
};

using ScopedWtsConnectState =
    base::ScopedGeneric<WTS_CONNECTSTATE_CLASS*,
                        ScopedWtsConnectStateCloseTraits>;
using ScopedWtsSessionInfo =
    base::ScopedGeneric<PWTS_SESSION_INFO, ScopedWtsSessionInfoCloseTraits>;

// Returns `true` if there is a user logged on and active in the specified
// session.
bool IsSessionActive(std::optional<DWORD> session_id) {
  if (!session_id) {
    return false;
  }

  ScopedWtsConnectState wts_connect_state;
  DWORD bytes_returned = 0;
  if (::WTSQuerySessionInformation(
          WTS_CURRENT_SERVER_HANDLE, *session_id, WTSConnectState,
          reinterpret_cast<LPTSTR*>(
              ScopedWtsConnectState::Receiver(wts_connect_state).get()),
          &bytes_returned)) {
    CHECK_EQ(bytes_returned, sizeof(WTS_CONNECTSTATE_CLASS));
    return *wts_connect_state.get() == WTSActive;
  }

  return false;
}

// Returns the currently active session.
// `WTSGetActiveConsoleSessionId` retrieves the Terminal Services session
// currently attached to the physical console, so that is attempted first.
// `WTSGetActiveConsoleSessionId` does not work for terminal servers where the
// current active session is always the console. For those, an active session
// is found by enumerating all the sessions that are present on the system, and
// the first active session is returned.
std::optional<DWORD> GetActiveSessionId() {
  if (DWORD active_session_id = ::WTSGetActiveConsoleSessionId();
      IsSessionActive(active_session_id)) {
    return active_session_id;
  }

  ScopedWtsSessionInfo session_info;
  DWORD num_sessions = 0;
  if (::WTSEnumerateSessions(WTS_CURRENT_SERVER_HANDLE, 0, 1,
                             ScopedWtsSessionInfo::Receiver(session_info).get(),
                             &num_sessions)) {
    for (size_t i = 0; i < num_sessions; ++i) {
      if (session_info.get()[i].State == WTSActive) {
        return session_info.get()[i].SessionId;
      }
    }
  }

  return {};
}

std::vector<DWORD> FindProcesses(const std::wstring& process_name) {
  base::NamedProcessIterator iter(process_name, nullptr);
  std::vector<DWORD> pids;
  while (const base::ProcessEntry* process_entry = iter.NextProcessEntry()) {
    pids.push_back(process_entry->pid());
  }
  return pids;
}

// Returns processes running under `session_id`.
std::vector<DWORD> FindProcessesInSession(const std::wstring& process_name,
                                          std::optional<DWORD> session_id) {
  if (!session_id) {
    return {};
  }
  std::vector<DWORD> pids;
  for (const auto pid : FindProcesses(process_name)) {
    DWORD process_session = 0;
    if (::ProcessIdToSessionId(pid, &process_session) &&
        (process_session == *session_id)) {
      pids.push_back(pid);
    }
  }
  return pids;
}

// Returns the first instance found of explorer.exe.
std::optional<DWORD> GetExplorerPid() {
  std::vector<DWORD> pids =
      FindProcessesInSession(L"EXPLORER.EXE", GetActiveSessionId());
  if (pids.empty()) {
    return {};
  }
  return pids[0];
}

// Returns an impersonation token for the user running process_id.
HResultOr<ScopedKernelHANDLE> GetImpersonationToken(
    std::optional<DWORD> process_id) {
  if (!process_id) {
    return base::unexpected(E_UNEXPECTED);
  }
  base::win::ScopedHandle process(::OpenProcess(
      PROCESS_DUP_HANDLE | PROCESS_QUERY_INFORMATION, TRUE, *process_id));
  if (!process.IsValid()) {
    return base::unexpected(HRESULTFromLastError());
  }
  ScopedKernelHANDLE process_token;
  if (!::OpenProcessToken(process.Get(), TOKEN_DUPLICATE | TOKEN_QUERY,
                          ScopedKernelHANDLE::Receiver(process_token).get())) {
    return base::unexpected(HRESULTFromLastError());
  }
  ScopedKernelHANDLE user_token;
  if (!::DuplicateTokenEx(process_token.get(),
                          TOKEN_IMPERSONATE | TOKEN_QUERY |
                              TOKEN_ASSIGN_PRIMARY | TOKEN_DUPLICATE,
                          NULL, SecurityImpersonation, TokenPrimary,
                          ScopedKernelHANDLE::Receiver(user_token).get())) {
    return base::unexpected(HRESULTFromLastError());
  }
  return user_token;
}

}  // namespace

HResultOr<ScopedKernelHANDLE> GetLoggedOnUserToken() {
  return GetImpersonationToken(GetExplorerPid());
}

bool IsAuditMode() {
  base::win::RegKey setup_state_key;
  std::wstring state;
  return setup_state_key.Open(HKEY_LOCAL_MACHINE, kSetupStateKey,
                              KEY_QUERY_VALUE) == ERROR_SUCCESS &&
         setup_state_key.ReadValue(kImageStateValueName, &state) ==
             ERROR_SUCCESS &&
         (base::EqualsCaseInsensitiveASCII(state, kImageStateUnuseableValue) ||
          base::EqualsCaseInsensitiveASCII(state,
                                           kImageStateGeneralAuditValue) ||
          base::EqualsCaseInsensitiveASCII(state,
                                           kImageStateSpecialAuditValue));
}

bool SetOemInstallState() {
  if (!::IsUserAnAdmin() || !IsAuditMode()) {
    return false;
  }

  const base::Time now = base::Time::Now();
  VLOG(1) << "OEM install time set: " << now;
  return base::win::RegKey(HKEY_LOCAL_MACHINE, CLIENTS_KEY,
                           Wow6432(KEY_SET_VALUE))
             .WriteValue(kRegValueOemInstallTimeMin,
                         now.ToDeltaSinceWindowsEpoch().InMinutes()) ==
         ERROR_SUCCESS;
}

bool ResetOemInstallState() {
  VLOG(1) << "OEM install reset at time: " << base::Time::Now();
  const LONG result =
      base::win::RegKey(HKEY_LOCAL_MACHINE, CLIENTS_KEY, Wow6432(KEY_SET_VALUE))
          .DeleteValue(kRegValueOemInstallTimeMin);
  return result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND;
}

bool IsOemInstalling() {
  DWORD oem_install_time_minutes = 0;
  if (base::win::RegKey(HKEY_LOCAL_MACHINE, CLIENTS_KEY,
                        Wow6432(KEY_QUERY_VALUE))
          .ReadValueDW(kRegValueOemInstallTimeMin, &oem_install_time_minutes) !=
      ERROR_SUCCESS) {
    VLOG(2) << "OemInstallTime not found";
    return false;
  }
  const base::Time now = base::Time::Now();
  const base::Time oem_install_time = base::Time::FromDeltaSinceWindowsEpoch(
      base::Minutes(oem_install_time_minutes));
  const base::TimeDelta time_in_oem_mode = now - oem_install_time;
  const bool is_oem_installing = time_in_oem_mode < kMinOemModeTime;
  if (!is_oem_installing) {
    ResetOemInstallState();
  }
  VLOG(1) << "now: " << now << ", OEM install time: " << oem_install_time
          << ", time_in_oem_mode: " << time_in_oem_mode
          << ", is_oem_installing: " << is_oem_installing;
  return is_oem_installing;
}

std::wstring StringFromGuid(const GUID& guid) {
  // {xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}
  constexpr int kGuidStringCharacters =
      1 + 8 + 1 + 4 + 1 + 4 + 1 + 4 + 1 + 12 + 1 + 1;
  wchar_t guid_string[kGuidStringCharacters] = {0};
  CHECK_NE(::StringFromGUID2(guid, guid_string, kGuidStringCharacters), 0);
  return guid_string;
}

bool StoreRunTimeEnrollmentToken(const std::string& enrollment_token) {
  VLOG(1) << __func__ << ": " << enrollment_token;
  return base::win::RegKey(HKEY_LOCAL_MACHINE,
                           GetAppClientsKey(kUpdaterAppId).c_str(),
                           Wow6432(KEY_SET_VALUE))
             .WriteValue(kRegValueCloudManagementEnrollmentToken,
                         base::SysUTF8ToWide(enrollment_token).c_str()) ==
         ERROR_SUCCESS;
}

std::optional<base::FilePath> GetUniqueTempFilePath(base::FilePath file) {
  base::FilePath temp_dir;
  if (file.empty() || !base::GetTempDir(&temp_dir)) {
    return {};
  }
  return temp_dir.Append(base::StrCat(
      {file.RemoveExtension().BaseName().value(),
       base::UTF8ToWide(base::Uuid::GenerateRandomV4().AsLowercaseString()),
       file.Extension()}));
}

std::optional<base::FilePath> GetBundledEnterpriseCompanionExecutablePath(
    UpdaterScope scope) {
  std::optional<base::FilePath> install_dir =
      GetVersionedInstallDirectory(scope);
  if (!install_dir) {
    return std::nullopt;
  }

  return install_dir->AppendASCII(
      base::StrCat({base::FilePath::FromASCII(kCompanionAppExecutableName)
                        .RemoveExtension()
                        .MaybeAsASCII(),
                    kExecutableSuffix, ".exe"}));
}

}  // namespace updater
