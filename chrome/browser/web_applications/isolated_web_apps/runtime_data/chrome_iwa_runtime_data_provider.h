// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_CHROME_IWA_RUNTIME_DATA_PROVIDER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_CHROME_IWA_RUNTIME_DATA_PROVIDER_H_

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/types/pass_key.h"
#include "base/values.h"
#include "components/webapps/isolated_web_apps/public/iwa_runtime_data_provider.h"

class BrowserProcessImpl;
class TestingBrowserProcess;

namespace web_app {

// Augments the IwaRuntimeDataProvider with chrome-specific data and policies.
// Consumers in //chrome should use this interface to access IWA runtime data.
class ChromeIwaRuntimeDataProvider : public IwaRuntimeDataProvider {
 public:
  struct SpecialAppPermissionsInfo {
    base::Value AsDebugValue() const;
    bool skip_capture_started_notification;
  };

  struct UserInstallAllowlistItemData {
    explicit UserInstallAllowlistItemData(const std::string& enterprise_name);
    ~UserInstallAllowlistItemData();
    UserInstallAllowlistItemData(const UserInstallAllowlistItemData&);

    base::Value AsDebugValue() const;

    std::string enterprise_name;
  };

  static ChromeIwaRuntimeDataProvider& GetInstance();

  // Note that these methods do not take ownership of `instance`; the lifetime
  // management remains the caller's responsibility.
  static void SetInstance(
      base::PassKey<BrowserProcessImpl, TestingBrowserProcess>,
      ChromeIwaRuntimeDataProvider* instance);
  static base::AutoReset<ChromeIwaRuntimeDataProvider*> SetInstanceForTesting(
      ChromeIwaRuntimeDataProvider* instance);

  // Only bundles present in the managed allowlist can be installed and updated.
  virtual bool IsManagedInstallPermitted(
      std::string_view web_bundle_id) const = 0;
  virtual bool IsManagedUpdatePermitted(
      std::string_view web_bundle_id) const = 0;
  virtual bool IsBundleBlocklisted(std::string_view web_bundle_id) const = 0;

  virtual const SpecialAppPermissionsInfo* GetSpecialAppPermissionsInfo(
      const std::string& web_bundle_id) const = 0;
  virtual std::vector<std::string> GetSkipMultiCaptureNotificationBundleIds()
      const = 0;

  virtual const UserInstallAllowlistItemData* GetUserInstallAllowlistData(
      const std::string& web_bundle_id) const = 0;

  virtual void WriteDebugMetadata(base::DictValue& log) const = 0;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_CHROME_IWA_RUNTIME_DATA_PROVIDER_H_
