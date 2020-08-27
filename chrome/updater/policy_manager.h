// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_POLICY_MANAGER_H_
#define CHROME_UPDATER_POLICY_MANAGER_H_

#include <memory>
#include <string>

namespace updater {

// The Policy Manager Interface is implemented by policy managers such as Group
// Policy and Device Management.
class PolicyManagerInterface {
 public:
  virtual ~PolicyManagerInterface() = default;

  // This is human-readable string that indicates the policy manager being
  // queried.
  virtual std::string source() const = 0;

  // This method returns |true| if the current policy manager determines that
  // its policies are operational. For instance, the Device Management Policy
  // Manager will return |true| for this method if the machine is joined to
  // a DM server.
  virtual bool IsManaged() const = 0;

  // Returns the policy for how often the Updater should check for updates.
  // Returns the time interval between update checks in minutes.
  // 0 indicates updates are disabled.
  virtual bool GetLastCheckPeriodMinutes(int* minutes) const = 0;

  // For domain-joined machines, checks the current time against the times that
  // updates are suppressed. Updates are suppressed if the current time falls
  // between the start time and the duration.
  // The duration does not account for daylight savings time. For instance, if
  // the start time is 22:00 hours, and with a duration of 8 hours, the updates
  // will be suppressed for 8 hours regardless of whether daylight savings time
  // changes happen in between.
  virtual bool GetUpdatesSuppressedTimes(int* start_hour,
                                         int* start_min,
                                         int* duration_min) const = 0;
  // Returns the policy for the download preference.
  virtual bool GetDownloadPreferenceGroupPolicy(
      std::string* download_preference) const = 0;

  // Returns the policy for the package cache size limit in megabytes.
  virtual bool GetPackageCacheSizeLimitMBytes(int* cache_size_limit) const = 0;

  // Returns the policy for the package cache expiration in days.
  virtual bool GetPackageCacheExpirationTimeDays(
      int* cache_life_limit) const = 0;

  // Returns kPolicyEnabled if installation of the specified app is allowed.
  // Otherwise, returns kPolicyDisabled.
  virtual bool GetEffectivePolicyForAppInstalls(const std::string& app_id,
                                                int* install_policy) const = 0;
  // Returns kPolicyEnabled if updates of the specified app is allowed.
  // Otherwise, returns one of kPolicyDisabled, kPolicyManualUpdatesOnly, or
  // kPolicyAutomaticUpdatesOnly.
  virtual bool GetEffectivePolicyForAppUpdates(const std::string& app_id,
                                               int* update_policy) const = 0;
  // Returns the target version prefix for the app.
  // Examples:
  // * "" (or not configured): update to latest version available.
  // * "55.": update to any minor version of 55 (e.g. 55.24.34 or 55.60.2).
  // * "55.2.": update to any minor version of 55.2 (e.g. 55.2.34 or 55.2.2).
  // * "55.24.34": update to this specific version only.
  virtual bool GetTargetVersionPrefix(
      const std::string& app_id,
      std::string* target_version_prefix) const = 0;
  // Returns whether the RollbackToTargetVersion policy has been set for the
  // app. If RollbackToTargetVersion is set, the TargetVersionPrefix policy
  // governs the version to rollback clients with higher versions to.
  virtual bool IsRollbackToTargetVersionAllowed(
      const std::string& app_id,
      bool* rollback_allowed) const = 0;
  // Returns a proxy mode such as |auto_detect|.
  virtual bool GetProxyMode(std::string* proxy_mode) const = 0;

  // Returns a proxy PAC URL.
  virtual bool GetProxyPacUrl(std::string* proxy_pac_url) const = 0;

  // Returns a proxy server.
  virtual bool GetProxyServer(std::string* proxy_server) const = 0;

  // Returns a channel, for example {stable|beta|dev}.
  virtual bool GetTargetChannel(const std::string& app_id,
                                std::string* channel) const = 0;
};

std::unique_ptr<PolicyManagerInterface> GetPolicyManager();

}  // namespace updater

#endif  // CHROME_UPDATER_POLICY_MANAGER_H_
