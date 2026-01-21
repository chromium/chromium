// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/gmock_expected_support.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "components/webapps/isolated_web_apps/test_support/signing_keys.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {

namespace {

using testing::ElementsAre;
using testing::IsEmpty;
using testing::UnorderedElementsAre;

}  // namespace

class IsolatedWebAppGenericPermissionsPolicyBrowserTest
    : public IsolatedWebAppBrowserTestHarness {
 public:
  bool IsFeatureAllowed(content::RenderFrameHost* const frame,
                        std::string_view feature) {
    return content::EvalJs(frame, "document.featurePolicy.allowsFeature('" +
                                      std::string(feature) + "')")
        .ExtractBool();
  }

  std::vector<std::string> GetAllowlist(content::RenderFrameHost* const frame,
                                        std::string_view feature) {
    std::vector<std::string> allowlist;
    const auto get_allowlist_result = content::EvalJs(
        frame, "document.featurePolicy.getAllowlistForFeature('" +
                   std::string(feature) + "')");
    for (const auto& item : get_allowlist_result.ExtractList()) {
      allowlist.push_back(item.GetString());
    }
    return allowlist;
  }

  std::vector<std::string> GetAllowedFeatures(
      content::RenderFrameHost* const frame) {
    std::vector<std::string> allowlist;
    const auto get_allowed_features =
        content::EvalJs(frame, "document.featurePolicy.allowedFeatures()");
    for (const auto& item : get_allowed_features.ExtractList()) {
      allowlist.push_back(item.GetString());
    }
    return allowlist;
  }
};

IN_PROC_BROWSER_TEST_F(IsolatedWebAppGenericPermissionsPolicyBrowserTest,
                       ManifestPolicyIsApplied) {
  const ManifestBuilder manifest_builder =
      ManifestBuilder()
          .AddPermissionsPolicy(
              network::mojom::PermissionsPolicyFeature::kCamera, /*self=*/true,
              /*origins=*/{})
          .AddPermissionsPolicy(
              network::mojom::PermissionsPolicyFeature::kMicrophone,
              /*self=*/false, /*origins=*/{});

  const std::unique_ptr<ScopedBundledIsolatedWebApp> app =
      IsolatedWebAppBuilder(manifest_builder).BuildBundle();
  app->TrustSigningKey();
  const auto url_info = app->InstallChecked(profile());

  content::RenderFrameHost* const frame = OpenApp(url_info.app_id());
  ASSERT_TRUE(frame);

  EXPECT_TRUE(IsFeatureAllowed(frame, "camera"));
  EXPECT_THAT(GetAllowlist(frame, "camera"),
              ElementsAre(url_info.origin().Serialize()));

  EXPECT_FALSE(IsFeatureAllowed(frame, "microphone"));
  EXPECT_THAT(GetAllowlist(frame, "microphone"), IsEmpty());

  EXPECT_FALSE(IsFeatureAllowed(frame, "geolocation"));
  EXPECT_THAT(GetAllowlist(frame, "geolocation"), IsEmpty());
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppGenericPermissionsPolicyBrowserTest,
                       HeadersAlsoApply) {
  const ManifestBuilder manifest_builder =
      ManifestBuilder()
          .AddPermissionsPolicy(
              network::mojom::PermissionsPolicyFeature::kCamera, /*self=*/true,
              /*origins=*/{})
          .AddPermissionsPolicy(
              network::mojom::PermissionsPolicyFeature::kMicrophone,
              /*self=*/true, /*origins=*/{})
          .AddPermissionsPolicy(
              network::mojom::PermissionsPolicyFeature::kGeolocation,
              /*self=*/true, /*origins=*/{});

  IsolatedWebAppBuilder builder(manifest_builder);
  builder.AddResource("/", "<html><body></body></html>",
                      {{"Permissions-Policy", "camera=(), microphone=*"},
                       {"Content-Type", "text/html"}});

  const std::unique_ptr<ScopedBundledIsolatedWebApp> app =
      builder.BuildBundle();
  app->TrustSigningKey();
  const auto url_info = app->InstallChecked(profile());

  content::RenderFrameHost* const frame = OpenApp(url_info.app_id());
  ASSERT_TRUE(frame);
  const std::string origin = url_info.origin().Serialize();

  // Camera: Manifest 'self', Header 'none'. Result: 'none'.
  EXPECT_FALSE(IsFeatureAllowed(frame, "camera"));
  EXPECT_THAT(GetAllowlist(frame, "camera"), IsEmpty());

  // Microphone: Manifest 'self', Header *. Result: 'self'.
  EXPECT_TRUE(IsFeatureAllowed(frame, "microphone"));
  EXPECT_THAT(GetAllowlist(frame, "microphone"), ElementsAre(origin));

  // Geolocation: Manifest 'self', Header (missing -> *). Result: 'self'.
  EXPECT_TRUE(IsFeatureAllowed(frame, "geolocation"));
  EXPECT_THAT(GetAllowlist(frame, "geolocation"), ElementsAre(origin));
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppGenericPermissionsPolicyBrowserTest,
                       EmptyManifest) {
  const ManifestBuilder manifest_builder = ManifestBuilder(
      /*include_cross_origin_isolated_permissions_policy=*/false);

  IsolatedWebAppBuilder builder(manifest_builder);
  builder.AddResource("/", "<html><body></body></html>",
                      {{"Permissions-Policy", "camera=(), microphone=*"},
                       {"Content-Type", "text/html"}});

  const std::unique_ptr<ScopedBundledIsolatedWebApp> app =
      builder.BuildBundle();
  app->TrustSigningKey();
  const auto url_info = app->InstallChecked(profile());

  content::RenderFrameHost* const frame = OpenApp(url_info.app_id());
  ASSERT_TRUE(frame);
  const std::string origin = url_info.origin().Serialize();

  EXPECT_THAT(GetAllowedFeatures(frame), IsEmpty());

  // Camera: Manifest empty, Header 'none'. Result: 'none'.
  EXPECT_FALSE(IsFeatureAllowed(frame, "camera"));
  EXPECT_THAT(GetAllowlist(frame, "camera"), IsEmpty());

  // Microphone: Manifest empty, Header *. Result: 'none'.
  EXPECT_FALSE(IsFeatureAllowed(frame, "microphone"));
  EXPECT_THAT(GetAllowlist(frame, "microphone"), IsEmpty());

  // Geolocation: Manifest empty, Header (missing -> *). Result: 'none'.
  EXPECT_FALSE(IsFeatureAllowed(frame, "geolocation"));
  EXPECT_THAT(GetAllowlist(frame, "geolocation"), IsEmpty());
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppGenericPermissionsPolicyBrowserTest,
                       ManifestSelfHeaderSameOriginIntersection) {
  const ManifestBuilder manifest_builder =
      ManifestBuilder().AddPermissionsPolicy(
          network::mojom::PermissionsPolicyFeature::kCamera, /*self=*/true,
          /*origins=*/{});

  IsolatedWebAppBuilder builder(manifest_builder);
  // Using 'self' in header ensures intersection works when both manifest and
  // header specify 'self'.
  builder.AddResource(
      "/", "<html><body></body></html>",
      {{"Permissions-Policy", "camera=(self)"}, {"Content-Type", "text/html"}});

  const std::unique_ptr<ScopedBundledIsolatedWebApp> app =
      builder.BuildBundle();
  app->TrustSigningKey();
  const auto url_info = app->InstallChecked(profile());

  content::RenderFrameHost* const frame = OpenApp(url_info.app_id());
  ASSERT_TRUE(frame);
  const std::string origin = url_info.origin().Serialize();

  EXPECT_TRUE(IsFeatureAllowed(frame, "camera"));
  EXPECT_THAT(GetAllowlist(frame, "camera"), ElementsAre(origin));
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppGenericPermissionsPolicyBrowserTest,
                       CornerCases) {
  const ManifestBuilder manifest_builder =
      ManifestBuilder(
          /*include_cross_origin_isolated_permissions_policy=*/false)
          // Manifest: self, Header: not specified -> Result: 'self'
          .AddPermissionsPolicy(
              network::mojom::PermissionsPolicyFeature::kCrossOriginIsolated,
              /*self=*/true,
              /*origins=*/{})
          // Manifest: *, Header: 'self' -> Result: 'self'
          .AddPermissionsPolicyWildcard(
              network::mojom::PermissionsPolicyFeature::kCamera)
          // Manifest: *, Header: <origin> -> Result: <origin>
          .AddPermissionsPolicyWildcard(
              network::mojom::PermissionsPolicyFeature::kMicrophone)
          // Manifest: <origin>, Header: * -> Result: none (due to inheritance)
          .AddPermissionsPolicy(
              network::mojom::PermissionsPolicyFeature::kGeolocation,
              /*self=*/false,
              /*origins=*/{url::Origin::Create(GURL("https://meow.meow"))})
          // Manifest: <origin>, Header: 'self' (mismatch) -> Result: none
          .AddPermissionsPolicy(
              network::mojom::PermissionsPolicyFeature::kFullscreen,
              /*self=*/false,
              /*origins=*/{url::Origin::Create(GURL("https://meow.meow"))})
          // Manifest: 'self', Header: 2 origins (mismatch) -> Result: none
          .AddPermissionsPolicy(
              network::mojom::PermissionsPolicyFeature::kClipboardWrite,
              /*self=*/true,
              /*origins=*/{})
          // Manifest: 'self', Header: 'self' -> Result: 'self'
          .AddPermissionsPolicy(network::mojom::PermissionsPolicyFeature::
                                    kMulticastInDirectSockets,
                                /*self=*/true,
                                /*origins=*/{})
          // Manifest: 'none', Header: * -> Result: 'none'
          .AddPermissionsPolicy(
              network::mojom::PermissionsPolicyFeature::kGyroscope,
              /*self=*/false, /*origins=*/{})
          // Manifest: 'self' and origins, Header: 'self' and subset -> Result:
          // like header
          .AddPermissionsPolicy(
              network::mojom::PermissionsPolicyFeature::kClipboardRead,
              /*self=*/true, /*origins=*/
              {url::Origin::Create(GURL("https://raven.nevermore")),
               url::Origin::Create(GURL("https://cthulhu.rlyeh")),
               url::Origin::Create(GURL("https://meow.meow")),
               url::Origin::Create(GURL("https://www.bad.horse"))});

  const auto origin_str = IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
                              web_app::test::GetDefaultEd25519WebBundleId())
                              .origin()
                              .Serialize();

  IsolatedWebAppBuilder builder(manifest_builder);
  builder.AddResource(
      "/", "<html><body></body></html>",
      {{"Permissions-Policy",
        "camera=(self), microphone=(\"" + origin_str +
            "\"), geolocation=*, fullscreen=(self), screen-wake-lock=(self), "
            "clipboard-read=(self \"https://raven.nevermore\" "
            "\"https://www.bad.horse\"), "
            "clipboard-write=(\"https://raven.nevermore\" "
            "\"https://www.bad.horse\"), "
            "gyroscope=*"},
       {"Content-Type", "text/html"}});

  const std::unique_ptr<ScopedBundledIsolatedWebApp> app =
      builder.BuildBundle(web_package::test::GetDefaultEd25519KeyPair());
  app->TrustSigningKey();
  const auto url_info = app->InstallChecked(profile());

  content::RenderFrameHost* const frame = OpenApp(url_info.app_id());
  ASSERT_TRUE(frame);
  EXPECT_THAT(
      GetAllowedFeatures(frame),
      UnorderedElementsAre("camera", "cross-origin-isolated", "microphone",
                           "direct-sockets-multicast", "clipboard-read"));

  // Cross Origin Isolated: Manifest 'self', Header: not specified. Result:
  // 'self'.
  EXPECT_TRUE(IsFeatureAllowed(frame, "cross-origin-isolated"));
  EXPECT_THAT(GetAllowlist(frame, "cross-origin-isolated"),
              ElementsAre(origin_str));

  // Camera: Manifest *, Header 'self'. Result: 'self'.
  EXPECT_TRUE(IsFeatureAllowed(frame, "camera"));
  EXPECT_THAT(GetAllowlist(frame, "camera"), ElementsAre(origin_str));

  // Microphone: Manifest *, Header <origin>. Result: <origin>.
  EXPECT_TRUE(IsFeatureAllowed(frame, "microphone"));
  EXPECT_THAT(GetAllowlist(frame, "microphone"), ElementsAre(origin_str));

  // Geolocation: Manifest <origin>, Header *. Result would be <origin> but is
  // empty due to inheritance (self not covered by the manifest).
  EXPECT_FALSE(IsFeatureAllowed(frame, "geolocation"));
  EXPECT_THAT(GetAllowlist(frame, "geolocation"), IsEmpty());

  // Fullscreen: Manifest <origin>, Header 'self' (mismatch). Result: none.
  EXPECT_FALSE(IsFeatureAllowed(frame, "fullscreen"));
  EXPECT_THAT(GetAllowlist(frame, "fullscreen"), IsEmpty());

  // Clipboard Write: Manifest 'self', Header: 2 origins (mismatch). Result:
  // none.
  EXPECT_FALSE(IsFeatureAllowed(frame, "clipboard-write"));
  EXPECT_THAT(GetAllowlist(frame, "clipboard-write"), IsEmpty());

  // ScreenWakeLock: Manifest 'self', Header 'self'. Result: 'self'.
  EXPECT_TRUE(IsFeatureAllowed(frame, "direct-sockets-multicast"));
  EXPECT_THAT(GetAllowlist(frame, "direct-sockets-multicast"),
              ElementsAre(origin_str));

  // Gyroscope: Manifest 'none', Header *. Result: 'none'.
  EXPECT_FALSE(IsFeatureAllowed(frame, "gyroscope"));
  EXPECT_THAT(GetAllowlist(frame, "gyroscope"), IsEmpty());

  // Clipboard Read: Manifest 'self' + origins, Header: 'self' + subset.
  EXPECT_TRUE(IsFeatureAllowed(frame, "clipboard-read"));
  EXPECT_THAT(GetAllowlist(frame, "clipboard-read"),
              UnorderedElementsAre(origin_str, "https://www.bad.horse",
                                   "https://raven.nevermore"));
}

}  // namespace web_app
