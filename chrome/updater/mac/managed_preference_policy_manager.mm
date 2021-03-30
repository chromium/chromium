// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/mac/managed_preference_policy_manager.h"

#include <string>

#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/updater/mac/managed_preference_policy_manager_impl.h"
#include "chrome/updater/policy_manager.h"

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
  ~ManagedPreferencePolicyManager() override;

  // Overrides for PolicyManagerInterface.
  std::string source() const override;

  bool IsManaged() const override;

  bool GetLastCheckPeriodMinutes(int* minutes) const override;
  bool GetUpdatesSuppressedTimes(
      UpdatesSuppressedTimes* suppressed_times) const override;
  bool GetDownloadPreferenceGroupPolicy(
      std::string* download_preference) const override;
  bool GetPackageCacheSizeLimitMBytes(int* cache_size_limit) const override;
  bool GetPackageCacheExpirationTimeDays(int* cache_life_limit) const override;

  bool GetEffectivePolicyForAppInstalls(const std::string& app_id,
                                        int* install_policy) const override;
  bool GetEffectivePolicyForAppUpdates(const std::string& app_id,
                                       int* update_policy) const override;
  bool GetTargetVersionPrefix(
      const std::string& app_id,
      std::string* target_version_prefix) const override;
  bool IsRollbackToTargetVersionAllowed(const std::string& app_id,
                                        bool* rollback_allowed) const override;
  bool GetProxyMode(std::string* proxy_mode) const override;
  bool GetProxyPacUrl(std::string* proxy_pac_url) const override;
  bool GetProxyServer(std::string* proxy_server) const override;
  bool GetTargetChannel(const std::string& app_id,
                        std::string* channel) const override;

 private:
  base::scoped_nsobject<CRUManagedPreferencePolicyManager> impl_;
};

ManagedPreferencePolicyManager::ManagedPreferencePolicyManager(
    CRUUpdatePolicyDictionary* policyDict)
    : impl_([[CRUManagedPreferencePolicyManager alloc]
          initWithDictionary:policyDict]) {}

ManagedPreferencePolicyManager::~ManagedPreferencePolicyManager() = default;

bool ManagedPreferencePolicyManager::IsManaged() const {
  return [impl_ managed];
}

std::string ManagedPreferencePolicyManager::source() const {
  return base::SysNSStringToUTF8([impl_ source]);
}

bool ManagedPreferencePolicyManager::GetLastCheckPeriodMinutes(
    int* minutes) const {
  *minutes = [impl_ lastCheckPeriodMinutes];
  return (*minutes != kPolicyNotSet);
}

bool ManagedPreferencePolicyManager::GetUpdatesSuppressedTimes(
    UpdatesSuppressedTimes* suppressed_times) const {
  *suppressed_times = [impl_ updatesSuppressed];
  return suppressed_times->valid();
}

bool ManagedPreferencePolicyManager::GetDownloadPreferenceGroupPolicy(
    std::string* download_preference) const {
  NSString* value = [impl_ downloadPreference];
  if (value) {
    *download_preference = base::SysNSStringToUTF8(value);
    return true;
  }

  return false;
}

bool ManagedPreferencePolicyManager::GetPackageCacheSizeLimitMBytes(
    int* cache_size_limit) const {
  return false;  // Not supported on Mac.
}

bool ManagedPreferencePolicyManager::GetPackageCacheExpirationTimeDays(
    int* cache_life_limit) const {
  return false;  // Not supported on Mac.
}

bool ManagedPreferencePolicyManager::GetEffectivePolicyForAppInstalls(
    const std::string& app_id,
    int* install_policy) const {
  return false;  // Not supported on Mac.
}

bool ManagedPreferencePolicyManager::GetEffectivePolicyForAppUpdates(
    const std::string& app_id,
    int* update_policy) const {
  // Check app-specific settings first.
  *update_policy = [impl_ appUpdatePolicy:base::SysUTF8ToNSString(app_id)];
  if (*update_policy != kPolicyNotSet)
    return true;

  // Then fallback to global-level policy if needed.
  *update_policy = [impl_ defaultUpdatePolicy];
  return (*update_policy != kPolicyNotSet);
}

bool ManagedPreferencePolicyManager::GetTargetVersionPrefix(
    const std::string& app_id,
    std::string* target_version_prefix) const {
  NSString* value = [impl_ targetVersionPrefix:base::SysUTF8ToNSString(app_id)];
  if (value) {
    *target_version_prefix = base::SysNSStringToUTF8(value);
    return true;
  }

  return false;
}

bool ManagedPreferencePolicyManager::IsRollbackToTargetVersionAllowed(
    const std::string& app_id,
    bool* rollback_allowed) const {
  int rollback_policy =
      [impl_ rollbackToTargetVersion:base::SysUTF8ToNSString(app_id)];
  if (rollback_policy != kPolicyNotSet) {
    *rollback_allowed = (rollback_policy != 0);
    return true;
  }

  return false;
}

bool ManagedPreferencePolicyManager::GetProxyMode(
    std::string* proxy_mode) const {
  NSString* value = [impl_ proxyMode];
  if (value) {
    *proxy_mode = base::SysNSStringToUTF8(value);
    return true;
  }

  return false;
}

bool ManagedPreferencePolicyManager::GetProxyPacUrl(
    std::string* proxy_pac_url) const {
  NSString* value = [impl_ proxyPacURL];
  if (value) {
    *proxy_pac_url = base::SysNSStringToUTF8(value);
    return true;
  }

  return false;
}

bool ManagedPreferencePolicyManager::GetProxyServer(
    std::string* proxy_server) const {
  NSString* value = [impl_ proxyServer];
  if (value) {
    *proxy_server = base::SysNSStringToUTF8(value);
    return true;
  }

  return false;
}

bool ManagedPreferencePolicyManager::GetTargetChannel(
    const std::string& app_id,
    std::string* channel) const {
  NSString* value = [impl_ targetChannel:base::SysUTF8ToNSString(app_id)];
  if (value) {
    *channel = base::SysNSStringToUTF8(value);
    return true;
  }

  return false;
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

std::unique_ptr<PolicyManagerInterface> CreateManagedPreferencePolicyManager() {
  NSDictionary* policyDict = ReadManagedPreferencePolicyDictionary();
  return std::make_unique<ManagedPreferencePolicyManager>(policyDict);
}

}  // namespace updater
