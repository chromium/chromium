// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_TEST_POLICY_GENERATOR_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_TEST_POLICY_GENERATOR_H_

#include <optional>

#include "base/values.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/webapps/isolated_web_apps/types/iwa_version.h"
#include "components/webapps/isolated_web_apps/types/update_channel.h"
#include "url/gurl.h"
namespace web_app {

// This class simplifies generation of the IsolatedWebAppInstallForceList
// policy.
class PolicyGenerator {
 public:
  PolicyGenerator();
  ~PolicyGenerator();

  void AddForceInstalledIwa(
      const web_package::SignedWebBundleId& web_bundle_id,
      const GURL& update_manifest_url,
      const std::optional<UpdateChannel>& update_channel = std::nullopt,
      const std::optional<IwaVersion>& pinned_version = std::nullopt,
      bool allow_downgrades = false);

  base::Value Generate();

 private:
  base::Value::List app_policies_;
};
}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_TEST_POLICY_GENERATOR_H_
