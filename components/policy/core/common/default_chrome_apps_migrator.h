// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_DEFAULT_CHROME_APPS_MIGRATOR_H_
#define COMPONENTS_POLICY_CORE_COMMON_DEFAULT_CHROME_APPS_MIGRATOR_H_

#include <map>
#include <string>

#include "base/values.h"
#include "components/policy/core/common/policy_map.h"

namespace policy {

// This class is used as a temporary solution to handle force install policies
// for deprecated Chrome apps. It replaces ExtensionInstallForcelist policy
// for Chrome app with WebAppInstallForceList policy for the corresponding Web
// App. To preserve the pinning state, PinnedLauncherApps policy for Chrome app
// is replaced with the one for Web App.
// This code will be removed when the following steps are done:
// 1. Build discoverability for default apps in Admin panel (Dpanel).
// 2. Build new control logic for blocking installation (but not blocking use
// of) PWAs.
// 3. Migrate policy from Chrome Apps to PWAs in Admin surface (DMServer).
class POLICY_EXPORT DefaultChromeAppsMigrator {
 public:
  DefaultChromeAppsMigrator();
  explicit DefaultChromeAppsMigrator(
      std::map<std::string, std::string> chrome_app_to_web_app);

  DefaultChromeAppsMigrator(const DefaultChromeAppsMigrator&) = delete;
  DefaultChromeAppsMigrator& operator=(const DefaultChromeAppsMigrator&) =
      delete;

  DefaultChromeAppsMigrator(DefaultChromeAppsMigrator&&) noexcept;
  DefaultChromeAppsMigrator& operator=(DefaultChromeAppsMigrator&&) noexcept;

  ~DefaultChromeAppsMigrator();

  // Replaces ExtensionInstallForcelist policy for Chrome Apps listed in
  // `chrome_app_to_web_app_`.
  void Migrate(PolicyMap* policies) const;

 private:
  // Removes chrome Apps listed in `chrome_app_to_web_app_` from
  // ExtensionInstallForcelist policy. Returns ids of removed apps.
  std::vector<std::string> RemoveChromeAppsFromExtensionForcelist(
      PolicyMap* policies) const;

  // Checks that policy value type is list. If not, adds error message to the
  // policy entry and overrides policy value with an empty list.
  void EnsurePolicyValueIsList(PolicyMap* policies,
                               const std::string& policy_name) const;

  // Replaces policy to pin Chrome App from `chrome_app_to_web_app_` with policy
  // to pin corresponding Web App. It only changes PinnedLauncherApps policy,
  // which specifies pinned apps on Chrome OS.
  void MigratePinningPolicy(PolicyMap* policies) const;

  // Maps from ids of Chrome Apps that need to be replaced to Web App urls.
  std::map<std::string, std::string> chrome_app_to_web_app_;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_DEFAULT_CHROME_APPS_MIGRATOR_H_
