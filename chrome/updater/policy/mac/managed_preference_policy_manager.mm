// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/policy/mac/managed_preference_policy_manager.h"

#include <optional>
#include <string>
#include <vector>

#include "base/apple/bridging.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/enterprise_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/sys_string_conversions.h"
#include "base/time/time.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/policy/mac/managed_preference_policy_manager_impl.h"
#include "chrome/updater/policy/manager.h"

namespace {
NSString* const kManagedPreferencesUpdatePolicies = @"updatePolicies";
NSString* const kKeystoneSharedPreferenceSuite = @"com.google.Keystone";
}  // namespace

namespace updater {

class ManagedPreferencePolicyManager : public PolicyManagerInterface {
 public:
  ManagedPreferencePolicyManager(
      CRUUpdatePolicyDictionary* policy,
      const std::optional<bool>& override_is_managed_device);
  ManagedPreferencePolicyManager(const ManagedPreferencePolicyManager&) =
      delete;
  ManagedPreferencePolicyManager& operator=(
      const ManagedPreferencePolicyManager&) = delete;

  // Overrides for PolicyManagerInterface.
  std::string source() const override;

  bool HasActiveDevicePolicies() const override;

  std::optional<bool> CloudPolicyOverridesPlatformPolicy() const override;
  std::optional<base::TimeDelta> GetLastCheckPeriod() const override;
  std::optional<UpdatesSuppressedTimes> GetUpdatesSuppressedTimes()
      const override;
  std::optional<std::string> GetDownloadPreference() const override;
  std::optional<int> GetPackageCacheSizeLimitMBytes() const override;
  std::optional<int> GetPackageCacheExpirationTimeDays() const override;
  std::optional<int> GetEffectivePolicyForAppInstalls(
      const std::string& app_id) const override;
  std::optional<int> GetEffectivePolicyForAppUpdates(
      const std::string& app_id) const override;
  std::optional<std::string> GetTargetVersionPrefix(
      const std::string& app_id) const override;
  std::optional<bool> IsRollbackToTargetVersionAllowed(
      const std::string& app_id) const override;
  std::optional<std::string> GetProxyMode() const override;
  std::optional<std::string> GetProxyPacUrl() const override;
  std::optional<std::string> GetProxyServer() const override;
  std::optional<std::string> GetTargetChannel(
      const std::string& app_id) const override;
  std::optional<std::vector<std::string>> GetForceInstallApps() const override;
  std::optional<std::vector<std::string>> GetAppsWithPolicy() const override;

 private:
  ~ManagedPreferencePolicyManager() override;

  CRUManagedPreferencePolicyManager* __strong impl_;
  const bool is_managed_device_;
};

ManagedPreferencePolicyManager::ManagedPreferencePolicyManager(
    CRUUpdatePolicyDictionary* policyDict,
    const std::optional<bool>& override_is_managed_device)
    : impl_([[CRUManagedPreferencePolicyManager alloc]
          initWithDictionary:policyDict]),
      is_managed_device_(override_is_managed_device.value_or(
          base::IsManagedOrEnterpriseDevice())) {}

ManagedPreferencePolicyManager::~ManagedPreferencePolicyManager() = default;

bool ManagedPreferencePolicyManager::HasActiveDevicePolicies() const {
  return is_managed_device_ && impl_.hasActivePolicy;
}

std::string ManagedPreferencePolicyManager::source() const {
  return base::SysNSStringToUTF8(impl_.source);
}

std::optional<bool>
ManagedPreferencePolicyManager::CloudPolicyOverridesPlatformPolicy() const {
  return std::nullopt;
}

std::optional<base::TimeDelta>
ManagedPreferencePolicyManager::GetLastCheckPeriod() const {
  int minutes = [impl_ lastCheckPeriodMinutes];
  return minutes != kPolicyNotSet
             ? std::optional<base::TimeDelta>(base::Minutes(minutes))
             : std::nullopt;
}

std::optional<UpdatesSuppressedTimes>
ManagedPreferencePolicyManager::GetUpdatesSuppressedTimes() const {
  UpdatesSuppressedTimes suppressed_times = [impl_ updatesSuppressed];
  return suppressed_times.valid()
             ? std::optional<UpdatesSuppressedTimes>(suppressed_times)
             : std::nullopt;
}

std::optional<std::string>
ManagedPreferencePolicyManager::GetDownloadPreference() const {
  NSString* value = [impl_ downloadPreference];
  return value ? std::optional<std::string>(base::SysNSStringToUTF8(value))
               : std::nullopt;
}

std::optional<int>
ManagedPreferencePolicyManager::GetPackageCacheSizeLimitMBytes() const {
  return std::nullopt;  // Not supported on Mac.
}

std::optional<int>
ManagedPreferencePolicyManager::GetPackageCacheExpirationTimeDays() const {
  return std::nullopt;  // Not supported on Mac.
}

std::optional<int>
ManagedPreferencePolicyManager::GetEffectivePolicyForAppInstalls(
    const std::string& app_id) const {
  return std::nullopt;  // Not supported on Mac.
}

std::optional<int>
ManagedPreferencePolicyManager::GetEffectivePolicyForAppUpdates(
    const std::string& app_id) const {
  // Check app-specific settings first.
  int update_policy = [impl_ appUpdatePolicy:base::SysUTF8ToNSString(app_id)];
  if (update_policy != kPolicyNotSet) {
    return update_policy;
  }

  // Then fallback to global-level policy if needed.
  update_policy = [impl_ defaultUpdatePolicy];
  return update_policy != kPolicyNotSet ? std::optional<int>(update_policy)
                                        : std::nullopt;
}

std::optional<std::string>
ManagedPreferencePolicyManager::GetTargetVersionPrefix(
    const std::string& app_id) const {
  NSString* value = [impl_ targetVersionPrefix:base::SysUTF8ToNSString(app_id)];
  return value ? std::optional<std::string>(base::SysNSStringToUTF8(value))
               : std::nullopt;
}

std::optional<bool>
ManagedPreferencePolicyManager::IsRollbackToTargetVersionAllowed(
    const std::string& app_id) const {
  int rollback_policy =
      [impl_ rollbackToTargetVersion:base::SysUTF8ToNSString(app_id)];
  return rollback_policy != kPolicyNotSet
             ? std::optional<bool>(rollback_policy != 0)
             : std::nullopt;
}

std::optional<std::string> ManagedPreferencePolicyManager::GetProxyMode()
    const {
  NSString* value = [impl_ proxyMode];
  return value ? std::optional<std::string>(base::SysNSStringToUTF8(value))
               : std::nullopt;
}

std::optional<std::string> ManagedPreferencePolicyManager::GetProxyPacUrl()
    const {
  NSString* value = [impl_ proxyPacURL];
  return value ? std::optional<std::string>(base::SysNSStringToUTF8(value))
               : std::nullopt;
}

std::optional<std::string> ManagedPreferencePolicyManager::GetProxyServer()
    const {
  NSString* value = [impl_ proxyServer];
  return value ? std::optional<std::string>(base::SysNSStringToUTF8(value))
               : std::nullopt;
}

std::optional<std::string> ManagedPreferencePolicyManager::GetTargetChannel(
    const std::string& app_id) const {
  NSString* value = [impl_ targetChannel:base::SysUTF8ToNSString(app_id)];
  return value ? std::optional<std::string>(base::SysNSStringToUTF8(value))
               : std::nullopt;
}

std::optional<std::vector<std::string>>
ManagedPreferencePolicyManager::GetForceInstallApps() const {
  return std::nullopt;
}

std::optional<std::vector<std::string>>
ManagedPreferencePolicyManager::GetAppsWithPolicy() const {
  NSArray<NSString*>* apps_with_policy = [impl_ appsWithPolicy];
  if (!apps_with_policy) {
    return std::nullopt;
  }

  std::vector<std::string> app_ids;
  for (NSString* app in apps_with_policy) {
    app_ids.push_back(base::SysNSStringToUTF8(app));
  }

  return app_ids;
}

NSDictionary* ReadManagedPreferencePolicyDictionary() {
  base::apple::ScopedCFTypeRef<CFPropertyListRef> policies(
      CFPreferencesCopyAppValue(
          base::apple::NSToCFPtrCast(kManagedPreferencesUpdatePolicies),
          base::apple::NSToCFPtrCast(kKeystoneSharedPreferenceSuite)));
  if (!policies) {
    return nil;
  }

  if (!CFPreferencesAppValueIsForced(
          base::apple::NSToCFPtrCast(kManagedPreferencesUpdatePolicies),
          base::apple::NSToCFPtrCast(kKeystoneSharedPreferenceSuite))) {
    return nil;
  }

  if (CFGetTypeID(policies.get()) != CFDictionaryGetTypeID()) {
    return nil;
  }

  return base::apple::CFToNSOwnershipCast((CFDictionaryRef)policies.release());
}

scoped_refptr<PolicyManagerInterface> CreateManagedPreferencePolicyManager(
    const std::optional<bool>& override_is_managed_device) {
  NSDictionary* policyDict = ReadManagedPreferencePolicyDictionary();
  return base::MakeRefCounted<ManagedPreferencePolicyManager>(
      policyDict, override_is_managed_device);
}

}  // namespace updater
