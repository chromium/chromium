// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_UTIL_WIN_UTIL_H_
#define CHROME_UPDATER_UTIL_WIN_UTIL_H_

#include <windows.h>

#include <wrl/client.h>
#include <wrl/implements.h>

#include <cstdint>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/debug/alias.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/functional/function_ref.h"
#include "base/hash/hash.h"
#include "base/logging.h"
#include "base/process/process_iterator.h"
#include "base/ranges/algorithm.h"
#include "base/scoped_generic.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/win/atl.h"
#include "base/win/scoped_handle.h"
#include "base/win/win_util.h"
#include "base/win/windows_types.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/win/scoped_handle.h"

namespace base {
class FilePath;
}

struct IidComparator {
  constexpr bool operator()(const IID& lhs, const IID& rhs) const {
    auto lhs_prefix = std::tie(lhs.Data1, lhs.Data2, lhs.Data3);
    auto rhs_prefix = std::tie(rhs.Data1, rhs.Data2, rhs.Data3);
    if (lhs_prefix < rhs_prefix) {
      return true;
    }
    if (lhs_prefix == rhs_prefix) {
      return base::ranges::lexicographical_compare(lhs.Data4, rhs.Data4);
    }
    return false;
  }
};

namespace updater {

// Converts a `guid` to a string with the format
// {xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}.
[[nodiscard]] std::wstring StringFromGuid(const GUID& guid);

template <typename ValueT>
using HResultOr = base::expected<ValueT, HRESULT>;

class ScHandleTraits {
 public:
  using Handle = SC_HANDLE;

  ScHandleTraits() = delete;
  ScHandleTraits(const ScHandleTraits&) = delete;
  ScHandleTraits& operator=(const ScHandleTraits&) = delete;

  static bool CloseHandle(SC_HANDLE handle) {
    return ::CloseServiceHandle(handle) != FALSE;
  }

  static bool IsHandleValid(SC_HANDLE handle) { return handle != nullptr; }

  static SC_HANDLE NullHandle() { return nullptr; }
};

using ScopedScHandle =
    base::win::GenericScopedHandle<ScHandleTraits,
                                   base::win::DummyVerifierTraits>;

class ProcessFilterName : public base::ProcessFilter {
 public:
  explicit ProcessFilterName(const std::wstring& process_name);
  ~ProcessFilterName() override = default;

  // Overrides for base::ProcessFilter.
  bool Includes(const base::ProcessEntry& entry) const override;

 private:
  // Case-insensive name of the program image to look for, not including the
  // path. The name is not localized, therefore the function must be used
  // to look up only processes whose names are known to be ASCII.
  std::wstring process_name_;
};

namespace internal {

template <typename T>
using WrlRuntimeClass = Microsoft::WRL::RuntimeClass<
    Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
    T>;

}  // namespace internal

// Implements `DynamicIIDs` for interface `Interface`, where `Interface` is the
// implemented interface. `iid_user` and `iid_system` are aliases for interface
// `Interface` for user and system installs respectively.
//
// Usage: derive your COM class that implements interface `Interface` from
// `DynamicIIDsImpl<Interface, iid_user, iid_system>`.
template <typename Interface, REFIID iid_user, REFIID iid_system>
class DynamicIIDsImpl : public internal::WrlRuntimeClass<Interface> {
 public:
  DynamicIIDsImpl() {
    VLOG(3) << __func__ << ": Interface: " << typeid(Interface).name()
            << ": iid_user: " << StringFromGuid(iid_user)
            << ": iid_system: " << StringFromGuid(iid_system)
            << ": IsSystemInstall(): " << IsSystemInstall();
  }

  IFACEMETHODIMP QueryInterface(REFIID riid, void** object) override {
    return internal::WrlRuntimeClass<Interface>::QueryInterface(
        riid == (IsSystemInstall() ? iid_system : iid_user)
            ? __uuidof(Interface)
            : riid,
        object);
  }
};

// Macro that makes it easier to derive from `DynamicIIDsImpl`.
#define DYNAMICIIDSIMPL(interface)                      \
  DynamicIIDsImpl<interface, __uuidof(interface##User), \
                  __uuidof(interface##System)>

// Macros that makes it easier to call the `IDispatchImpl` constructor.
#define IID_MAP_ENTRY_USER(interface) \
  { __uuidof(interface##User), __uuidof(interface) }
#define IID_MAP_ENTRY_SYSTEM(interface) \
  { __uuidof(interface##System), __uuidof(interface) }
#define IID_MAPS_USERSYSTEM(interface) \
  {IID_MAP_ENTRY_USER(interface)}, {   \
    IID_MAP_ENTRY_SYSTEM(interface)    \
  }

// Returns the last error as an HRESULT or E_FAIL if last error is NO_ERROR.
// This is not a drop in replacement for the HRESULT_FROM_WIN32 macro.
// The macro maps a NO_ERROR to S_OK, whereas the HRESULTFromLastError maps a
// NO_ERROR to E_FAIL.
HRESULT HRESULTFromLastError();

struct NamedObjectAttributes {
  NamedObjectAttributes(const std::wstring& name, const CSecurityDesc& sd);
  NamedObjectAttributes(const NamedObjectAttributes& other) = delete;
  NamedObjectAttributes& operator=(const NamedObjectAttributes& other) = delete;
  ~NamedObjectAttributes();

  std::wstring name;

  // `CSecurityAttributes` has broken value semantics because it does not update
  // its `SECURITY_ATTRIBUTES` base to keep it in sync with the internal
  // `m_SecurityDescriptor` data member.
  CSecurityAttributes sa;
};

// For machine and local system, the prefix would be "Global\G{obj_name}".
// For user, the prefix would be "Global\G{user_sid}{obj_name}".
// For machine objects, returns a security attributes that gives permissions to
// both Admins and SYSTEM. This allows for cases where SYSTEM creates the named
// object first. The default DACL for SYSTEM will not allow Admins access.
NamedObjectAttributes GetNamedObjectAttributes(const wchar_t* base_name,
                                               UpdaterScope scope);

// Gets the security descriptor with the default DACL for the current process
// user. The owner is the current user, the group is the current primary group.
// Returns security attributes on success, nullopt on failure.
std::optional<CSecurityDesc> GetCurrentUserDefaultSecurityDescriptor();

// Get security descriptor containing a DACL that grants the ACCESS_MASK access
// to admins and system.
CSecurityDesc GetAdminDaclSecurityDescriptor(ACCESS_MASK accessmask);

// Returns the registry path `Software\{CompanyName}\Update\Clients\{app_id}`.
std::wstring GetAppClientsKey(const std::string& app_id);
std::wstring GetAppClientsKey(const std::wstring& app_id);

// Returns the registry path
// `Software\{CompanyName}\Update\ClientState\{app_id}`.
std::wstring GetAppClientStateKey(const std::string& app_id);
std::wstring GetAppClientStateKey(const std::wstring& app_id);

// Returns the registry path
// `Software\{CompanyName}\Update\ClientState\{app_id}\cohort`.
std::wstring GetAppCohortKey(const std::string& app_id);
std::wstring GetAppCohortKey(const std::wstring& app_id);

// Returns the registry path
// `Software\{CompanyName}\Update\Clients\{app_id}\Commands\{command_id}`.
std::wstring GetAppCommandKey(const std::wstring& app_id,
                              const std::wstring& command_id);

// Returns the registry value
// `{HKRoot}\Software\{CompanyName}\Update\ClientState\{app_id}\ap`.
std::string GetAppAPValue(UpdaterScope scope, const std::string& app_id);

// Returns the registry path for the Updater app id under the |Clients| subkey.
// The path does not include the registry root hive prefix.
std::wstring GetRegistryKeyClientsUpdater();

// Returns the registry path for the Updater app id under the |ClientState|
// subkey. The path does not include the registry root hive prefix.
std::wstring GetRegistryKeyClientStateUpdater();

// Set `name` in `root`\`key` to `value`.
bool SetRegistryKey(HKEY root,
                    const std::wstring& key,
                    const std::wstring& name,
                    const std::wstring& value);

// Deletes or sets the `eulaaccepted` value in the `Google\Update` key, based on
// whether `eula_accepted` is `true` or `false`. Returns `true` on success.
bool SetEulaAccepted(UpdaterScope scope, bool eula_accepted);

// Returns `true` if the token is an elevated administrator. If
// `token` is `NULL`, the current thread token is used.
HResultOr<bool> IsTokenAdmin(HANDLE token);

// Returns true if the user is running as an elevated
// administrator.
HResultOr<bool> IsUserAdmin();

// Returns `true` if the user is running as a
// non-elevated administrator.
HResultOr<bool> IsUserNonElevatedAdmin();

// Returns `true` if the COM caller is an admin.
HResultOr<bool> IsCOMCallerAdmin();

// Returns `true` if the UAC is enabled.
bool IsUACOn();

// Returns `true` if running at high integrity with UAC on.
bool IsElevatedWithUACOn();

// Returns a string representing the UAC settings and elevation state for the
// caller. The value can be used for logging purposes.
std::string GetUACState();

// Returns the versioned service name in the following format:
// "{ProductName}{InternalService/Service}{UpdaterVersion}".
// For instance: "ChromiumUpdaterInternalService92.0.0.1".
std::wstring GetServiceName(bool is_internal_service);

// Returns `KEY_WOW64_32KEY | access`. All registry access under the Updater key
// should use `Wow6432(access)` as the `REGSAM`.
REGSAM Wow6432(REGSAM access);

// Starts a new process via ::ShellExecuteEx. `parameters` and `verb` can be
// empty strings. The function waits until the spawned process has completed.
// `verb` specifies the action to perform. For instance, the "runas" verb
// launches an application as administrator with an UAC prompt if UAC is enabled
// and the parent process is running at medium integrity.
// Returns the exit code of the process or HRESULT on failure. Returns 0 if the
// process was created successfully but the exit code is unknown.
HResultOr<DWORD> ShellExecuteAndWait(const base::FilePath& file_path,
                                     const std::wstring& parameters,
                                     const std::wstring& verb);

// Starts a new elevated process. `file_path` specifies the program to be run.
// `parameters` can be an empty string.
// The function waits until the spawned process has completed.
// Returns the exit code of the process or HRESULT on failure. Returns 0 if the
// process was created successfully but the exit code is unknown.
HResultOr<DWORD> RunElevated(const base::FilePath& file_path,
                             const std::wstring& parameters);

// Runs `cmd_line` de-elevated.The function does not wait for the spawned
// process.
HRESULT RunDeElevatedCmdLine(const std::wstring& cmd_line);

std::optional<base::FilePath> GetGoogleUpdateExePath(UpdaterScope scope);

// Causes the COM runtime not to handle exceptions. Failing to set this
// up is a critical error, since ignoring exceptions may lead to corrupted
// program state.
[[nodiscard]] HRESULT DisableCOMExceptionHandling();

// Builds a command line running `MSIExec` on the provided
// `msi_installer`,`arguments`, and `installer_data_file`, with added logging to
// a log file in the same directory as the MSI installer.
std::wstring BuildMsiCommandLine(
    const std::wstring& arguments,
    const std::optional<base::FilePath>& installer_data_file,
    const base::FilePath& msi_installer);

// Builds a command line running the provided `exe_installer`, `arguments`, and
// `installer_data_file`.
std::wstring BuildExeCommandLine(
    const std::wstring& arguments,
    const std::optional<base::FilePath>& installer_data_file,
    const base::FilePath& exe_installer);

// Returns `true` if the service specified is currently running or starting.
bool IsServiceRunning(const std::wstring& service_name);

// Returns the HKEY root corresponding to the UpdaterScope:
// * scope == UpdaterScope::kSystem == HKEY_LOCAL_MACHINE
// * scope == UpdaterScope::kUser == HKEY_CURRENT_USER
HKEY UpdaterScopeToHKeyRoot(UpdaterScope scope);

// Returns an OSVERSIONINFOEX for the current OS version.
std::optional<OSVERSIONINFOEX> GetOSVersion();

// Compares the current OS to the supplied version.  The value of `oper` should
// be one of the predicate values from `::VerSetConditionMask()`, for example,
// `VER_GREATER` or `VER_GREATER_EQUAL`. `os_version` is usually from a prior
// call to `::GetVersionEx` or `::RtlGetVersion`.
bool CompareOSVersions(const OSVERSIONINFOEX& os, BYTE oper);

// This function calls ::SetDefaultDllDirectories to restrict DLL loads to
// either full paths or %SYSTEM32%. ::SetDefaultDllDirectories is available on
// Windows 8.1 and above, and on Windows Vista and above when KB2533623 is
// applied.
[[nodiscard]] bool EnableSecureDllLoading();

// Enables metadata protection in the heap manager. This allows for the process
// to be terminated immediately when a buffer overflow or illegal heap
// operations are detected. This call enables protection for the entire process
// and cannot be reversed.
bool EnableProcessHeapMetadataProtection();

// Creates a unique temporary directory. The directory is created under a secure
// location if the caller is admin.
std::optional<base::ScopedTempDir> CreateSecureTempDir();

// Signals the shutdown event that causes legacy GoogleUpdate processes to exit.
// Returns a closure that resets the shutdown event when it goes out of scope.
[[nodiscard]] base::ScopedClosureRunner SignalShutdownEvent(UpdaterScope scope);

// Returns `true` if the legacy GoogleUpdate shutdown event is signaled.
bool IsShutdownEventSignaled(UpdaterScope scope);

// Stops processes running under the provided `path`, by first waiting
// `wait_period`, and if the processes still have not exited, by terminating the
// processes.
void StopProcessesUnderPath(const base::FilePath& path,
                            base::TimeDelta wait_period);

// Returns `true` if the argument is a guid.
bool IsGuid(const std::wstring& s);

// Runs `callback` for each run value in the registry that matches `prefix`.
void ForEachRegistryRunValueWithPrefix(
    const std::wstring& prefix,
    base::FunctionRef<void(const std::wstring&)> callback);

// Deletes the registry value at `root\\path`, and returns `true` on success or
// if the path does not exist.
[[nodiscard]] bool DeleteRegValue(HKEY root,
                                  const std::wstring& path,
                                  const std::wstring& value);

// Runs `callback` for each system service that matches `service_name_prefix`
// and `display_name_prefix`. `display_name_prefix` can be empty, in which case,
// only `service_name_prefix` is used for the matching.
void ForEachServiceWithPrefix(
    const std::wstring& service_name_prefix,
    const std::wstring& display_name_prefix,
    base::FunctionRef<void(const std::wstring&)> callback);

// Deletes `service_name` system service and returns `true` on success.
[[nodiscard]] bool DeleteService(const std::wstring& service_name);

// Logs CLSID entries in HKLM and HKCU under both the 64-bit and 32-bit hives
// for the given CLSID.
void LogClsidEntries(REFCLSID clsid);

template <typename T, typename... TArgs>
Microsoft::WRL::ComPtr<T> MakeComObjectOrCrash(TArgs&&... args) {
  auto obj = Microsoft::WRL::Make<T>(std::forward<TArgs>(args)...);
  CHECK(obj);
  return obj;
}

template <typename T, typename I, typename... TArgs>
[[nodiscard]] HRESULT MakeAndInitializeComObject(I** obj, TArgs&&... args) {
  return Microsoft::WRL::MakeAndInitialize<T>(obj,
                                              std::forward<TArgs>(args)...);
}

template <typename T, typename I, typename... TArgs>
[[nodiscard]] HRESULT MakeAndInitializeComObject(Microsoft::WRL::ComPtr<I>& obj,
                                                 TArgs&&... args) {
  return MakeAndInitializeComObject<T>(static_cast<I**>(&obj),
                                       std::forward<TArgs>(args)...);
}

// Returns the base install directory for the x86 versions of the updater.
// Does not create the directory if it does not exist.
[[nodiscard]] std::optional<base::FilePath> GetInstallDirectoryX86(
    UpdaterScope scope);

// Gets the contents under a given registry key.
std::optional<std::wstring> GetRegKeyContents(const std::wstring& reg_key);

// Returns the textual description of a system `error` as provided by the
// operating system. The function assumes that the locale value for the calling
// thread is set, otherwise, the function uses the user/system default LANGID,
// or it defaults to US English.
std::wstring GetTextForSystemError(int error);

// Retrieves the logged on user token for the active explorer process if one
// exists.
HResultOr<ScopedKernelHANDLE> GetLoggedOnUserToken();

// Returns true if running in Windows Audit mode, as documented at
// http://technet.microsoft.com/en-us/library/cc721913.aspx.
bool IsAuditMode();

// Writes the OEM install beginning timestamp in the registry.
bool SetOemInstallState();

// Removes the OEM install beginning timestamp from the registry.
bool ResetOemInstallState();

// Returns `true` if the OEM install time is present and it has been less than
// `kMinOemModeTime` since the OEM install.
bool IsOemInstalling();

// Stores the runtime enrollment token to the persistent storage.
bool StoreRunTimeEnrollmentToken(const std::string& enrollment_token);

// Returns a unique temp file path of the form
// `%TMP%\{name}{guid}.{fileextension}`, where `name` and `extension` are the
// name and extension of `file`.
std::optional<base::FilePath> GetUniqueTempFilePath(base::FilePath file);

}  // namespace updater

#endif  // CHROME_UPDATER_UTIL_WIN_UTIL_H_
