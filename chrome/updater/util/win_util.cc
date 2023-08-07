// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/util/win_util.h"

#include <aclapi.h>
#include <objidl.h>
#include <regstr.h>
#include <shellapi.h>
#include <shlobj.h>
#include <windows.h>
#include <wrl/client.h>

#include <algorithm>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#include "base/base_paths_win.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/containers/cxx20_erase.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback_helpers.h"
#include "base/functional/function_ref.h"
#include "base/logging.h"
#include "base/memory/free_deleter.h"
#include "base/path_service.h"
#include "base/process/kill.h"
#include "base/process/process.h"
#include "base/process/process_iterator.h"
#include "base/ranges/algorithm.h"
#include "base/scoped_native_library.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/system/sys_info.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "base/win/atl.h"
#include "base/win/registry.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_handle.h"
#include "base/win/scoped_localalloc.h"
#include "base/win/scoped_process_information.h"
#include "base/win/scoped_variant.h"
#include "base/win/startup_information.h"
#include "base/win/win_util.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/updater_version.h"
#include "chrome/updater/util/util.h"
#include "chrome/updater/win/scoped_handle.h"
#include "chrome/updater/win/user_info.h"
#include "chrome/updater/win/win_constants.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace updater {

namespace {

HResultOr<bool> IsUserRunningSplitToken() {
  HANDLE token = NULL;
  if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &token))
    return base::unexpected(HRESULTFromLastError());
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
  if (!::IsValidSid(sid))
    return E_FAIL;
  SID_IDENTIFIER_AUTHORITY* authority = ::GetSidIdentifierAuthority(sid);
  if (!authority)
    return E_FAIL;
  constexpr SID_IDENTIFIER_AUTHORITY kMandatoryLabelAuth =
      SECURITY_MANDATORY_LABEL_AUTHORITY;
  if (std::memcmp(authority, &kMandatoryLabelAuth,
                  sizeof(SID_IDENTIFIER_AUTHORITY))) {
    return E_FAIL;
  }
  PUCHAR count = ::GetSidSubAuthorityCount(sid);
  if (!count || *count != 1)
    return E_FAIL;
  DWORD* rid = ::GetSidSubAuthority(sid, 0);
  if (!rid)
    return E_FAIL;
  if ((*rid & 0xFFF) != 0 || *rid > SECURITY_MANDATORY_PROTECTED_PROCESS_RID)
    return E_FAIL;
  *level = static_cast<MANDATORY_LEVEL>(*rid >> 12);
  return S_OK;
}

// Gets the mandatory integrity level of a process.
HRESULT GetProcessIntegrityLevel(DWORD process_id, MANDATORY_LEVEL* level) {
  HANDLE process = ::OpenProcess(PROCESS_QUERY_INFORMATION, false, process_id);
  if (!process)
    return HRESULTFromLastError();
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

absl::optional<CSecurityDesc> GetCurrentUserDefaultSecurityDescriptor() {
  CAccessToken token;
  if (!token.GetProcessToken(TOKEN_QUERY))
    return absl::nullopt;

  CSecurityDesc security_desc;
  CSid sid_owner;
  if (!token.GetOwner(&sid_owner))
    return absl::nullopt;

  security_desc.SetOwner(sid_owner);
  CSid sid_group;
  if (!token.GetPrimaryGroup(&sid_group))
    return absl::nullopt;

  security_desc.SetGroup(sid_group);

  CDacl dacl;
  if (!token.GetDefaultDacl(&dacl))
    return absl::nullopt;

  CSid sid_user;
  if (!token.GetUser(&sid_user))
    return absl::nullopt;
  if (!dacl.AddAllowedAce(sid_user, GENERIC_ALL))
    return absl::nullopt;

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
  return base::StrCat({COHORT_KEY, app_id});
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

int GetDownloadProgress(int64_t downloaded_bytes, int64_t total_bytes) {
  if (downloaded_bytes == -1 || total_bytes == -1 || total_bytes == 0)
    return -1;
  CHECK_LE(downloaded_bytes, total_bytes);
  return 100 * std::clamp(static_cast<double>(downloaded_bytes) / total_bytes,
                           0.0, 1.0);
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
  if (!::CheckTokenMembership(token, administrators_group, &is_member))
    return base::unexpected(HRESULTFromLastError());
  return base::ok(is_member);
}

HResultOr<bool> IsUserAdmin() {
  return IsTokenAdmin(NULL);
}

HResultOr<bool> IsUserNonElevatedAdmin() {
  HANDLE token = NULL;
  if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_READ, &token))
    return base::unexpected(HRESULTFromLastError());
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
  HRESULT hr = ::CoImpersonateClient();
  if (hr == RPC_E_CALL_COMPLETE) {
    // RPC_E_CALL_COMPLETE indicates that the caller is in-proc.
    return base::ok(::IsUserAnAdmin());
  }

  if (FAILED(hr)) {
    return base::unexpected(hr);
  }

  HResultOr<ScopedKernelHANDLE> token = []() -> decltype(token) {
    ScopedKernelHANDLE token;
    absl::Cleanup co_revert_to_self = [] { ::CoRevertToSelf(); };
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
  if (is_user_admin.has_value())
    base::StringAppendF(&s, "IsUserAdmin: %d, ", is_user_admin.value());

  HResultOr<bool> is_user_non_elevated_admin = IsUserNonElevatedAdmin();
  if (is_user_non_elevated_admin.has_value()) {
    base::StringAppendF(&s, "IsUserNonElevatedAdmin: %d, ",
                        is_user_non_elevated_admin.value());
  }

  base::StringAppendF(&s, "IsUACOn: %d, IsElevatedWithUACOn: %d", IsUACOn(),
                      IsElevatedWithUACOn());
  return s;
}

std::wstring GetServiceName(bool is_internal_service) {
  std::wstring service_name = GetServiceDisplayName(is_internal_service);
  base::EraseIf(service_name, base::IsAsciiWhitespace<wchar_t>);
  return service_name;
}

std::wstring GetServiceDisplayName(bool is_internal_service) {
  return base::StrCat(
      {base::ASCIIToWide(PRODUCT_FULLNAME_STRING), L" ",
       is_internal_service ? kWindowsInternalServiceName : kWindowsServiceName,
       L" ", kUpdaterVersionUtf16});
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
    LOG(WARNING) << __func__
                 << ": ::AllowSetForegroundWindow failed: " << ::GetLastError();
  }

  int ret_val = 0;
  if (!process.WaitForExit(&ret_val))
    return base::unexpected(HRESULTFromLastError());

  return base::ok(static_cast<DWORD>(ret_val));
}

HResultOr<DWORD> RunElevated(const base::FilePath& file_path,
                             const std::wstring& parameters) {
  return ShellExecuteAndWait(file_path, parameters, L"runas");
}

HRESULT RunDeElevated(const std::wstring& path,
                      const std::wstring& parameters) {
  Microsoft::WRL::ComPtr<IShellWindows> shell;
  HRESULT hr = ::CoCreateInstance(CLSID_ShellWindows, nullptr,
                                  CLSCTX_LOCAL_SERVER, IID_PPV_ARGS(&shell));
  if (FAILED(hr))
    return hr;

  long hwnd = 0;
  Microsoft::WRL::ComPtr<IDispatch> dispatch;
  hr = shell->FindWindowSW(base::win::ScopedVariant(CSIDL_DESKTOP).AsInput(),
                           base::win::ScopedVariant().AsInput(), SWC_DESKTOP,
                           &hwnd, SWFO_NEEDDISPATCH, &dispatch);
  if (hr == S_FALSE || FAILED(hr)) {
    return hr == S_FALSE ? E_FAIL : hr;
  }

  Microsoft::WRL::ComPtr<IServiceProvider> service;
  hr = dispatch.As(&service);
  if (FAILED(hr))
    return hr;

  Microsoft::WRL::ComPtr<IShellBrowser> browser;
  hr = service->QueryService(SID_STopLevelBrowser, IID_PPV_ARGS(&browser));
  if (FAILED(hr))
    return hr;

  Microsoft::WRL::ComPtr<IShellView> view;
  hr = browser->QueryActiveShellView(&view);
  if (FAILED(hr))
    return hr;

  hr = view->GetItemObject(SVGIO_BACKGROUND, IID_PPV_ARGS(&dispatch));
  if (FAILED(hr))
    return hr;

  Microsoft::WRL::ComPtr<IShellFolderViewDual> folder;
  hr = dispatch.As(&folder);
  if (FAILED(hr))
    return hr;

  hr = folder->get_Application(&dispatch);
  if (FAILED(hr))
    return hr;

  Microsoft::WRL::ComPtr<IShellDispatch2> shell_dispatch;
  hr = dispatch.As(&shell_dispatch);
  if (FAILED(hr))
    return hr;

  return shell_dispatch->ShellExecute(
      base::win::ScopedBstr(path).Get(),
      base::win::ScopedVariant(parameters.c_str()),
      base::win::ScopedVariant::kEmptyVariant,
      base::win::ScopedVariant::kEmptyVariant,
      base::win::ScopedVariant::kEmptyVariant);
}

absl::optional<base::FilePath> GetGoogleUpdateExePath(UpdaterScope scope) {
  base::FilePath goopdate_base_dir;
  if (!base::PathService::Get(IsSystemInstall(scope)
                                  ? base::DIR_PROGRAM_FILESX86
                                  : base::DIR_LOCAL_APP_DATA,
                              &goopdate_base_dir)) {
    LOG(ERROR) << "Can't retrieve GoogleUpdate base directory.";
    return absl::nullopt;
  }

  return goopdate_base_dir.AppendASCII(COMPANY_SHORTNAME_STRING)
      .AppendASCII("Update")
      .Append(kLegacyExeName);
}

HRESULT DisableCOMExceptionHandling() {
  Microsoft::WRL::ComPtr<IGlobalOptions> options;
  HRESULT hr = ::CoCreateInstance(CLSID_GlobalOptions, nullptr,
                                  CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&options));
  if (FAILED(hr))
    return hr;
  return hr = options->Set(COMGLB_EXCEPTION_HANDLING,
                           COMGLB_EXCEPTION_DONOT_HANDLE);
}

std::wstring BuildMsiCommandLine(
    const std::wstring& arguments,
    const absl::optional<base::FilePath>& installer_data_file,
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
    const absl::optional<base::FilePath>& installer_data_file,
    const base::FilePath& exe_installer) {
  if (!exe_installer.MatchesExtension(L".exe")) {
    return std::wstring();
  }

  return base::StrCat(
      {base::CommandLine::QuoteForCommandLineToArgvW(exe_installer.value()),
       L" ", arguments, [&installer_data_file]() {
         if (!installer_data_file)
           return std::wstring();

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

absl::optional<OSVERSIONINFOEX> GetOSVersion() {
  // `::RtlGetVersion` is being used here instead of `::GetVersionEx`, because
  // the latter function can return the incorrect version if it is shimmed using
  // an app compat shim.
  using RtlGetVersion = LONG(WINAPI*)(OSVERSIONINFOEX*);
  static const RtlGetVersion rtl_get_version = reinterpret_cast<RtlGetVersion>(
      ::GetProcAddress(::GetModuleHandle(L"ntdll.dll"), "RtlGetVersion"));
  if (!rtl_get_version)
    return absl::nullopt;

  OSVERSIONINFOEX os_out = {};
  os_out.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);

  rtl_get_version(&os_out);
  if (!os_out.dwMajorVersion)
    return absl::nullopt;

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

  if (!set_default_dll_directories)
    return true;

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

absl::optional<base::ScopedTempDir> CreateSecureTempDir() {
  base::FilePath temp_dir;
  if (!base::CreateNewTempDirectory(FILE_PATH_LITERAL(COMPANY_SHORTNAME_STRING),
                                    &temp_dir)) {
    return absl::nullopt;
  }

  base::ScopedTempDir temp_dir_owner;
  if (temp_dir_owner.Set(temp_dir)) {
    return temp_dir_owner;
  }
  return absl::nullopt;
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
  if (!event_handle.IsValid())
    return false;

  base::WaitableEvent event(std::move(event_handle));
  return event.IsSignaled();
}

void StopProcessesUnderPath(const base::FilePath& path,
                            const base::TimeDelta& wait_period) {
  // Filters processes running under `path_prefix`.
  class PathPrefixProcessFilter : public base::ProcessFilter {
   public:
    explicit PathPrefixProcessFilter(const base::FilePath& path_prefix)
        : path_prefix_(path_prefix) {}

    bool Includes(const base::ProcessEntry& entry) const override {
      base::Process process(::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,
                                          false, entry.th32ProcessID));
      if (!process.IsValid())
        return false;

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

absl::optional<base::CommandLine> CommandLineForLegacyFormat(
    const std::wstring& cmd_string) {
  int num_args = 0;
  base::win::ScopedLocalAllocTyped<wchar_t*> args(
      ::CommandLineToArgvW(cmd_string.c_str(), &num_args));
  if (!args)
    return absl::nullopt;

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
      return absl::nullopt;
    }

    if (!is_legacy_switch(args.get()[i])) {
      // This is a bare argument.
      command_line.AppendArg(base::WideToASCII(args.get()[i]));
      continue;
    }

    const std::string switch_name = base::WideToASCII(&args.get()[i][1]);
    if (switch_name.empty()) {
      VLOG(1) << "Empty switch in command line: [" << cmd_string << "]";
      return absl::nullopt;
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

absl::optional<base::FilePath> GetInstallDirectory(UpdaterScope scope) {
  base::FilePath app_data_dir;
  if (!base::PathService::Get(IsSystemInstall(scope) ? base::DIR_PROGRAM_FILES
                                                     : base::DIR_LOCAL_APP_DATA,
                              &app_data_dir)) {
    LOG(ERROR) << "Can't retrieve app data directory.";
    return absl::nullopt;
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

void LogClsidEntries(REFCLSID clsid) {
  const std::wstring local_server32_reg_path(
      base::StrCat({base::StrCat({L"Software\\Classes\\CLSID\\",
                                  base::win::WStringFromGUID(clsid)}),
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

absl::optional<base::FilePath> GetInstallDirectoryX86(UpdaterScope scope) {
  if (!IsSystemInstall(scope)) {
    return GetInstallDirectory(scope);
  }
  base::FilePath install_dir;
  if (!base::PathService::Get(base::DIR_PROGRAM_FILESX86, &install_dir)) {
    LOG(ERROR) << "Can't retrieve directory for DIR_PROGRAM_FILESX86.";
    return absl::nullopt;
  }
  return install_dir.AppendASCII(COMPANY_SHORTNAME_STRING)
      .AppendASCII(PRODUCT_FULLNAME_STRING);
}

}  // namespace updater
