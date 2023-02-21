// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/policy/mac/managed_preference_policy_manager.h"

#include <string>
#include <vector>

#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_nsobject.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/sys_string_conversions.h"
#include "base/time/time.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/policy/mac/managed_preference_policy_manager_impl.h"
#include "chrome/updater/policy/manager.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace updater {

static NSString* const kManagedPreferencesUpdatePolicies = @"updatePolicies";
static NSString* const kKeystoneSharedPreferenceSuite = @"com.google.Keystone";

class ManagedPreferencePolicyManager : public PolicyManagerInterface {
 public:
  explicit ManagedPreferencePolicyManager(CRUUpdatePolicyDictionary* policy);
  ManagedPreferencePolicyManager(const ManagedPreferencePolicyManager&) =
      delete;
  ManagedPreferencePolicyManager& operator=(
      const ManagedPreferencePolicyManager&) = delete;

  // Overrides for PolicyManagerInterface.
  std::string source() const override;

  bool HasActiveDevicePolicies() const override;

  absl::optional<base::TimeDelta> GetLastCheckPeriod() const override;
  absl::optional<UpdatesSuppressedTimes> GetUpdatesSuppressedTimes()
      const override;
  absl::optional<std::string> GetDownloadPreferenceGroupPolicy() const override;
  absl::optional<int> GetPackageCacheSizeLimitMBytes() const override;
  absl::optional<int> GetPackageCacheExpirationTimeDays() const override;
  absl::optional<int> GetEffectivePolicyForAppInstalls(
      const std::string& app_id) const override;
  absl::optional<int> GetEffectivePolicyForAppUpdates(
      const std::string& app_id) const override;
  absl::optional<std::string> GetTargetVersionPrefix(
      const std::string& app_id) const override;
  absl::optional<bool> IsRollbackToTargetVersionAllowed(
      const std::string& app_id) const override;
  absl::optional<std::string> GetProxyMode() const override;
  absl::optional<std::string> GetProxyPacUrl() const override;
  absl::optional<std::string> GetProxyServer() const override;
  absl::optional<std::string> GetTargetChannel(
      const std::string& app_id) const override;
  absl::optional<std::vector<std::string>> GetForceInstallApps() const override;
  absl::optional<std::vector<std::string>> GetAppsWithPolicy() const override;

 private:
  ~ManagedPreferencePolicyManager() override;
  base::scoped_nsobject<CRUManagedPreferencePolicyManager> impl_;
};

ManagedPreferencePolicyManager::ManagedPreferencePolicyManager(
    CRUUpdatePolicyDictionary* policyDict)
    : impl_([[CRUManagedPreferencePolicyManager alloc]
          initWithDictionary:policyDict]) {}

ManagedPreferencePolicyManager::~ManagedPreferencePolicyManager() = default;

bool ManagedPreferencePolicyManager::HasActiveDevicePolicies() const {
  return [impl_ managed];
}

std::string ManagedPreferencePolicyManager::source() const {
  return base::SysNSStringToUTF8([impl_ source]);
}

absl::optional<base::TimeDelta>
ManagedPreferencePolicyManager::GetLastCheckPeriod() const {
  int minutes = [impl_ lastCheckPeriodMinutes];
  return minutes != kPolicyNotSet
             ? absl::optional<base::TimeDelta>(base::Minutes(minutes))
             : absl::nullopt;
}

absl::optional<UpdatesSuppressedTimes>
ManagedPreferencePolicyManager::GetUpdatesSuppressedTimes() const {
  UpdatesSuppressedTimes suppressed_times = [impl_ updatesSuppressed];
  return suppressed_times.valid()
             ? absl::optional<UpdatesSuppressedTimes>(suppressed_times)
             : absl::nullopt;
}

absl::optional<std::string>
ManagedPreferencePolicyManager::GetDownloadPreferenceGroupPolicy() const {
  NSString* value = [impl_ downloadPreference];
  return value ? absl::optional<std::string>(base::SysNSStringToUTF8(value))
               : absl::nullopt;
}

absl::optional<int>
ManagedPreferencePolicyManager::GetPackageCacheSizeLimitMBytes() const {
  return absl::nullopt;  // Not supported on Mac.
}

absl::optional<int>
ManagedPreferencePolicyManager::GetPackageCacheExpirationTimeDays() const {
  return absl::nullopt;  // Not supported on Mac.
}

absl::optional<int>
ManagedPreferencePolicyManager::GetEffectivePolicyForAppInstalls(
    const std::string& app_id) const {
  return absl::nullopt;  // Not supported on Mac.
}

absl::optional<int>
ManagedPreferencePolicyManager::GetEffectivePolicyForAppUpdates(
    const std::string& app_id) const {
  // Check app-specific settings first.
  int update_policy = [impl_ appUpdatePolicy:base::SysUTF8ToNSString(app_id)];
  if (update_policy != kPolicyNotSet)
    return update_policy;

  // Then fallback to global-level policy if needed.
  update_policy = [impl_ defaultUpdatePolicy];
  return update_policy != kPolicyNotSet ? absl::optional<int>(update_policy)
                                        : absl::nullopt;
}

absl::optional<std::string>
ManagedPreferencePolicyManager::GetTargetVersionPrefix(
    const std::string& app_id) const {
  NSString* value = [impl_ targetVersionPrefix:base::SysUTF8ToNSString(app_id)];
  return value ? absl::optional<std::string>(base::SysNSStringToUTF8(value))
               : absl::nullopt;
}

absl::optional<bool>
ManagedPreferencePolicyManager::IsRollbackToTargetVersionAllowed(
    const std::string& app_id) const {
  int rollback_policy =
      [impl_ rollbackToTargetVersion:base::SysUTF8ToNSString(app_id)];
  return rollback_policy != kPolicyNotSet
             ? absl::optional<bool>(rollback_policy != 0)
             : absl::nullopt;
}

absl::optional<std::string> ManagedPreferencePolicyManager::GetProxyMode()
    const {
  NSString* value = [impl_ proxyMode];
  return value ? absl::optional<std::string>(base::SysNSStringToUTF8(value))
               : absl::nullopt;
}

absl::optional<std::string> ManagedPreferencePolicyManager::GetProxyPacUrl()
    const {
  NSString* value = [impl_ proxyPacURL];
  return value ? absl::optional<std::string>(base::SysNSStringToUTF8(value))
               : absl::nullopt;
}

absl::optional<std::string> ManagedPreferencePolicyManager::GetProxyServer()
    const {
  NSString* value = [impl_ proxyServer];
  return value ? absl::optional<std::string>(base::SysNSStringToUTF8(value))
               : absl::nullopt;
}

absl::optional<std::string> ManagedPreferencePolicyManager::GetTargetChannel(
    const std::string& app_id) const {
  NSString* value = [impl_ targetChannel:base::SysUTF8ToNSString(app_id)];
  return value ? absl::optional<std::string>(base::SysNSStringToUTF8(value))
               : absl::nullopt;
}

absl::optional<std::vector<std::string>>
ManagedPreferencePolicyManager::GetForceInstallApps() const {
  return absl::nullopt;
}

absl::optional<std::vector<std::string>>
ManagedPreferencePolicyManager::GetAppsWithPolicy() const {
  NSArray<NSString*>* apps_with_policy = [impl_ appsWithPolicy];
  if (!apps_with_policy) {
    return absl::nullopt;
  }

  std::vector<std::string> app_ids;
  for (NSString* app in apps_with_policy) {
    app_ids.push_back(base::SysNSStringToUTF8(app));
  }

  return app_ids;
}

NSDictionary* ReadManagedPreferencePolicyDictionary() {
  base::ScopedCFTypeRef<CFPropertyListRef> policies(CFPreferencesCopyAppValue(
      (__bridge CFStringRef)kManagedPreferencesUpdatePolicies,
      (__bridge CFStringRef)kKeystoneSharedPreferenceSuite));
  if (!policies)
    return nil;

  if (!CFPreferencesAppValueIsForced(
          (__bridge CFStringRef)kManagedPreferencesUpdatePolicies,
          (__bridge CFStringRef)kKeystoneSharedPreferenceSuite)) {
    return nil;
  }

  if (CFGetTypeID(policies) != CFDictionaryGetTypeID())
    return nil;

  return reinterpret_cast<NSDictionary*>(CFBridgingRelease(policies.release()));
}

scoped_refptr<PolicyManagerInterface> CreateManagedPreferencePolicyManager() {
  NSDictionary* policyDict = ReadManagedPreferencePolicyDictionary();
  return base::MakeRefCounted<ManagedPreferencePolicyManager>(policyDict);
}

}  // namespace updater
