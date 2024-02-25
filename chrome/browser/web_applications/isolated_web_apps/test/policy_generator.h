// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_TEST_POLICY_GENERATOR_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_TEST_POLICY_GENERATOR_H_

#include "base/values.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "url/gurl.h"
namespace web_app {

// This class simplifies generation of the IsolatedWebAppInstallForceList
// policy.
class PolicyGenerator {
 public:
  PolicyGenerator();
  ~PolicyGenerator();

  void AddForceInstalledIwa(web_package::SignedWebBundleId id,
                            GURL update_manifest_url);
  base::Value Generate();

 private:
  struct IwaForceInstalledPolicy {
    web_package::SignedWebBundleId id_;
    GURL update_manifest_url_;
  };

  std::vector<IwaForceInstalledPolicy> app_policies_;
};
}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_TEST_POLICY_GENERATOR_H_
