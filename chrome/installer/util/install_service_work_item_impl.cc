// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/install_service_work_item_impl.h"

#include <cguid.h>
#include <guiddef.h>

#include <memory>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/win/registry.h"
#include "base/win/win_util.h"
#include "chrome/installer/util/install_util.h"

using base::win::RegKey;

namespace installer {

namespace {

constexpr uint32_t kServiceType = SERVICE_WIN32_OWN_PROCESS;
constexpr uint32_t kServiceStartType = SERVICE_DEMAND_START;
constexpr uint32_t kServiceErrorControl = SERVICE_ERROR_NORMAL;
constexpr wchar_t kServiceDependencies[] = L"RPCSS\0";

// For the service handle, all permissions that could possibly be used in all
// Do/Rollback scenarios are requested since the handle is reused.
constexpr uint32_t kServiceAccess =
    DELETE | SERVICE_QUERY_CONFIG | SERVICE_CHANGE_CONFIG;

// One value for each possible outcome of DoImpl.
// These values are logged to histograms. Entries should not be renumbered and
// numeric values should never be reused.
enum class ServiceInstallResult {
  kFailedFreshInstall = 0,
  kFailedInstallNewAfterFailedUpgrade = 1,
  kFailedOpenSCManager = 2,
  kSucceededChangeServiceConfig = 3,
  kSucceededFreshInstall = 4,
  kSucceededInstallNewAndDeleteOriginal = 5,
  kSucceededInstallNewAndFailedDeleteOriginal = 6,
  kSucceededServiceCorrectlyConfigured = 7,
  kMaxValue = kSucceededServiceCorrectlyConfigured,
};

// One value for each possible outcome of RollbackImpl.
// These values are logged to histograms. Entries should not be renumbered and
// numeric values should never be reused.
enum class ServiceRollbackResult {
  kFailedDeleteCurrentService = 0,
  kFailedRollbackOriginalServiceConfig = 1,
  kSucceededDeleteCurrentService = 2,
  kSucceededRollbackOriginalServiceConfig = 3,
  kMaxValue = kSucceededRollbackOriginalServiceConfig,
};

// One value for each possible call to RecordWin32ApiErrorCode. When new values
// are added here, histogram_suffixes "SetupInstallWin32Apis" needs to be
// updated in histograms.xml.
constexpr char kChangeServiceConfig[] = "ChangeServiceConfig";
constexpr char kCreateService[] = "CreateService";
constexpr char kDeleteService[] = "DeleteService";
constexpr char kOpenSCManager[] = "OpenSCManager";

void RecordServiceInstallResult(ServiceInstallResult value) {
  // Use the histogram function rather than the macro since only one value will
  // be recorded per run.
  base::UmaHistogramEnumeration("Setup.Install.ServiceInstallResult", value);
}

void RecordServiceRollbackResult(ServiceRollbackResult value) {
  // Uses the histogram function rather than the macro since only one value will
  // be recorded per run.
  base::UmaHistogramEnumeration("Setup.Install.ServiceRollbackResult", value);
}

// Records the last Win32 error in a histogram named
// "Setup.Install.Win32ApiError.|function|". |function| is one of the values in
// the list of histogram suffixes "SetupInstallWin32Apis" above.
void RecordWin32ApiErrorCode(const char* function) {
  auto error_code = ::GetLastError();

  // Uses the histogram function rather than the macro since the name of the
  // histogram is computed at runtime.
  base::UmaHistogramSparse(
      base::StrCat({"Setup.Install.Win32ApiError.", function}), error_code);
}

std::wstring GetComRegistryPath(base::WStringPiece hive, const GUID& guid) {
  return base::StrCat(
      {L"Software\\Classes\\", hive, L"\\", base::win::WStringFromGUID(guid)});
}

std::wstring GetComClsidRegistryPath(const GUID& clsid) {
  return GetComRegistryPath(L"CLSID", clsid);
}

std::wstring GetComAppidRegistryPath(const GUID& appid) {
  return GetComRegistryPath(L"AppID", appid);
}

std::wstring GetComIidRegistryPath(const GUID& iid) {
  return GetComRegistryPath(L"Interface", iid);
}

std::wstring GetComTypeLibRegistryPath(const GUID& iid) {
  return GetComRegistryPath(L"TypeLib", iid);
}

}  // namespace

InstallServiceWorkItemImpl::ServiceConfig::ServiceConfig()
    : is_valid(false),
      type(SERVICE_KERNEL_DRIVER),
      start_type(SERVICE_DISABLED),
      error_control(SERVICE_ERROR_CRITICAL) {}

InstallServiceWorkItemImpl::ServiceConfig::ServiceConfig(
    uint32_t service_type,
    uint32_t service_start_type,
    uint32_t service_error_control,
    const std::wstring& service_cmd_line,
    const wchar_t* dependencies_multi_sz)
    : is_valid(true),
      type(service_type),
      start_type(service_start_type),
      error_control(service_error_control),
      cmd_line(service_cmd_line),
      dependencies(MultiSzToVector(dependencies_multi_sz)) {}

InstallServiceWorkItemImpl::ServiceConfig::ServiceConfig(ServiceConfig&& rhs) =
    default;

InstallServiceWorkItemImpl::ServiceConfig::~ServiceConfig() = default;

InstallServiceWorkItemImpl::InstallServiceWorkItemImpl(
    const std::wstring& service_name,
    const std::wstring& display_name,
    const base::CommandLine& service_cmd_line,
    const std::wstring& registry_path,
    const std::vector<GUID>& clsids,
    const std::vector<GUID>& iids)
    : com_registration_work_items_(WorkItem::CreateWorkItemList()),
      service_name_(service_name),
      display_name_(display_name),
      service_cmd_line_(service_cmd_line),
      registry_path_(registry_path),
      clsids_(clsids),
      iids_(iids),
      rollback_existing_service_(false),
      rollback_new_service_(false),
      original_service_still_exists_(false) {}

InstallServiceWorkItemImpl::~InstallServiceWorkItemImpl() = default;

bool InstallServiceWorkItemImpl::DoImpl() {
  return DoInstallService() && DoComRegistration();
}

bool InstallServiceWorkItemImpl::DoInstallService() {
  scm_.Set(::OpenSCManager(nullptr, nullptr,
                           SC_MANAGER_CONNECT | SC_MANAGER_CREATE_SERVICE));
  if (!scm_.IsValid()) {
    PLOG(ERROR) << "::OpenSCManager Failed";
    RecordServiceInstallResult(ServiceInstallResult::kFailedOpenSCManager);
    RecordWin32ApiErrorCode(kOpenSCManager);
    return false;
  }

  if (!OpenService()) {
    VPLOG(1) << "Attempting to install new service following failure to open";
    const bool succeeded = InstallNewService();
    if (succeeded) {
      RecordServiceInstallResult(ServiceInstallResult::kSucceededFreshInstall);
      return succeeded;
    }

    PLOG(ERROR) << "Failed to install service "
                << GetCurrentServiceName().c_str();
    RecordServiceInstallResult(ServiceInstallResult::kFailedFreshInstall);
    RecordWin32ApiErrorCode(kCreateService);
    // Fall through to try installing the service by generating a new name.
  } else if (UpgradeService()) {
    // It is preferable to do a lightweight upgrade of the existing service,
    // instead of deleting and recreating a new service, since it is less
    // likely to fail. Less intrusive to the SCM and to AV/Anti-malware
    // programs.
    return true;
  } else {
    LOG(ERROR) << "Failed to upgrade service "
               << GetCurrentServiceName().c_str();
  }

  // Save the original service name. Then create a new service name so as to not
  // conflict with the previous one to be safe, then install the new service.
  original_service_name_ = GetCurrentServiceName();
  if (!CreateAndSetServiceName())
    PLOG(ERROR) << "Failed to create and set unique service name";

  ScopedScHandle original_service = std::move(service_);
  if (InstallNewService()) {
    // Delete the previous version of the service.
    if (DeleteService(std::move(original_service))) {
      RecordServiceInstallResult(
          ServiceInstallResult::kSucceededInstallNewAndDeleteOriginal);
    } else {
      original_service_still_exists_ = true;

      RecordServiceInstallResult(
          ServiceInstallResult::kSucceededInstallNewAndFailedDeleteOriginal);
      RecordWin32ApiErrorCode(kDeleteService);
    }

    return true;
  }

  PLOG(ERROR) << "Failed to install service with new name "
              << GetCurrentServiceName().c_str();
  RecordServiceInstallResult(
      ServiceInstallResult::kFailedInstallNewAfterFailedUpgrade);
  RecordWin32ApiErrorCode(kCreateService);
  return false;
}

bool InstallServiceWorkItemImpl::DoComRegistration() {
  for (const auto& clsid : clsids_) {
    const std::wstring clsid_reg_path = GetComClsidRegistryPath(clsid);
    const std::wstring appid_reg_path = GetComAppidRegistryPath(clsid);

    com_registration_work_items_->AddCreateRegKeyWorkItem(
        HKEY_LOCAL_MACHINE, clsid_reg_path, WorkItem::kWow64Default);
    com_registration_work_items_->AddSetRegValueWorkItem(
        HKEY_LOCAL_MACHINE, clsid_reg_path, WorkItem::kWow64Default, L"AppID",
        base::win::WStringFromGUID(clsid), true);
    com_registration_work_items_->AddCreateRegKeyWorkItem(
        HKEY_LOCAL_MACHINE, appid_reg_path, WorkItem::kWow64Default);
    com_registration_work_items_->AddSetRegValueWorkItem(
        HKEY_LOCAL_MACHINE, appid_reg_path, WorkItem::kWow64Default,
        L"LocalService", GetCurrentServiceName(), true);
  }

  for (const auto& iid : iids_) {
    const std::wstring iid_reg_path = GetComIidRegistryPath(iid);
    const std::wstring typelib_reg_path = GetComTypeLibRegistryPath(iid);

    // Registering the Ole Automation marshaler with the CLSID
    // {00020424-0000-0000-C000-000000000046} as the proxy/stub for the
    // interface.
    com_registration_work_items_->AddCreateRegKeyWorkItem(
        HKEY_LOCAL_MACHINE, iid_reg_path, WorkItem::kWow64Default);
    com_registration_work_items_->AddCreateRegKeyWorkItem(
        HKEY_LOCAL_MACHINE, iid_reg_path + L"\\ProxyStubClsid32",
        WorkItem::kWow64Default);
    com_registration_work_items_->AddSetRegValueWorkItem(
        HKEY_LOCAL_MACHINE, iid_reg_path + L"\\ProxyStubClsid32",
        WorkItem::kWow64Default, L"", L"{00020424-0000-0000-C000-000000000046}",
        true);
    com_registration_work_items_->AddCreateRegKeyWorkItem(
        HKEY_LOCAL_MACHINE, iid_reg_path + L"\\TypeLib",
        WorkItem::kWow64Default);
    com_registration_work_items_->AddSetRegValueWorkItem(
        HKEY_LOCAL_MACHINE, iid_reg_path + L"\\TypeLib",
        WorkItem::kWow64Default, L"", base::win::WStringFromGUID(iid), true);
    com_registration_work_items_->AddSetRegValueWorkItem(
        HKEY_LOCAL_MACHINE, iid_reg_path + L"\\TypeLib",
        WorkItem::kWow64Default, L"Version", L"1.0", true);

    // The TypeLib registration for the Ole Automation marshaler.
    com_registration_work_items_->AddCreateRegKeyWorkItem(
        HKEY_LOCAL_MACHINE, typelib_reg_path, WorkItem::kWow64Default);
    com_registration_work_items_->AddCreateRegKeyWorkItem(
        HKEY_LOCAL_MACHINE, typelib_reg_path + L"\\1.0",
        WorkItem::kWow64Default);
    com_registration_work_items_->AddCreateRegKeyWorkItem(
        HKEY_LOCAL_MACHINE, typelib_reg_path + L"\\1.0\\0",
        WorkItem::kWow64Default);
    com_registration_work_items_->AddCreateRegKeyWorkItem(
        HKEY_LOCAL_MACHINE, typelib_reg_path + L"\\1.0\\0\\win32",
        WorkItem::kWow64Default);
    com_registration_work_items_->AddSetRegValueWorkItem(
        HKEY_LOCAL_MACHINE, typelib_reg_path + L"\\1.0\\0\\win32",
        WorkItem::kWow64Default, L"", service_cmd_line_.GetProgram().value(),
        true);
    com_registration_work_items_->AddCreateRegKeyWorkItem(
        HKEY_LOCAL_MACHINE, typelib_reg_path + L"\\1.0\\0\\win64",
        WorkItem::kWow64Default);
    com_registration_work_items_->AddSetRegValueWorkItem(
        HKEY_LOCAL_MACHINE, typelib_reg_path + L"\\1.0\\0\\win64",
        WorkItem::kWow64Default, L"", service_cmd_line_.GetProgram().value(),
        true);
  }

  return com_registration_work_items_->Do();
}

void InstallServiceWorkItemImpl::RollbackImpl() {
  com_registration_work_items_->Rollback();

  DCHECK(!(rollback_existing_service_ && rollback_new_service_));

  if (!rollback_existing_service_ && !rollback_new_service_)
    return;

  if (rollback_existing_service_) {
    DCHECK(service_.IsValid());
    DCHECK(original_service_config_.is_valid);
    if (RestoreOriginalServiceConfig()) {
      RecordServiceRollbackResult(
          ServiceRollbackResult::kSucceededRollbackOriginalServiceConfig);
    } else {
      RecordServiceRollbackResult(
          ServiceRollbackResult::kFailedRollbackOriginalServiceConfig);
      RecordWin32ApiErrorCode(kChangeServiceConfig);
    }
    return;
  }

  DCHECK(rollback_new_service_);
  DCHECK(service_.IsValid());

  // Delete the newly created service.
  if (DeleteCurrentService()) {
    RecordServiceRollbackResult(
        ServiceRollbackResult::kSucceededDeleteCurrentService);
  } else {
    RecordServiceRollbackResult(
        ServiceRollbackResult::kFailedDeleteCurrentService);
    RecordWin32ApiErrorCode(kDeleteService);
  }

  if (original_service_name_.empty())
    return;

  if (original_service_still_exists_) {
    // Set only the service name back to original_service_name_ and return.
    if (!SetServiceName(original_service_name_)) {
      PLOG(ERROR) << "Failed to restore service name to "
                  << original_service_name_;
    }
    return;
  }

  // Recreate original service with a new service name to avoid possible SCM
  // issues with reusing the old name.
  if (!CreateAndSetServiceName())
    PLOG(ERROR) << "Failed to create and set unique service name";
  if (!ReinstallOriginalService())
    RecordWin32ApiErrorCode(kCreateService);
}

bool InstallServiceWorkItemImpl::DeleteServiceImpl() {
  // Uninstall the elevation service.
  for (const auto& clsid : clsids_) {
    for (const auto& reg_path :
         {GetComClsidRegistryPath(clsid), GetComAppidRegistryPath(clsid)}) {
      InstallUtil::DeleteRegistryKey(HKEY_LOCAL_MACHINE, reg_path,
                                     WorkItem::kWow64Default);
    }
  }

  for (const auto& iid : iids_) {
    for (const auto& reg_path :
         {GetComIidRegistryPath(iid), GetComTypeLibRegistryPath(iid)}) {
      InstallUtil::DeleteRegistryKey(HKEY_LOCAL_MACHINE, reg_path,
                                     WorkItem::kWow64Default);
    }
  }

  scm_.Set(::OpenSCManager(nullptr, nullptr, SC_MANAGER_CONNECT));
  if (!scm_.IsValid()) {
    DPLOG(ERROR) << "::OpenSCManager Failed";
    return false;
  }

  if (!OpenService())
    return false;

  if (!DeleteCurrentService())
    return false;

  // If the service cannot be deleted, the service name value is not deleted.
  // This is to allow for identifying that an existing instance of the service
  // is still installed when a future install or upgrade runs.
  base::win::RegKey key;
  auto result = key.Open(HKEY_LOCAL_MACHINE, registry_path_.c_str(),
                         KEY_SET_VALUE | KEY_WOW64_32KEY);
  if (result != ERROR_SUCCESS)
    return result == ERROR_FILE_NOT_FOUND || result == ERROR_PATH_NOT_FOUND;

  result = key.DeleteValue(service_name_.c_str());
  return result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND ||
         result == ERROR_PATH_NOT_FOUND;
}

bool InstallServiceWorkItemImpl::IsServiceCorrectlyConfigured(
    const ServiceConfig& config) {
  return config.type == kServiceType &&
         config.start_type == kServiceStartType &&
         config.error_control == kServiceErrorControl &&
         !_wcsicmp(config.cmd_line.c_str(),
                   service_cmd_line_.GetCommandLineString().c_str()) &&
         config.dependencies == MultiSzToVector(kServiceDependencies);
}

bool InstallServiceWorkItemImpl::DeleteCurrentService() {
  return DeleteService(std::move(service_));
}

bool InstallServiceWorkItemImpl::OpenService() {
  DCHECK(scm_.IsValid());
  service_.Set(::OpenService(scm_.Get(), GetCurrentServiceName().c_str(),
                             kServiceAccess));
  return service_.IsValid();
}

bool InstallServiceWorkItemImpl::GetServiceConfig(ServiceConfig* config) const {
  DCHECK(config);
  DCHECK(service_.IsValid());

  constexpr uint32_t kMaxQueryConfigBufferBytes = 8 * 1024;

  // ::QueryServiceConfig expects a buffer of at most 8K bytes, according to
  // documentation. While the size of the buffer can be dynamically computed,
  // we just assume the maximum size for simplicity.
  auto buffer = std::make_unique<uint8_t[]>(kMaxQueryConfigBufferBytes);
  DWORD bytes_needed_ignored = 0;
  QUERY_SERVICE_CONFIG* service_config =
      reinterpret_cast<QUERY_SERVICE_CONFIG*>(buffer.get());
  if (!::QueryServiceConfig(service_.Get(), service_config,
                            kMaxQueryConfigBufferBytes,
                            &bytes_needed_ignored)) {
    PLOG(ERROR) << "QueryServiceConfig failed "
                << GetCurrentServiceName().c_str();
    return false;
  }

  *config = ServiceConfig(
      service_config->dwServiceType, service_config->dwStartType,
      service_config->dwErrorControl,
      service_config->lpBinaryPathName ? service_config->lpBinaryPathName : L"",
      service_config->lpDependencies ? service_config->lpDependencies : L"");
  return true;
}

// Creates a unique name of the form "{prefix}1c9b3d6baf90df3" and stores it in
// the registry under HKLM\Google\Chrome. Subsequent invocations of
// GetCurrentServiceName() will return this new value.
// The service_name_ is used as the "Name" entry in the registry under
// HKLM\Software\Google\Update\ClientState\{appguid}. For example,
// HKLM\Software\Google\Update\ClientState\{appguid}
//  "Name"  "elevationservice", "Type" "REG_SZ", "Data" "elevationservice0394"
bool InstallServiceWorkItemImpl::CreateAndSetServiceName() const {
  const std::wstring versioned_service_name(GenerateVersionedServiceName());
  return SetServiceName(versioned_service_name);
}

bool InstallServiceWorkItemImpl::SetServiceName(
    const std::wstring& service_name) const {
  base::win::RegKey key;

  auto result = key.Create(HKEY_LOCAL_MACHINE, registry_path_.c_str(),
                           KEY_SET_VALUE | KEY_WOW64_32KEY);
  DCHECK(result == ERROR_SUCCESS);
  if (result != ERROR_SUCCESS) {
    ::SetLastError(result);
    PLOG(ERROR) << "key.Open failed";
    return false;
  }

  result = key.WriteValue(service_name_.c_str(), service_name.c_str());
  if (result != ERROR_SUCCESS) {
    ::SetLastError(result);
    PLOG(ERROR) << "key.WriteValue failed";
    return false;
  }

  return true;
}

std::wstring InstallServiceWorkItemImpl::GetCurrentServiceName() const {
  base::win::RegKey key;

  auto result = key.Open(HKEY_LOCAL_MACHINE, registry_path_.c_str(),
                         KEY_QUERY_VALUE | KEY_WOW64_32KEY);
  if (result != ERROR_SUCCESS)
    return service_name_;

  std::wstring versioned_service_name;
  key.ReadValue(service_name_.c_str(), &versioned_service_name);
  return versioned_service_name.empty() ? service_name_
                                        : versioned_service_name;
}

std::wstring InstallServiceWorkItemImpl::GetCurrentServiceDisplayName() const {
  return base::StringPrintf(L"%ls (%ls)", display_name_.c_str(),
                            GetCurrentServiceName().c_str());
}

std::vector<wchar_t> InstallServiceWorkItemImpl::MultiSzToVector(
    const wchar_t* multi_sz) {
  if (!multi_sz)
    return std::vector<wchar_t>();

  if (!*multi_sz)
    return std::vector<wchar_t>(1, L'\0');

  // Scan forward to the second terminating '\0' at the end of the list of
  // strings in the multi-sz.
  const wchar_t* scan = multi_sz;
  do {
    scan += wcslen(scan) + 1;
  } while (*scan);

  return std::vector<wchar_t>(multi_sz, scan + 1);
}

bool InstallServiceWorkItemImpl::InstallNewService() {
  DCHECK(!service_.IsValid());
  bool success = InstallService(ServiceConfig(
      kServiceType, kServiceStartType, kServiceErrorControl,
      service_cmd_line_.GetCommandLineString(), kServiceDependencies));
  if (success)
    rollback_new_service_ = true;
  return success;
}

bool InstallServiceWorkItemImpl::UpgradeService() {
  DCHECK(service_.IsValid());
  DCHECK(!original_service_config_.is_valid);

  ServiceConfig config;
  if (!GetServiceConfig(&config))
    return false;

  if (IsServiceCorrectlyConfigured(config)) {
    RecordServiceInstallResult(
        ServiceInstallResult::kSucceededServiceCorrectlyConfigured);
    return true;
  }

  original_service_config_ = std::move(config);

  bool success = ChangeServiceConfig(ServiceConfig(
      kServiceType, kServiceStartType, kServiceErrorControl,
      service_cmd_line_.GetCommandLineString(), kServiceDependencies));
  if (success) {
    rollback_existing_service_ = true;
    RecordServiceInstallResult(
        ServiceInstallResult::kSucceededChangeServiceConfig);
  } else {
    RecordWin32ApiErrorCode(kChangeServiceConfig);
  }

  return success;
}

bool InstallServiceWorkItemImpl::ReinstallOriginalService() {
  return InstallService(original_service_config_);
}

bool InstallServiceWorkItemImpl::RestoreOriginalServiceConfig() {
  return ChangeServiceConfig(original_service_config_);
}

bool InstallServiceWorkItemImpl::InstallService(const ServiceConfig& config) {
  ScopedScHandle service(::CreateService(
      scm_.Get(), GetCurrentServiceName().c_str(),
      GetCurrentServiceDisplayName().c_str(), kServiceAccess, config.type,
      config.start_type, config.error_control, config.cmd_line.c_str(), nullptr,
      nullptr,
      !config.dependencies.empty() ? config.dependencies.data() : nullptr,
      nullptr, nullptr));
  if (!service.IsValid()) {
    PLOG(WARNING) << "Failed to create service "
                  << GetCurrentServiceName().c_str();
    return false;
  }

  service_ = std::move(service);
  return true;
}

bool InstallServiceWorkItemImpl::ChangeServiceConfig(
    const ServiceConfig& config) {
  DCHECK(service_.IsValid());

  // Change the configuration of the existing service.
  if (!::ChangeServiceConfig(
          service_.Get(), config.type, config.start_type, config.error_control,
          config.cmd_line.c_str(), nullptr, nullptr,
          !config.dependencies.empty() ? config.dependencies.data() : nullptr,
          nullptr, nullptr, nullptr)) {
    PLOG(WARNING) << "Failed to change service config "
                  << GetCurrentServiceName().c_str();
    return false;
  }

  return true;
}

bool InstallServiceWorkItemImpl::DeleteService(ScopedScHandle service) const {
  if (!service.IsValid())
    return false;

  if (!::DeleteService(service.Get())) {
    DWORD error = ::GetLastError();
    PLOG(WARNING) << "DeleteService failed " << GetCurrentServiceName().c_str();
    return error == ERROR_SERVICE_MARKED_FOR_DELETE;
  }

  return true;
}

std::wstring InstallServiceWorkItemImpl::GenerateVersionedServiceName() const {
  const FILETIME filetime = base::Time::Now().ToFileTime();
  return base::StringPrintf(L"%ls%x%x", service_name_.c_str(),
                            filetime.dwHighDateTime, filetime.dwLowDateTime);
}

}  // namespace installer
