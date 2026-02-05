// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_trust_checker.h"

#include "base/test/gmock_expected_support.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/install/non_installed_bundle_inspection_context.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/fake_iwa_runtime_data_provider_mixin.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/web_package/test_support/signed_web_bundles/signing_keys.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/test/browser_test.h"

using testing::_;

namespace web_app {

class IsolatedWebAppTrustCheckerBrowserTest
    : public IsolatedWebAppBrowserTestHarness {
 protected:
  web_app::FakeIwaRuntimeDataProviderMixin data_provider_{&mixin_host_};
};

IN_PROC_BROWSER_TEST_F(IsolatedWebAppTrustCheckerBrowserTest,
                       UserInstallAllowlist) {
  const auto allowlisted_bundle_id =
      web_package::test::GetDefaultEd25519WebBundleId();
  data_provider_->Update([&](auto& update) {
    update.AddToUserInstallAllowlist(
        allowlisted_bundle_id,
        ChromeIwaRuntimeDataProvider::UserInstallAllowlistItemData(
            /*enterprise_name=*/"Google LLC"));
  });

  EXPECT_THAT(
      IsolatedWebAppTrustChecker::IsOperationAllowed(
          *profile(), allowlisted_bundle_id,
          /*dev_mode=*/false,
          IwaInstallOperation{
              .source = webapps::WebappInstallSource::IWA_GRAPHICAL_INSTALLER}),
      base::test::HasValue());
  EXPECT_THAT(
      IsolatedWebAppTrustChecker::IsOperationAllowed(
          *profile(), web_package::test::GetDefaultEcdsaP256WebBundleId(),
          /*dev_mode=*/false,
          IwaInstallOperation{
              .source = webapps::WebappInstallSource::IWA_GRAPHICAL_INSTALLER}),
      base::test::ErrorIs(_));
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppTrustCheckerBrowserTest, Blocklist) {
  const auto allowlisted_bundle_id =
      web_package::test::GetDefaultEd25519WebBundleId();
  data_provider_->Update(
      [&](auto& update) { update.AddToBlocklist(allowlisted_bundle_id); });

  EXPECT_THAT(
      IsolatedWebAppTrustChecker::IsOperationAllowed(
          *profile(), allowlisted_bundle_id,
          /*dev_mode=*/false,
          IwaInstallOperation{
              .source = webapps::WebappInstallSource::IWA_GRAPHICAL_INSTALLER}),
      base::test::ErrorIs(_));
}

}  // namespace web_app
