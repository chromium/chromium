// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/policy_service.h"

#include "base/check.h"
#include "build/build_config.h"

#if defined(OS_WIN)
#include "chrome/updater/win/group_policy_manager.h"
#elif defined(OS_MAC)
#include "chrome/updater/mac/managed_preference_policy_manager.h"
#endif

namespace updater {

// Only policy manager that are enterprise managed are used by the policy
// service.
PolicyService::PolicyService() : default_policy_manager_(GetPolicyManager()) {
#if defined(OS_WIN)
  auto group_policy_manager = std::make_unique<GroupPolicyManager>();
  if (group_policy_manager->IsManaged())
    policy_managers_.emplace_back(std::move(group_policy_manager));
#endif
    // TODO (crbug/1122118): Inject the DMPolicyManager here.
#if defined(OS_MAC)
  auto mac_policy_manager = CreateManagedPreferencePolicyManager();
  if (mac_policy_manager->IsManaged())
    policy_managers_.emplace_back(std::move(mac_policy_manager));
#endif
  UpdateActivePolicyManager();
}

PolicyService::~PolicyService() = default;

void PolicyService::SetPolicyManagersForTesting(
    std::vector<std::unique_ptr<PolicyManagerInterface>> managers) {
  policy_managers_ = std::move(managers);
  UpdateActivePolicyManager();
}

std::string PolicyService::source() const {
  return active_policy_manager_->source();
}

bool PolicyService::IsManaged() const {
  return active_policy_manager_->IsManaged();
}

bool PolicyService::GetLastCheckPeriodMinutes(int* minutes) const {
  return active_policy_manager_->GetLastCheckPeriodMinutes(minutes) ||
         (ShouldFallbackToDefaultManager() &&
          default_policy_manager_->GetLastCheckPeriodMinutes(minutes));
}

bool PolicyService::GetUpdatesSuppressedTimes(int* start_hour,
                                              int* start_min,
                                              int* duration_min) const {
  return active_policy_manager_->GetUpdatesSuppressedTimes(
             start_hour, start_min, duration_min) ||
         (ShouldFallbackToDefaultManager() &&
          default_policy_manager_->GetUpdatesSuppressedTimes(
              start_hour, start_min, duration_min));
}

bool PolicyService::GetDownloadPreferenceGroupPolicy(
    std::string* download_preference) const {
  return active_policy_manager_->GetDownloadPreferenceGroupPolicy(
             download_preference) ||
         (ShouldFallbackToDefaultManager() &&
          default_policy_manager_->GetDownloadPreferenceGroupPolicy(
              download_preference));
}

bool PolicyService::GetPackageCacheSizeLimitMBytes(
    int* cache_size_limit) const {
  return active_policy_manager_->GetPackageCacheSizeLimitMBytes(
             cache_size_limit) ||
         (ShouldFallbackToDefaultManager() &&
          default_policy_manager_->GetPackageCacheSizeLimitMBytes(
              cache_size_limit));
}

bool PolicyService::GetPackageCacheExpirationTimeDays(
    int* cache_life_limit) const {
  return active_policy_manager_->GetPackageCacheExpirationTimeDays(
             cache_life_limit) ||
         (ShouldFallbackToDefaultManager() &&
          default_policy_manager_->GetPackageCacheExpirationTimeDays(
              cache_life_limit));
}

bool PolicyService::GetEffectivePolicyForAppInstalls(
    const std::string& app_id,
    int* install_policy) const {
  return active_policy_manager_->GetEffectivePolicyForAppInstalls(
             app_id, install_policy) ||
         (ShouldFallbackToDefaultManager() &&
          default_policy_manager_->GetEffectivePolicyForAppInstalls(
              app_id, install_policy));
}

bool PolicyService::GetEffectivePolicyForAppUpdates(const std::string& app_id,
                                                    int* update_policy) const {
  return active_policy_manager_->GetEffectivePolicyForAppUpdates(
             app_id, update_policy) ||
         (ShouldFallbackToDefaultManager() &&
          default_policy_manager_->GetEffectivePolicyForAppUpdates(
              app_id, update_policy));
}

bool PolicyService::GetTargetChannel(const std::string& app_id,
                                     std::string* channel) const {
  return active_policy_manager_->GetTargetChannel(app_id, channel) ||
         (ShouldFallbackToDefaultManager() &&
          default_policy_manager_->GetTargetChannel(app_id, channel));
}

bool PolicyService::GetTargetVersionPrefix(
    const std::string& app_id,
    std::string* target_version_prefix) const {
  return active_policy_manager_->GetTargetVersionPrefix(
             app_id, target_version_prefix) ||
         (ShouldFallbackToDefaultManager() &&
          default_policy_manager_->GetTargetVersionPrefix(
              app_id, target_version_prefix));
}

bool PolicyService::IsRollbackToTargetVersionAllowed(
    const std::string& app_id,
    bool* rollback_allowed) const {
  return active_policy_manager_->IsRollbackToTargetVersionAllowed(
             app_id, rollback_allowed) ||
         (ShouldFallbackToDefaultManager() &&
          default_policy_manager_->IsRollbackToTargetVersionAllowed(
              app_id, rollback_allowed));
}

bool PolicyService::GetProxyMode(std::string* proxy_mode) const {
  return active_policy_manager_->GetProxyMode(proxy_mode) ||
         (ShouldFallbackToDefaultManager() &&
          default_policy_manager_->GetProxyMode(proxy_mode));
}

bool PolicyService::GetProxyPacUrl(std::string* proxy_pac_url) const {
  return active_policy_manager_->GetProxyPacUrl(proxy_pac_url) ||
         (ShouldFallbackToDefaultManager() &&
          default_policy_manager_->GetProxyPacUrl(proxy_pac_url));
}

bool PolicyService::GetProxyServer(std::string* proxy_server) const {
  return active_policy_manager_->GetProxyServer(proxy_server) ||
         (ShouldFallbackToDefaultManager() &&
          default_policy_manager_->GetProxyServer(proxy_server));
}

const PolicyManagerInterface& PolicyService::GetActivePolicyManager() {
  DCHECK(active_policy_manager_);
  return *active_policy_manager_;
}

bool PolicyService::ShouldFallbackToDefaultManager() const {
  return active_policy_manager_ != default_policy_manager_.get();
}

void PolicyService::UpdateActivePolicyManager() {
  // The active policy manager is either the default policy manager or the
  // manager with the highest level that is managed.
  active_policy_manager_ = default_policy_manager_.get();
  for (const auto& manager : policy_managers_) {
    if (manager->IsManaged()) {
      active_policy_manager_ = manager.get();
      return;
    }
  }
}

std::unique_ptr<PolicyService> GetUpdaterPolicyService() {
  return std::make_unique<PolicyService>();
}

}  // namespace updater
