// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/setup/install_service_work_item_impl.h"

#include <string.h>
#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/win/registry.h"
#include "chrome/install_static/install_util.h"

using base::win::RegKey;

namespace installer {

namespace {

constexpr uint32_t kServiceType = SERVICE_WIN32_OWN_PROCESS;
constexpr uint32_t kServiceStartType = SERVICE_DEMAND_START;
constexpr uint32_t kServiceErrorControl = SERVICE_ERROR_NORMAL;
constexpr base::char16 kServiceDependencies[] = L"RPCSS\0";

// For the service handle, all permissions that could possibly be used in all
// Do/Rollback scenarios are requested since the handle is reused.
constexpr uint32_t kServiceAccess =
    DELETE | SERVICE_QUERY_CONFIG | SERVICE_CHANGE_CONFIG;

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
    const base::string16& service_cmd_line,
    const base::char16* dependencies_multi_sz)
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
    const base::string16& service_name,
    const base::string16& display_name,
    const base::CommandLine& service_cmd_line)
    : service_name_(service_name),
      display_name_(display_name),
      service_cmd_line_(service_cmd_line.GetCommandLineString()),
      rollback_existing_service_(false),
      rollback_new_service_(false),
      original_service_still_exists_(false) {}

InstallServiceWorkItemImpl::~InstallServiceWorkItemImpl() = default;

bool InstallServiceWorkItemImpl::DoImpl() {
  scm_.Set(::OpenSCManager(nullptr, nullptr,
                           SC_MANAGER_CONNECT | SC_MANAGER_CREATE_SERVICE));
  if (!scm_.IsValid()) {
    DPLOG(ERROR) << "::OpenSCManager Failed";
    return false;
  }

  if (!OpenService())
    return InstallNewService();

  // It is preferable to do a lightweight upgrade of the existing service,
  // instead of deleting and recreating a new service, since it is less
  // likely to fail. Less intrusive to the SCM and to AV/Anti-malware programs.
  if (UpgradeService())
    return true;

  // Save the original service name. Then create a new service name so as to not
  // conflict with the previous one to be safe, then install the new service.
  original_service_name_ = GetCurrentServiceName();
  LOG_IF(WARNING, !CreateAndSetServiceName());

  ScopedScHandle original_service = std::move(service_);
  if (InstallNewService()) {
    // Delete the previous version of the service.
    if (!DeleteService(std::move(original_service)))
      original_service_still_exists_ = true;

    return true;
  }

  return false;
}

void InstallServiceWorkItemImpl::RollbackImpl() {
  DCHECK(!(rollback_existing_service_ && rollback_new_service_));

  if (!rollback_existing_service_ && !rollback_new_service_)
    return;

  if (rollback_existing_service_) {
    DCHECK(service_.IsValid());
    DCHECK(original_service_config_.is_valid);
    LOG_IF(WARNING, !RestoreOriginalServiceConfig());
    return;
  }

  DCHECK(rollback_new_service_);
  DCHECK(service_.IsValid());

  // Delete the newly created service.
  // TODO(ganesh): If this Delete fails, there will be an extra service. We need
  // to use UMA to record occurrences. And have cleanup code that is run on
  // subsequent updates to delete stale services.
  LOG_IF(WARNING, !DeleteCurrentService());

  if (original_service_name_.empty())
    return;

  if (original_service_still_exists_) {
    // Set only the service name back to original_service_name_ and return.
    LOG_IF(WARNING, !SetServiceName(original_service_name_));
    return;
  }

  // Recreate original service with a new service name to avoid possible SCM
  // issues with reusing the old name.
  LOG_IF(WARNING, !CreateAndSetServiceName());
  LOG_IF(WARNING, !ReinstallOriginalService());
}

bool InstallServiceWorkItemImpl::DeleteServiceImpl() {
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
  auto result = key.Open(HKEY_LOCAL_MACHINE,
                         install_static::GetClientStateKeyPath().c_str(),
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
         !_wcsicmp(config.cmd_line.c_str(), service_cmd_line_.c_str()) &&
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
    DPLOG(ERROR) << "QueryServiceConfig failed";
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
  const base::string16 versioned_service_name(GenerateVersionedServiceName());
  return SetServiceName(versioned_service_name);
}

bool InstallServiceWorkItemImpl::SetServiceName(
    const base::string16& service_name) const {
  base::win::RegKey key;

  // This assumes that a WorkItem to create the key has already executed before
  // this WorkItem. this is generally true since one is added in
  // AddUninstallShortcutWorkItems.
  auto result = key.Open(HKEY_LOCAL_MACHINE,
                         install_static::GetClientStateKeyPath().c_str(),
                         KEY_SET_VALUE | KEY_WOW64_32KEY);
  if (result != ERROR_SUCCESS) {
    ::SetLastError(result);
    DPLOG(ERROR) << "key.Open failed";
    return false;
  }

  result = key.WriteValue(service_name_.c_str(), service_name.c_str());
  if (result != ERROR_SUCCESS) {
    ::SetLastError(result);
    DPLOG(ERROR) << "key.WriteValue failed";
    return false;
  }

  return true;
}

base::string16 InstallServiceWorkItemImpl::GetCurrentServiceName() const {
  base::win::RegKey key;

  auto result = key.Open(HKEY_LOCAL_MACHINE,
                         install_static::GetClientStateKeyPath().c_str(),
                         KEY_QUERY_VALUE | KEY_WOW64_32KEY);
  if (result != ERROR_SUCCESS)
    return service_name_;

  base::string16 versioned_service_name;
  key.ReadValue(service_name_.c_str(), &versioned_service_name);
  return versioned_service_name.empty() ? service_name_
                                        : versioned_service_name;
}

std::vector<base::char16> InstallServiceWorkItemImpl::MultiSzToVector(
    const base::char16* multi_sz) {
  if (!multi_sz)
    return std::vector<base::char16>();

  if (!*multi_sz)
    return std::vector<base::char16>(1, L'\0');

  // Scan forward to the second terminating '\0' at the end of the list of
  // strings in the multi-sz.
  const base::char16* scan = multi_sz;
  do {
    scan += wcslen(scan) + 1;
  } while (*scan);

  return std::vector<base::char16>(multi_sz, scan + 1);
}

bool InstallServiceWorkItemImpl::InstallNewService() {
  DCHECK(!service_.IsValid());
  bool success = InstallService(
      ServiceConfig(kServiceType, kServiceStartType, kServiceErrorControl,
                    service_cmd_line_, kServiceDependencies));
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

  if (IsServiceCorrectlyConfigured(config))
    return true;

  original_service_config_ = std::move(config);

  bool success = ChangeServiceConfig(
      ServiceConfig(kServiceType, kServiceStartType, kServiceErrorControl,
                    service_cmd_line_, kServiceDependencies));
  if (success)
    rollback_existing_service_ = true;

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
      scm_.Get(), GetCurrentServiceName().c_str(), display_name_.c_str(),
      kServiceAccess, config.type, config.start_type, config.error_control,
      config.cmd_line.c_str(), nullptr, nullptr,
      !config.dependencies.empty() ? config.dependencies.data() : nullptr,
      nullptr, nullptr));
  if (!service.IsValid()) {
    DPLOG(WARNING) << "Failed to create service";
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
    DPLOG(WARNING) << "Failed to change service config";
    return false;
  }

  return true;
}

bool InstallServiceWorkItemImpl::DeleteService(ScopedScHandle service) const {
  if (!service.IsValid())
    return false;

  if (!::DeleteService(service.Get())) {
    DWORD error = ::GetLastError();
    DPLOG(WARNING) << "DeleteService failed";
    return error == ERROR_SERVICE_MARKED_FOR_DELETE;
  }

  return true;
}

base::string16 InstallServiceWorkItemImpl::GenerateVersionedServiceName()
    const {
  const FILETIME filetime = base::Time::Now().ToFileTime();
  return base::StringPrintf(L"%ls%x%x", service_name_.c_str(),
                            filetime.dwHighDateTime, filetime.dwLowDateTime);
}

}  // namespace installer
