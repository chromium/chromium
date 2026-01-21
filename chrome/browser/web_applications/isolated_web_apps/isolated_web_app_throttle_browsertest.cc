// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_throttle.h"

#include "base/check_deref.h"
#include "base/files/file_path.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/test_future.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/iwa_permissions_policy_cache.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "components/webapps/isolated_web_apps/types/iwa_origin.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace web_app {

namespace {

using testing::IsEmpty;
using testing::IsNull;
using testing::NotNull;
using testing::UnorderedElementsAre;

MATCHER_P2(IsolatedAppPermissionPolicyEntryIs, feature, allowed_origins, "") {
  return arg.feature == feature && arg.allowed_origins == allowed_origins;
}

}  // namespace

class IsolatedWebAppThrottleBrowserTest
    : public IsolatedWebAppBrowserTestHarness {
 protected:
  IwaPermissionsPolicyCache* GetCache() {
    return IwaPermissionsPolicyCacheFactory::GetForProfile(profile());
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }
};

IN_PROC_BROWSER_TEST_F(IsolatedWebAppThrottleBrowserTest,
                       ManifestWithPermissionsPolicy) {
  ManifestBuilder manifest_builder =
      ManifestBuilder()
          .AddPermissionsPolicy(
              network::mojom::PermissionsPolicyFeature::kCamera, /*self=*/true,
              /*origins=*/{url::Origin::Create(GURL("https://example.com"))})
          .AddPermissionsPolicyWildcard(
              network::mojom::PermissionsPolicyFeature::kDirectSockets)
          .AddPermissionsPolicy(
              network::mojom::PermissionsPolicyFeature::kMicrophone,
              /*self=*/false, /*origins=*/{})
          .AddPermissionsPolicy(
              network::mojom::PermissionsPolicyFeature::kGeolocation,
              /*self=*/true, /*origins=*/
              {url::Origin::Create(GURL("https://foo.com")),
               url::Origin::Create(GURL("https://bar.com"))})
          .AddPermissionsPolicyWildcard(
              network::mojom::PermissionsPolicyFeature::kFullscreen)
          .AddPermissionsPolicy(
              network::mojom::PermissionsPolicyFeature::kHid,
              /*self=*/false, /*origins=*/
              {url::Origin::Create(GURL("https://baz.com")),
               url::Origin::Create(GURL("https://google.com"))});
  std::unique_ptr<ScopedBundledIsolatedWebApp> app =
      IsolatedWebAppBuilder(manifest_builder).BuildBundle();
  ASSERT_OK_AND_ASSIGN(IsolatedWebAppUrlInfo url_info, app->Install(profile()));

  IwaOrigin iwa_origin = IwaOrigin::Create(url_info.origin().GetURL()).value();
  EXPECT_THAT(GetCache()->GetPolicy(iwa_origin), IsNull());

  // Navigate to the app.
  content::RenderFrameHost* frame = OpenApp(url_info.app_id());
  ASSERT_THAT(frame, NotNull());

  // Check that the policy is cached.
  using CacheEntry = IwaPermissionsPolicyCache::CacheEntry;
  const CacheEntry* policy = GetCache()->GetPolicy(iwa_origin);
  ASSERT_THAT(policy, NotNull());

  EXPECT_THAT(
      *policy,
      UnorderedElementsAre(
          IsolatedAppPermissionPolicyEntryIs(
              "camera",
              std::vector<std::string>({"'self'", "https://example.com"})),
          IsolatedAppPermissionPolicyEntryIs(
              "cross-origin-isolated", std::vector<std::string>({"'self'"})),
          IsolatedAppPermissionPolicyEntryIs("direct-sockets",
                                             std::vector<std::string>({"*"})),
          IsolatedAppPermissionPolicyEntryIs(
              "microphone", std::vector<std::string>({"'none'"})),
          IsolatedAppPermissionPolicyEntryIs(
              "geolocation",
              std::vector<std::string>(
                  {"'self'", "https://foo.com", "https://bar.com"})),
          IsolatedAppPermissionPolicyEntryIs("fullscreen",
                                             std::vector<std::string>({"*"})),
          IsolatedAppPermissionPolicyEntryIs(
              "hid", std::vector<std::string>(
                         {"https://baz.com", "https://google.com"}))));
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppThrottleBrowserTest,
                       ManifestWithoutPermissionsPolicy) {
  ManifestBuilder manifest_builder = ManifestBuilder(false);
  std::unique_ptr<ScopedBundledIsolatedWebApp> app =
      IsolatedWebAppBuilder(manifest_builder).BuildBundle();
  ASSERT_OK_AND_ASSIGN(IsolatedWebAppUrlInfo url_info, app->Install(profile()));

  IwaOrigin iwa_origin = IwaOrigin::Create(url_info.origin().GetURL()).value();
  EXPECT_THAT(GetCache()->GetPolicy(iwa_origin), IsNull());

  content::RenderFrameHost* frame = OpenApp(url_info.app_id());
  ASSERT_THAT(frame, NotNull());

  const IwaPermissionsPolicyCache::CacheEntry* policy =
      GetCache()->GetPolicy(iwa_origin);
  ASSERT_THAT(policy, NotNull());
  EXPECT_THAT(*policy, IsEmpty());
}

}  // namespace web_app
