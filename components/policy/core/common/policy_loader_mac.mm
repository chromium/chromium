// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/policy_loader_mac.h"

#include <utility>

#include <Foundation/Foundation.h>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/enterprise_util.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/mac/foundation_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/mac_util.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_load_status.h"
#include "components/policy/core/common/policy_loader_common.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/preferences_mac.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/core/common/schema_map.h"

using base::ScopedCFTypeRef;

namespace policy {

namespace {

// Encapsulates logic to determine if enterprise policies should be honored.
bool ShouldHonorPolicies() {
  // Only honor sensitive policies if the Mac is managed or connected to an
  // enterprise.
  // TODO (crbug.com/1322121): Use PlatformManagementService instead.
  return base::IsManagedOrEnterpriseDevice();
}

}  // namespace

PolicyLoaderMac::PolicyLoaderMac(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    const base::FilePath& managed_policy_path,
    MacPreferences* preferences)
    : PolicyLoaderMac(task_runner,
                      managed_policy_path,
                      preferences,
                      kCFPreferencesCurrentApplication) {}

PolicyLoaderMac::PolicyLoaderMac(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    const base::FilePath& managed_policy_path,
    MacPreferences* preferences,
    CFStringRef application_id)
    : AsyncPolicyLoader(task_runner, /*periodic_updates=*/true),
      preferences_(preferences),
      managed_policy_path_(managed_policy_path),
      application_id_(CFStringCreateCopy(kCFAllocatorDefault, application_id)) {
}

PolicyLoaderMac::~PolicyLoaderMac() {
}

void PolicyLoaderMac::InitOnBackgroundThread() {
  if (!managed_policy_path_.empty()) {
    watcher_.Watch(managed_policy_path_,
                   base::FilePathWatcher::Type::kNonRecursive,
                   base::BindRepeating(&PolicyLoaderMac::OnFileUpdated,
                                       base::Unretained(this)));
  }

  base::File::Info file_info;
  bool managed_policy_file_exists = false;
  if (base::GetFileInfo(managed_policy_path_, &file_info) &&
      !file_info.is_directory) {
    managed_policy_file_exists = true;
  }

  base::UmaHistogramBoolean("EnterpriseCheck.IsManagedOrEnterpriseDevice",
                            base::IsManagedOrEnterpriseDevice());

  base::UmaHistogramBoolean("EnterpriseCheck.IsManaged2",
                            managed_policy_file_exists);
  base::UmaHistogramBoolean("EnterpriseCheck.IsEnterpriseUser",
                            base::IsEnterpriseDevice());

  base::UmaHistogramEnumeration("EnterpriseCheck.Mac.IsDeviceMDMEnrolledOld",
                                base::IsDeviceRegisteredWithManagementOld());
  base::UmaHistogramEnumeration("EnterpriseCheck.Mac.IsDeviceMDMEnrolledNew",
                                base::IsDeviceRegisteredWithManagementNew());
  base::DeviceUserDomainJoinState state =
      base::AreDeviceAndUserJoinedToDomain();
  base::UmaHistogramBoolean("EnterpriseCheck.Mac.IsDeviceDomainJoined",
                            state.device_joined);
  base::UmaHistogramBoolean("EnterpriseCheck.Mac.IsCurrentUserDomainUser",
                            state.user_joined);
}

PolicyBundle PolicyLoaderMac::Load() {
  preferences_->AppSynchronize(application_id_);
  PolicyBundle bundle;

  // Load Chrome's policy.
  PolicyMap& chrome_policy =
      bundle.Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()));

  PolicyLoadStatusUmaReporter status;
  bool policy_present = false;
  const Schema* schema =
      schema_map()->GetSchema(PolicyNamespace(POLICY_DOMAIN_CHROME, ""));
  for (Schema::Iterator it = schema->GetPropertiesIterator(); !it.IsAtEnd();
       it.Advance()) {
    base::ScopedCFTypeRef<CFStringRef> name(
        base::SysUTF8ToCFStringRef(it.key()));
    base::ScopedCFTypeRef<CFPropertyListRef> value(
        preferences_->CopyAppValue(name, application_id_));
    if (!value)
      continue;
    policy_present = true;
    bool forced = preferences_->AppValueIsForced(name, application_id_);
    PolicyLevel level =
        forced ? POLICY_LEVEL_MANDATORY : POLICY_LEVEL_RECOMMENDED;
    PolicyScope scope = POLICY_SCOPE_USER;
    if (forced) {
      scope = preferences_->IsManagedPolicyAvailableForMachineScope(name)
                  ? POLICY_SCOPE_MACHINE
                  : POLICY_SCOPE_USER;
    }
    std::unique_ptr<base::Value> policy = PropertyToValue(value);
    if (policy) {
      chrome_policy.Set(it.key(), level, scope, POLICY_SOURCE_PLATFORM,
                        std::move(*policy), nullptr);
    } else {
      status.Add(POLICY_LOAD_STATUS_PARSE_ERROR);
    }
  }

  if (!policy_present)
    status.Add(POLICY_LOAD_STATUS_NO_POLICY);

  // Load policy for the registered components.
  LoadPolicyForDomain(POLICY_DOMAIN_EXTENSIONS, "extensions", &bundle);

  if (!ShouldHonorPolicies())
    FilterSensitivePolicies(&chrome_policy);

  return bundle;
}

base::Time PolicyLoaderMac::LastModificationTime() {
  base::File::Info file_info;
  if (!base::GetFileInfo(managed_policy_path_, &file_info) ||
      file_info.is_directory) {
    return base::Time();
  }

  return file_info.last_modified;
}

#if BUILDFLAG(IS_MAC)

base::FilePath PolicyLoaderMac::GetManagedPolicyPath(CFStringRef bundle_id) {
  // This constructs the path to the plist file in which Mac OS X stores the
  // managed preference for the application. This is undocumented and therefore
  // fragile, but if it doesn't work out, AsyncPolicyLoader has a task that
  // polls periodically in order to reload managed preferences later even if we
  // missed the change.

  base::FilePath path;
  if (!base::mac::GetLocalDirectory(NSLibraryDirectory, &path))
    return base::FilePath();
  path = path.Append(FILE_PATH_LITERAL("Managed Preferences"));
  char* login = getlogin();
  if (!login)
    return base::FilePath();
  path = path.AppendASCII(login);
  return path.Append(base::SysCFStringRefToUTF8(bundle_id) + ".plist");
}

#endif

void PolicyLoaderMac::LoadPolicyForDomain(PolicyDomain domain,
                                          const std::string& domain_name,
                                          PolicyBundle* bundle) {
  std::string id_prefix(base::SysCFStringRefToUTF8(application_id_));
  id_prefix.append(".").append(domain_name).append(".");

  const ComponentMap* components = schema_map()->GetComponents(domain);
  if (!components)
    return;

  for (ComponentMap::const_iterator it = components->begin();
       it != components->end(); ++it) {
    PolicyMap policy;
    LoadPolicyForComponent(id_prefix + it->first, it->second, &policy);
    if (!policy.empty())
      bundle->Get(PolicyNamespace(domain, it->first)).Swap(&policy);
  }
}

void PolicyLoaderMac::LoadPolicyForComponent(
    const std::string& bundle_id_string,
    const Schema& schema,
    PolicyMap* policy) {
  // TODO(joaodasilva): Extensions may be registered in a ComponentMap
  // without a schema, to allow a graceful update of the Legacy Browser Support
  // extension on Windows. Remove this check once that support is removed.
  if (!schema.valid())
    return;

  base::ScopedCFTypeRef<CFStringRef> bundle_id(
      base::SysUTF8ToCFStringRef(bundle_id_string));
  preferences_->AppSynchronize(bundle_id);

  for (Schema::Iterator it = schema.GetPropertiesIterator(); !it.IsAtEnd();
       it.Advance()) {
    base::ScopedCFTypeRef<CFStringRef> pref_name(
        base::SysUTF8ToCFStringRef(it.key()));
    base::ScopedCFTypeRef<CFPropertyListRef> value(
        preferences_->CopyAppValue(pref_name, bundle_id));
    if (!value)
      continue;
    bool forced = preferences_->AppValueIsForced(pref_name, bundle_id);
    PolicyLevel level =
        forced ? POLICY_LEVEL_MANDATORY : POLICY_LEVEL_RECOMMENDED;
    PolicyScope scope = POLICY_SCOPE_USER;
    if (forced) {
      scope = preferences_->IsManagedPolicyAvailableForMachineScope(pref_name)
                  ? POLICY_SCOPE_MACHINE
                  : POLICY_SCOPE_USER;
    }
    std::unique_ptr<base::Value> policy_value = PropertyToValue(value);
    if (policy_value) {
      policy->Set(it.key(), level, scope, POLICY_SOURCE_PLATFORM,
                  std::move(*policy_value), nullptr);
    }
  }
}

void PolicyLoaderMac::OnFileUpdated(const base::FilePath& path, bool error) {
  if (!error)
    Reload(false);
}

}  // namespace policy
