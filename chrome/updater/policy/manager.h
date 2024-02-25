// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_POLICY_MANAGER_H_
#define CHROME_UPDATER_POLICY_MANAGER_H_

#include <optional>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "chrome/updater/constants.h"

namespace updater {

// Updates are suppressed if the current time falls between the start time and
// the duration. The duration does not account for daylight savings time.
// For instance, if the start time is 22:00 hours, and with a duration of 8
// hours, the updates will be suppressed for 8 hours regardless of whether
// daylight savings time changes happen in between.
class UpdatesSuppressedTimes {
 public:
  bool operator==(const UpdatesSuppressedTimes& other) const;
  bool operator!=(const UpdatesSuppressedTimes& other) const;

  bool valid() const;

  // Returns true if and only if the `hour`:`minute` wall clock time falls
  // within this suppression period.
  bool contains(int hour, int minute) const;

  int start_hour_ = kPolicyNotSet;
  int start_minute_ = kPolicyNotSet;
  int duration_minute_ = kPolicyNotSet;
};

// The Policy Manager Interface is implemented by policy managers such as Group
// Policy and Device Management.
class PolicyManagerInterface
    : public base::RefCountedThreadSafe<PolicyManagerInterface> {
 public:
  // This is human-readable string that indicates the policy manager being
  // queried.
  virtual std::string source() const = 0;

  // This method returns |true| if the current policy manager determines that
  // its policies are operational. For instance, the Device Management Policy
  // Manager will return |true| for this method if the machine is joined to
  // a DM server.
  virtual bool HasActiveDevicePolicies() const = 0;

  // Whether the cloud policy overrides the platform policy.
  // This policy affects Windows only.
  // The policy value from device management provider takes precedence if this
  // policy has conflict values.
  virtual std::optional<bool> CloudPolicyOverridesPlatformPolicy() const = 0;

  // Returns the policy for how often the Updater should check for updates.
  // Returns the time interval between update checks.
  // 0 indicates updates are disabled.
  virtual std::optional<base::TimeDelta> GetLastCheckPeriod() const = 0;

  // For domain-joined machines, checks the current time against the times that
  // updates are suppressed.
  virtual std::optional<UpdatesSuppressedTimes> GetUpdatesSuppressedTimes()
      const = 0;

  // Returns the policy for the download preference.
  virtual std::optional<std::string> GetDownloadPreference() const = 0;

  // Returns the policy for the package cache size limit in megabytes.
  virtual std::optional<int> GetPackageCacheSizeLimitMBytes() const = 0;

  // Returns the policy for the package cache expiration in days.
  virtual std::optional<int> GetPackageCacheExpirationTimeDays() const = 0;

  // Returns kPolicyEnabled if installation of the specified app is allowed.
  // Otherwise, returns kPolicyDisabled.
  virtual std::optional<int> GetEffectivePolicyForAppInstalls(
      const std::string& app_id) const = 0;
  // Returns kPolicyEnabled if updates of the specified app is allowed.
  // Otherwise, returns one of kPolicyDisabled, kPolicyManualUpdatesOnly, or
  // kPolicyAutomaticUpdatesOnly.
  virtual std::optional<int> GetEffectivePolicyForAppUpdates(
      const std::string& app_id) const = 0;
  // Returns the target version prefix for the app.
  // Examples:
  // * "" (or not configured): update to latest version available.
  // * "55.": update to any minor version of 55 (e.g. 55.24.34 or 55.60.2).
  // * "55.2.": update to any minor version of 55.2 (e.g. 55.2.34 or 55.2.2).
  // * "55.24.34": update to this specific version only.
  virtual std::optional<std::string> GetTargetVersionPrefix(
      const std::string& app_id) const = 0;
  // Returns whether the RollbackToTargetVersion policy has been set for the
  // app. If RollbackToTargetVersion is set, the TargetVersionPrefix policy
  // governs the version to rollback clients with higher versions to.
  virtual std::optional<bool> IsRollbackToTargetVersionAllowed(
      const std::string& app_id) const = 0;
  // Returns a proxy mode such as |auto_detect|.
  virtual std::optional<std::string> GetProxyMode() const = 0;

  // Returns a proxy PAC URL.
  virtual std::optional<std::string> GetProxyPacUrl() const = 0;

  // Returns a proxy server.
  virtual std::optional<std::string> GetProxyServer() const = 0;

  // Returns a channel, for example {stable|beta|dev}.
  virtual std::optional<std::string> GetTargetChannel(
      const std::string& app_id) const = 0;

  // Returns a list of apps that need to be downloaded and installed by the
  // updater.
  virtual std::optional<std::vector<std::string>> GetForceInstallApps()
      const = 0;

  // Returns all apps that have some policy set.
  virtual std::optional<std::vector<std::string>> GetAppsWithPolicy() const = 0;

 protected:
  friend class base::RefCountedThreadSafe<PolicyManagerInterface>;
  virtual ~PolicyManagerInterface() = default;
};

scoped_refptr<PolicyManagerInterface> GetDefaultValuesPolicyManager();

}  // namespace updater

#endif  // CHROME_UPDATER_POLICY_MANAGER_H_
