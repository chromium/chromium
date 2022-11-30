// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/os/system_util_cleaner.h"

#include <windows.h>

#include <accctrl.h>
#include <aclapi.h>
#include <lmcons.h>
#include <shellapi.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/process/process_info.h"
#include "base/process/process_iterator.h"
#include "base/strings/string_util.h"
#include "base/threading/simple_thread.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/scoped_handle.h"
#include "base/win/scoped_localalloc.h"
#include "base/win/sid.h"
#include "base/win/windows_version.h"
#include "chrome/chrome_cleaner/constants/chrome_cleaner_switches.h"
#include "chrome/chrome_cleaner/constants/quarantine_constants.h"
#include "chrome/chrome_cleaner/os/disk_util.h"
#include "chrome/chrome_cleaner/os/scoped_service_handle.h"
#include "chrome/chrome_cleaner/os/system_util.h"

namespace chrome_cleaner {

namespace {

// The size of strings to be passed down to user info API.
const size_t kAccountSidMaxStringSize = UNLEN + 1;

// The number of iterations to poll if a service is stopped correctly.
const unsigned int kMaxServiceQueryIterations = 100;

// The number of iteration to poll if a service exists.
const unsigned int kMaxServiceExistIterations = 5;

// The sleep time in ms between each poll.
const unsigned int kServiceQueryWaitTimeMs = 100;

// The number of iterations to poll if a process is stopped correctly.
const unsigned int kMaxProcessQueryIterations = 50;

// The sleep time in ms between each poll.
const unsigned int kProcessQueryWaitTimeMs = 100;

// Class used to run ShellExecuteEx() in a separate thread where COM is
// STA-initialized.
class LaunchElevatedProcessThreadDelegate
    : public base::DelegateSimpleThread::Delegate {
 public:
  LaunchElevatedProcessThreadDelegate(const base::CommandLine& command_line,
                                      HWND hwnd)
      : command_line_(command_line), hwnd_(hwnd) {}

  void Run() override {
    // ShellExecuteEx() must run in a STA-initialized thread. The
    // ScopedCOMInitializer object's default constructor initializes COM as an
    // STA.
    //
    // The code that calls ShellExecuteEx was inspired by
    // https://cs-staging.chromium.org/webrtc/src/base/process/launch.h?l=261
    base::win::ScopedCOMInitializer scoped_com_initializer;
    if (!scoped_com_initializer.Succeeded()) {
      PLOG(ERROR) << "Failed to initialize COM when launching elevated process";
      return;
    }

    const std::wstring file = command_line_.GetProgram().value();
    const std::wstring arguments = command_line_.GetArgumentsString();

    SHELLEXECUTEINFO shex_info = {};
    shex_info.cbSize = sizeof(shex_info);
    shex_info.fMask = SEE_MASK_NOCLOSEPROCESS;
    shex_info.hwnd = hwnd_;
    shex_info.lpVerb = L"runas";
    shex_info.lpFile = file.c_str();
    shex_info.lpParameters = arguments.c_str();
    shex_info.nShow = SW_SHOWNORMAL;

    if (::ShellExecuteEx(&shex_info))
      privileged_process_ = base::Process(shex_info.hProcess);
    else
      PLOG(ERROR) << "Failed to launch elevated process";
  }

  // Must be called only after the simple thread object has been joined.
  base::Process GetProcess() { return std::move(privileged_process_); }

 private:
  const base::CommandLine command_line_;
  const HWND hwnd_;
  base::Process privileged_process_;
};

// Return true if running as the system user.
bool IsRunningAsSystem() {
  wchar_t name[kAccountSidMaxStringSize];
  DWORD name_size = kAccountSidMaxStringSize;
  CHECK(::GetUserName(name, &name_size));
  return ::wcscmp(name, L"SYSTEM") == 0;
}

// Get the type of elevation of the current process.
HRESULT GetElevationType(PTOKEN_ELEVATION_TYPE elevation) {
  DCHECK(elevation);
  *elevation = TokenElevationTypeDefault;

  HANDLE token = nullptr;
  if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &token))
    return HRESULT_FROM_WIN32(GetLastError());

  base::win::ScopedHandle process_token(token);

  DWORD size = 0;
  TOKEN_ELEVATION_TYPE elevation_type;
  if (!::GetTokenInformation(token, TokenElevationType, &elevation_type,
                             sizeof(elevation_type), &size)) {
    return HRESULT_FROM_WIN32(GetLastError());
  }

  *elevation = elevation_type;
  return S_OK;
}

// Verify if the current process is running with admin rights or not.
bool CheckForAdminRights() {
  if (IsRunningAsSystem()) {
    return true;
  } else {
    if (base::GetCurrentProcessIntegrityLevel() == base::HIGH_INTEGRITY)
      return true;
    TOKEN_ELEVATION_TYPE elevation = TokenElevationTypeDefault;
    return SUCCEEDED(GetElevationType(&elevation)) &&
           elevation == TokenElevationTypeFull;
  }
}

bool SetRightsPrivileges(const wchar_t* privilege_name, bool enable) {
  HANDLE token = nullptr;
  if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES,
                          &token)) {
    PLOG(WARNING) << "Failed to OpenProcessToken of CurrentProcess.";
    return false;
  }
  base::win::ScopedHandle process_token(token);

  LUID previous_rights = {};
  if (!::LookupPrivilegeValue(nullptr, privilege_name, &previous_rights)) {
    PLOG(WARNING) << "Failed to LookupPrivilegeValue.";
    return false;
  }

  TOKEN_PRIVILEGES token_privileges;
  token_privileges.PrivilegeCount = 1;
  token_privileges.Privileges[0].Luid = previous_rights;

  if (enable)
    token_privileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
  else
    token_privileges.Privileges[0].Attributes = 0;

  if (!::AdjustTokenPrivileges(process_token.Get(), FALSE, &token_privileges,
                               sizeof(TOKEN_PRIVILEGES), nullptr, nullptr)) {
    PLOG(WARNING) << "Failed to AdjustTokenPrivileges.";
    return false;
  }
  return true;
}

bool IsPrivilegeEnabled(const wchar_t* privilege_name) {
  HANDLE token = nullptr;
  if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_READ, &token)) {
    PLOG(ERROR) << "Can't open process for TOKEN_READ.";
    return false;
  }
  base::win::ScopedHandle process_token(token);

  LUID rights = {};
  if (!::LookupPrivilegeValue(nullptr, privilege_name, &rights)) {
    PLOG(ERROR) << "Can't lookup privilege value.";
    return false;
  }

  // Get the list of privileges in the token.
  DWORD size = 0;
  ::GetTokenInformation(token, TokenPrivileges, nullptr, 0, &size);
  std::unique_ptr<BYTE[]> token_privileges_bytes(new BYTE[size]);
  TOKEN_PRIVILEGES* token_privileges =
      reinterpret_cast<TOKEN_PRIVILEGES*>(token_privileges_bytes.get());
  if (!::GetTokenInformation(token, TokenPrivileges, token_privileges, size,
                             &size)) {
    PLOG(ERROR) << "Can't GetTokenInformation.";
    return false;
  }
  for (size_t i = 0; i < token_privileges->PrivilegeCount; ++i) {
    if (token_privileges->Privileges[i].Luid.LowPart == rights.LowPart &&
        token_privileges->Privileges[i].Luid.HighPart == rights.HighPart) {
      return token_privileges->Privileges[i].Attributes == SE_PRIVILEGE_ENABLED;
    }
  }
  return false;
}

bool SendStopToService(const wchar_t* service_name) {
  DCHECK(service_name);
  LOG(INFO) << "Stopping service '" << service_name << "'.";

  ScopedServiceHandle service;
  if (!service.OpenService(service_name, SC_MANAGER_ALL_ACCESS, SERVICE_STOP))
    return false;
  // If the service doesn't exist, assume it's stopped.
  if (!service.IsValid())
    return true;

  // Stop the service.
  SERVICE_STATUS service_state;
  if (!::ControlService(service.get(), SERVICE_CONTROL_STOP, &service_state)) {
    if (::GetLastError() == ERROR_SERVICE_NOT_ACTIVE) {
      LOG(INFO) << "Service '" << service_name << "' is not active.";
      return true;
    }
    PLOG(WARNING) << "Control service failed: could not stop the service.";
    return false;
  }
  return true;
}

bool GetQuarantineFolderPath(base::FilePath* output_path) {
  DCHECK(output_path);

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kQuarantineDirSwitch)) {
    *output_path = command_line->GetSwitchValuePath(kQuarantineDirSwitch);
  } else {
    base::FilePath product_path;
    if (!GetAppDataProductDirectory(&product_path)) {
      LOG(ERROR) << "Failed to get AppData product directory.";
      return false;
    }
    *output_path = product_path.Append(kQuarantineFolder);
  }
  return true;
}

}  // namespace

bool AcquireDebugRightsPrivileges() {
  return SetRightsPrivileges(SE_DEBUG_NAME, true);
}

bool ReleaseDebugRightsPrivileges() {
  return SetRightsPrivileges(SE_DEBUG_NAME, false);
}

bool HasDebugRightsPrivileges() {
  return IsPrivilegeEnabled(SE_DEBUG_NAME);
}

bool HasAdminRights() {
  static bool elevated = CheckForAdminRights();
  return elevated;
}

bool IsProcessRunning(const wchar_t* executable) {
  base::NamedProcessIterator iter(executable, nullptr);
  const base::ProcessEntry* entry = iter.NextProcessEntry();
  return entry != nullptr;
}

bool WaitForProcessesStopped(const wchar_t* executable) {
  DCHECK(executable);
  LOG(INFO) << "Wait for processes '" << executable << "'.";

  // Wait until the process is completely stopped.
  for (unsigned int iteration = 0; iteration < kMaxProcessQueryIterations;
       ++iteration) {
    if (!IsProcessRunning(executable))
      return true;
    ::Sleep(kProcessQueryWaitTimeMs);
  }

  // The process didn't terminate.
  LOG(ERROR) << "Cannot stop process '" << executable << "', timeout.";
  return false;
}

bool WaitForServiceDeleted(const wchar_t* service_name) {
  for (unsigned int iteration = 0; iteration < kMaxServiceExistIterations;
       ++iteration) {
    if (!DoesServiceExist(service_name))
      return true;
    ::Sleep(kServiceQueryWaitTimeMs);
  }
  return false;
}

bool DoesServiceExist(const wchar_t* service_name) {
  ScopedServiceHandle service;
  if (!service.OpenService(service_name, SC_MANAGER_ALL_ACCESS,
                           SERVICE_QUERY_STATUS)) {
    return false;
  }
  return service.IsValid();
}

bool WaitForServiceStopped(const wchar_t* service_name) {
  DCHECK(service_name);
  LOG(INFO) << "Wait for service '" << service_name << "'.";

  ScopedServiceHandle service;
  if (!service.OpenService(service_name, SC_MANAGER_ALL_ACCESS,
                           SERVICE_QUERY_STATUS)) {
    return false;
  }
  // If the service doesn't exist, assume it's stopped.
  if (!service.IsValid())
    return true;

  // Wait until the service is completely stopped.
  for (unsigned int iteration = 0; iteration < kMaxServiceQueryIterations;
       ++iteration) {
    SERVICE_STATUS_PROCESS service_state = {};
    DWORD needed_bytes = 0;
    if (!::QueryServiceStatusEx(service.get(), SC_STATUS_PROCESS_INFO,
                                reinterpret_cast<LPBYTE>(&service_state),
                                sizeof(SERVICE_STATUS_PROCESS),
                                &needed_bytes)) {
      PLOG(ERROR) << "QueryServiceStatusEx failed for service '" << service_name
                  << "'.";
      return false;
    }

    if (service_state.dwCurrentState == SERVICE_STOPPED)
      return true;

    if (service_state.dwCurrentState != SERVICE_STOP_PENDING &&
        service_state.dwCurrentState != SERVICE_RUNNING) {
      LOG(ERROR) << "Cannot stop service '" << service_name << "'"
                 << ", current service state '" << service_state.dwCurrentState
                 << "'.";
      return false;
    }

    ::Sleep(kServiceQueryWaitTimeMs);
  }

  // The service didn't terminate.
  LOG(ERROR) << "Cannot stop service '" << service_name << "', timeout.";
  return false;
}

bool StopService(const wchar_t* service_name) {
  if (!SendStopToService(service_name))
    return false;
  if (!WaitForServiceStopped(service_name))
    return false;
  return true;
}

bool DeleteService(const wchar_t* service_name) {
  DCHECK(service_name);
  LOG(INFO) << "Delete service '" << service_name << "'.";

  // Attempt to stop the service before deleting it, but don't worry if it
  // doesn't stop.
  StopService(service_name);

  ScopedServiceHandle service;
  if (!service.OpenService(service_name, SC_MANAGER_ALL_ACCESS, DELETE))
    return false;
  // If the service doesn't exist, assume it does not need to be deleted.
  if (!service.IsValid())
    return true;

  if (!::DeleteService(service.get())) {
    if (::GetLastError() == ERROR_SERVICE_MARKED_FOR_DELETE) {
      LOG(WARNING) << "Service '" << service_name
                   << "' has already been marked for deletion.";
      return true;
    }
    PLOG(WARNING) << "DeleteService failed for service '" << service_name
                  << "'.";
    return false;
  }

  return true;
}

// This sets up COM security to allow NetworkService, LocalService, and System
// to call back into the process. It is largely inspired by
// http://msdn.microsoft.com/en-us/library/windows/desktop/aa378987.aspx
// static
bool InitializeCOMSecurity() {
  // Create the security descriptor explicitly as follows because
  // CoInitializeSecurity() will not accept the relative security descriptors
  // returned by ConvertStringSecurityDescriptorToSecurityDescriptor().
  const size_t kSidCount = 5;
  uint64_t* sids[kSidCount][(SECURITY_MAX_SID_SIZE + sizeof(uint64_t) - 1) /
                            sizeof(uint64_t)] = {
      {}, {}, {}, {}, {},
  };

  // These are ordered by most interesting ones to try first.
  WELL_KNOWN_SID_TYPE sid_types[kSidCount] = {
      WinBuiltinAdministratorsSid,  // administrator group security identifier
      WinLocalServiceSid,           // local service security identifier
      WinNetworkServiceSid,         // network service security identifier
      WinSelfSid,                   // personal account security identifier
      WinLocalSystemSid,            // local system security identifier
  };

  // This creates a security descriptor that is equivalent to the following
  // security descriptor definition language (SDDL) string:
  //   O:BAG:BAD:(A;;0x1;;;LS)(A;;0x1;;;NS)(A;;0x1;;;PS)
  //   (A;;0x1;;;SY)(A;;0x1;;;BA)

  // Initialize the security descriptor.
  SECURITY_DESCRIPTOR security_desc = {};
  if (!::InitializeSecurityDescriptor(&security_desc,
                                      SECURITY_DESCRIPTOR_REVISION))
    return false;

  DCHECK_EQ(kSidCount, std::size(sids));
  DCHECK_EQ(kSidCount, std::size(sid_types));
  for (size_t i = 0; i < kSidCount; ++i) {
    DWORD sid_bytes = sizeof(sids[i]);
    if (!::CreateWellKnownSid(sid_types[i], nullptr, sids[i], &sid_bytes))
      return false;
  }

  // Setup the access control entries (ACE) for COM. You may need to modify
  // the access permissions for your application. COM_RIGHTS_EXECUTE and
  // COM_RIGHTS_EXECUTE_LOCAL are the minimum access rights required.
  EXPLICIT_ACCESS explicit_access[kSidCount] = {};
  DCHECK_EQ(kSidCount, std::size(sids));
  DCHECK_EQ(kSidCount, std::size(explicit_access));
  for (size_t i = 0; i < kSidCount; ++i) {
    explicit_access[i].grfAccessPermissions =
        COM_RIGHTS_EXECUTE | COM_RIGHTS_EXECUTE_LOCAL;
    explicit_access[i].grfAccessMode = SET_ACCESS;
    explicit_access[i].grfInheritance = NO_INHERITANCE;
    explicit_access[i].Trustee.pMultipleTrustee = nullptr;
    explicit_access[i].Trustee.MultipleTrusteeOperation = NO_MULTIPLE_TRUSTEE;
    explicit_access[i].Trustee.TrusteeForm = TRUSTEE_IS_SID;
    explicit_access[i].Trustee.TrusteeType = TRUSTEE_IS_GROUP;
    explicit_access[i].Trustee.ptstrName = reinterpret_cast<LPTSTR>(sids[i]);
  }

  // Create an access control list (ACL) using this ACE list, if this succeeds
  // make sure to ::LocalFree(acl).
  ACL* acl = nullptr;
  DWORD acl_result = ::SetEntriesInAcl(std::size(explicit_access),
                                       explicit_access, nullptr, &acl);
  if (acl_result != ERROR_SUCCESS || acl == nullptr)
    return false;

  HRESULT hr = E_FAIL;

  // Set the security descriptor owner and group to Administrators and set the
  // discretionary access control list (DACL) to the ACL.
  if (::SetSecurityDescriptorOwner(&security_desc, sids[0], FALSE) &&
      ::SetSecurityDescriptorGroup(&security_desc, sids[0], FALSE) &&
      ::SetSecurityDescriptorDacl(&security_desc, TRUE, acl, FALSE)) {
    // Initialize COM. You may need to modify the parameters of
    // CoInitializeSecurity() for your application. Note that an
    // explicit security descriptor is being passed down.
    hr = ::CoInitializeSecurity(
        &security_desc, -1, nullptr, nullptr, RPC_C_AUTHN_LEVEL_PKT_PRIVACY,
        RPC_C_IMP_LEVEL_IDENTIFY, nullptr,
        EOAC_DISABLE_AAA | EOAC_NO_CUSTOM_MARSHAL, nullptr);
  }

  ::LocalFree(acl);
  return SUCCEEDED(hr);
}

base::Process LaunchElevatedProcessWithAssociatedWindow(
    const base::CommandLine& command_line,
    HWND hwnd) {
  LaunchElevatedProcessThreadDelegate runner(command_line, hwnd);
  base::DelegateSimpleThread thread(&runner, "ElevatedProcessLauncher");
  thread.Start();
  thread.Join();

  return runner.GetProcess();
}

// Only allow administrator group to access the quarantine folder.
bool InitializeQuarantineFolder(base::FilePath* output_quarantine_path) {
  DCHECK(output_quarantine_path);

  base::FilePath quarantine_path;
  if (!GetQuarantineFolderPath(&quarantine_path)) {
    LOG(ERROR) << "Failed to get quarantine folder path.";
    return false;
  }

  if (!base::DirectoryExists(quarantine_path) &&
      !base::CreateDirectory(quarantine_path)) {
    LOG(ERROR) << "Failed to create quarantine folder.";
    return false;
  }

  const absl::optional<base::win::Sid> admin_sid = base::win::Sid::FromKnownSid(
      base::win::WellKnownSid::kBuiltinAdministrators);
  if (!admin_sid) {
    LOG(ERROR) << "Failed to get administrator sid.";
    return false;
  }

  EXPLICIT_ACCESS explicit_access[1] = {};
  explicit_access[0].grfAccessPermissions = GENERIC_ALL;
  explicit_access[0].grfAccessMode = SET_ACCESS;
  explicit_access[0].grfInheritance = NO_INHERITANCE;
  ::BuildTrusteeWithSidW(&explicit_access[0].Trustee, admin_sid->GetPSID());

  PACL dacl_ptr = nullptr;
  if (::SetEntriesInAcl(std::size(explicit_access), explicit_access,
                        /*OldAcl=*/nullptr, &dacl_ptr) != ERROR_SUCCESS) {
    LOG(ERROR) << "Failed to create DACL for quarantine folder.";
    return false;
  }

  base::win::ScopedLocalAllocTyped<ACL> dacl =
      base::win::TakeLocalAlloc(dacl_ptr);
  DWORD result_code = ERROR_SUCCESS;
  // |PROTECTED_DACL_SECURITY_INFORMATION| will remove inherited ACLs.
  result_code = ::SetNamedSecurityInfo(
      const_cast<wchar_t*>(quarantine_path.value().c_str()), SE_FILE_OBJECT,
      OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION |
          PROTECTED_DACL_SECURITY_INFORMATION,
      admin_sid->GetPSID(), /*psidGroup=*/nullptr, dacl.get(),
      /*pSacl=*/nullptr);

  if (result_code != ERROR_SUCCESS) {
    LOG(ERROR) << "Failed to set ACLs to quarantine folder.";
    return false;
  }
  *output_quarantine_path = quarantine_path;
  return true;
}

}  // namespace chrome_cleaner
