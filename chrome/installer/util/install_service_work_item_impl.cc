// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/installer/util/install_service_work_item_impl.h"

#include <cguid.h>
#include <guiddef.h>

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/win/registry.h"
#include "base/win/win_util.h"
#include "chrome/installer/util/registry_util.h"

using base::win::RegKey;

namespace installer {

namespace {

constexpr uint32_t kServiceType = SERVICE_WIN32_OWN_PROCESS;
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

std::wstring GetComRegistryPath(std::wstring_view hive, const GUID& guid) {
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

InstallServiceWorkItemImpl::ServiceConfig::ServiceConfig() = default;

InstallServiceWorkItemImpl::ServiceConfig::ServiceConfig(
    uint32_t service_type,
    uint32_t service_start_type,
    uint32_t service_error_control,
    const std::wstring& service_cmd_line,
    const wchar_t* dependencies_multi_sz,
    const std::wstring& service_display_name)
    : is_valid(true),
      type(service_type),
      start_type(service_start_type),
      error_control(service_error_control),
      cmd_line(service_cmd_line),
      dependencies(MultiSzToVector(dependencies_multi_sz)),
      display_name(service_display_name) {}

InstallServiceWorkItemImpl::ServiceConfig::ServiceConfig(
    const ServiceConfig& rhs) = default;
InstallServiceWorkItemImpl::ServiceConfig::ServiceConfig(ServiceConfig&& rhs) =
    default;

InstallServiceWorkItemImpl::ServiceConfig::~ServiceConfig() = default;

bool operator==(const InstallServiceWorkItemImpl::ServiceConfig& lhs,
                const InstallServiceWorkItemImpl::ServiceConfig& rhs) {
  return lhs.type == rhs.type && lhs.start_type == rhs.start_type &&
         lhs.error_control == rhs.error_control &&
         !_wcsicmp(lhs.cmd_line.c_str(), rhs.cmd_line.c_str()) &&
         lhs.dependencies == rhs.dependencies &&
         !_wcsicmp(lhs.display_name.c_str(), rhs.display_name.c_str());
}

InstallServiceWorkItemImpl::InstallServiceWorkItemImpl(
    const std::wstring& service_name,
    const std::wstring& display_name,
    const std::wstring& description,
    uint32_t start_type,
    const base::CommandLine& service_cmd_line,
    const base::CommandLine& com_service_cmd_line_args,
    const std::wstring& registry_path,
    const std::vector<GUID>& clsids,
    const std::vector<GUID>& iids)
    : com_registration_work_items_(WorkItem::CreateWorkItemList()),
      service_name_(service_name),
      display_name_(display_name),
      description_(description),
      start_type_(start_type),
      service_cmd_line_(service_cmd_line),
      com_service_cmd_line_args_(com_service_cmd_line_args),
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
    return false;
  }

  if (!OpenService()) {
    VPLOG(1) << "Attempting to install new service following failure to open";
    if (InstallNewService())
      return true;

    PLOG(ERROR) << "Failed to install service "
                << GetCurrentServiceName().c_str();
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
    if (!DeleteService(std::move(original_service)))
      original_service_still_exists_ = true;

    return true;
  }

  PLOG(ERROR) << "Failed to install service with new name "
              << GetCurrentServiceName().c_str();
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

    base::CommandLine::StringType com_service_args_string =
        com_service_cmd_line_args_.GetArgumentsString();
    if (!com_service_args_string.empty()) {
      com_registration_work_items_->AddSetRegValueWorkItem(
          HKEY_LOCAL_MACHINE, appid_reg_path, WorkItem::kWow64Default,
          L"ServiceParameters", com_service_args_string, true);
    } else {
      com_registration_work_items_->AddDeleteRegValueWorkItem(
          HKEY_LOCAL_MACHINE, appid_reg_path, WorkItem::kWow64Default,
          L"ServiceParameters");
    }
  }

  for (const auto& iid : iids_) {
    const std::wstring iid_reg_path = GetComIidRegistryPath(iid);
    const std::wstring typelib_reg_path = GetComTypeLibRegistryPath(iid);
    const std::wstring iid_string = base::win::WStringFromGUID(iid);

    for (const auto& key_flag : {KEY_WOW64_32KEY, KEY_WOW64_64KEY}) {
      // Registering the Ole Automation marshaler with the CLSID
      // {00020424-0000-0000-C000-000000000046} as the proxy/stub for the
      // interface.
      {
        const std::wstring path = iid_reg_path + L"\\ProxyStubClsid32";
        com_registration_work_items_->AddCreateRegKeyWorkItem(
            HKEY_LOCAL_MACHINE, path, key_flag);
        com_registration_work_items_->AddSetRegValueWorkItem(
            HKEY_LOCAL_MACHINE, path, key_flag, L"",
            L"{00020424-0000-0000-C000-000000000046}", true);
      }
      {
        const std::wstring path = iid_reg_path + L"\\TypeLib";
        com_registration_work_items_->AddCreateRegKeyWorkItem(
            HKEY_LOCAL_MACHINE, path, key_flag);
        com_registration_work_items_->AddSetRegValueWorkItem(
            HKEY_LOCAL_MACHINE, path, key_flag, L"", iid_string, true);
        com_registration_work_items_->AddSetRegValueWorkItem(
            HKEY_LOCAL_MACHINE, path, key_flag, L"Version", L"1.0", true);
      }
      com_registration_work_items_->AddSetRegValueWorkItem(
          HKEY_LOCAL_MACHINE, iid_reg_path, key_flag, L"",
          base::StrCat({L"Interface ", iid_string}), true);
    }

    // The TypeLib registration for the Ole Automation marshaler.
    for (const auto& path : {typelib_reg_path + L"\\1.0\\0\\win32",
                             typelib_reg_path + L"\\1.0\\0\\win64"}) {
      com_registration_work_items_->AddCreateRegKeyWorkItem(
          HKEY_LOCAL_MACHINE, path, WorkItem::kWow64Default);
      com_registration_work_items_->AddSetRegValueWorkItem(
          HKEY_LOCAL_MACHINE, path, WorkItem::kWow64Default, L"",
          service_cmd_line_.GetProgram().value(), true);
    }
    com_registration_work_items_->AddSetRegValueWorkItem(
        HKEY_LOCAL_MACHINE, typelib_reg_path + L"\\1.0",
        WorkItem::kWow64Default, L"",
        base::StrCat({L"TypeLib for Interface ", iid_string}), true);
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
    RestoreOriginalServiceConfig();
    return;
  }

  DCHECK(rollback_new_service_);
  DCHECK(service_.IsValid());

  // Delete the newly created service.
  DeleteCurrentService();

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
  ReinstallOriginalService();
}

bool InstallServiceWorkItemImpl::DeleteServiceImpl() {
  // Uninstall the elevation service.
  for (const auto& clsid : clsids_) {
    for (const auto& reg_path :
         {GetComClsidRegistryPath(clsid), GetComAppidRegistryPath(clsid)}) {
      installer::DeleteRegistryKey(HKEY_LOCAL_MACHINE, reg_path,
                                   WorkItem::kWow64Default);
    }
  }

  for (const auto& iid : iids_) {
    {
      const std::wstring reg_path = GetComIidRegistryPath(iid);
      for (const auto& key_flag : {KEY_WOW64_32KEY, KEY_WOW64_64KEY}) {
        installer::DeleteRegistryKey(HKEY_LOCAL_MACHINE, reg_path, key_flag);
      }
    }
    {
      const std::wstring reg_path = GetComTypeLibRegistryPath(iid);
      installer::DeleteRegistryKey(HKEY_LOCAL_MACHINE, reg_path,
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

InstallServiceWorkItemImpl::ServiceConfig
InstallServiceWorkItemImpl::MakeUpgradeServiceConfig(
    const ServiceConfig& original_config) {
  ServiceConfig new_config(kServiceType, start_type_, kServiceErrorControl,
                           service_cmd_line_.GetCommandLineString(),
                           kServiceDependencies,
                           GetCurrentServiceDisplayName());

  if (original_config.type == new_config.type)
    new_config.type = SERVICE_NO_CHANGE;
  if (original_config.start_type == new_config.start_type)
    new_config.start_type = SERVICE_NO_CHANGE;
  if (original_config.error_control == new_config.error_control)
    new_config.error_control = SERVICE_NO_CHANGE;
  if (!_wcsicmp(original_config.cmd_line.c_str(),
                new_config.cmd_line.c_str())) {
    new_config.cmd_line.clear();
  }
  if (original_config.dependencies == new_config.dependencies)
    new_config.dependencies.clear();
  if (!_wcsicmp(original_config.display_name.c_str(),
                new_config.display_name.c_str())) {
    new_config.display_name.clear();
  }

  return new_config;
}

bool InstallServiceWorkItemImpl::IsUpgradeNeeded(
    const ServiceConfig& new_config) {
  ServiceConfig default_config;
  default_config.is_valid = true;
  return !(new_config == default_config);
}

bool InstallServiceWorkItemImpl::ChangeServiceConfig(
    const ServiceConfig& config) {
  DCHECK(service_.IsValid());

  // Change the configuration of the existing service.
  // If the service is deleted, ::ChangeServiceConfig will fail with the error
  // ERROR_SERVICE_MARKED_FOR_DELETE.
  if (!::ChangeServiceConfig(
          service_.Get(), config.type, config.start_type, config.error_control,
          !config.cmd_line.empty() ? config.cmd_line.c_str() : nullptr,
          /*lpLoadOrderGroup=*/nullptr,
          /*lpdwTagId=*/nullptr,
          !config.dependencies.empty() ? config.dependencies.data() : nullptr,
          /*lpServiceStartName=*/nullptr, /*lpPassword=*/nullptr,
          !config.display_name.empty() ? config.display_name.c_str()
                                       : nullptr)) {
    PLOG(WARNING) << "Failed to change service config "
                  << GetCurrentServiceName().c_str();
    return false;
  }

  return true;
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
      service_config->lpDependencies ? service_config->lpDependencies : L"",
      service_config->lpDisplayName ? service_config->lpDisplayName : L"");
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
  return base::StrCat({display_name_, L" (", GetCurrentServiceName(), L")"});
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

// static
bool InstallServiceWorkItemImpl::IsComServiceInstalled(const GUID& clsid) {
  std::wstring appid_reg_path = GetComAppidRegistryPath(clsid);
  return base::win::RegKey(HKEY_LOCAL_MACHINE, appid_reg_path.c_str(),
                           KEY_QUERY_VALUE)
      .HasValue(L"LocalService");
}

std::wstring InstallServiceWorkItemImpl::GetCurrentServiceDescription() const {
  DCHECK(service_.IsValid());

  constexpr uint32_t kMaxQueryConfigBufferBytes = 8 * 1024;

  // ::QueryServiceConfig2 expects a buffer of at most 8K bytes, according to
  // documentation. While the size of the buffer can be dynamically computed,
  // we just assume the maximum size for simplicity.
  auto buffer = std::make_unique<uint8_t[]>(kMaxQueryConfigBufferBytes);
  DWORD bytes_needed_ignored = 0;
  SERVICE_DESCRIPTION* description =
      reinterpret_cast<SERVICE_DESCRIPTION*>(buffer.get());
  if (!::QueryServiceConfig2(service_.Get(), SERVICE_CONFIG_DESCRIPTION,
                             buffer.get(), kMaxQueryConfigBufferBytes,
                             &bytes_needed_ignored)) {
    PLOG(ERROR) << "QueryServiceConfig2 failed "
                << GetCurrentServiceName().c_str();
    return {};
  }

  return description->lpDescription ? description->lpDescription : L"";
}

void InstallServiceWorkItemImpl::SetDescription() {
  DCHECK(service_.IsValid());

  if (description_.empty()) {
    return;
  }

  std::wstring desc = description_;
  SERVICE_DESCRIPTION description = {desc.data()};
  if (!::ChangeServiceConfig2(service_.Get(), SERVICE_CONFIG_DESCRIPTION,
                              &description)) {
    PLOG(WARNING) << "Failed to set service description: "
                  << GetCurrentServiceName().c_str() << ": " << description_;
  }
}

bool InstallServiceWorkItemImpl::InstallNewService() {
  DCHECK(!service_.IsValid());
  bool success = InstallService(
      ServiceConfig(kServiceType, start_type_, kServiceErrorControl,
                    service_cmd_line_.GetCommandLineString(),
                    kServiceDependencies, GetCurrentServiceDisplayName()));
  if (success)
    rollback_new_service_ = true;

  SetDescription();
  return success;
}

bool InstallServiceWorkItemImpl::UpgradeService() {
  DCHECK(service_.IsValid());
  DCHECK(!original_service_config_.is_valid);

  ServiceConfig original_config;
  if (!GetServiceConfig(&original_config))
    return false;

  ServiceConfig new_config = MakeUpgradeServiceConfig(original_config);
  const bool upgrade_needed = IsUpgradeNeeded(new_config);
  if (upgrade_needed) {
    original_service_config_ = std::move(original_config);
  } else {
    // In order to determine whether the Service is correctly installed as
    // opposed to being in a "deleted" state, we attempt to change just the
    // display name in the service configuration even if it is correctly
    // configured.
    new_config.display_name = original_config.display_name;
  }

  // If the service is deleted, `ChangeServiceConfig()` will return false.
  bool success = ChangeServiceConfig(new_config);
  if (success && upgrade_needed)
    rollback_existing_service_ = true;

  SetDescription();
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
      scm_.Get(), GetCurrentServiceName().c_str(), config.display_name.c_str(),
      kServiceAccess, config.type, config.start_type, config.error_control,
      config.cmd_line.c_str(), nullptr, nullptr,
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
  return service_name_ +
         base::ASCIIToWide(base::StringPrintf("%lx%lx", filetime.dwHighDateTime,
                                              filetime.dwLowDateTime));
}

}  // namespace installer
