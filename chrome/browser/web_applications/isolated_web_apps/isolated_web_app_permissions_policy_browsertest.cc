// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/scoped_temp_file.h"
#include "base/strings/string_util.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/commands/install_isolated_web_app_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/install/isolated_web_app_install_source.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/web_package/test_support/signed_web_bundles/web_bundle_signer.h"
#include "components/web_package/web_bundle_builder.h"
#include "components/webapps/isolated_web_apps/test_support/signing_keys.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {

namespace {

using testing::ElementsAre;
using testing::HasSubstr;
using testing::IsEmpty;
using testing::UnorderedElementsAre;

struct InvalidPermissionsPolicyTestCase {
  std::string test_name;
  std::string permissions_policy;
};

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

class IsolatedWebAppInvalidPermissionsPolicyBrowserTest
    : public IsolatedWebAppBrowserTestHarness,
      public testing::WithParamInterface<InvalidPermissionsPolicyTestCase> {};

IN_PROC_BROWSER_TEST_P(IsolatedWebAppInvalidPermissionsPolicyBrowserTest,
                       InstallationFails) {
  web_package::WebBundleBuilder builder;
  builder.AddExchange("/", {{":status", "200"}, {"content-type", "text/html"}},
                      "<html></html>");
  builder.AddExchange(
      "/.well-known/manifest.webmanifest",
      {{":status", "200"}, {"content-type", "application/manifest+json"}},
      base::ReplaceStringPlaceholders(
          R"({
        "name": "Invalid Policy App",
        "version": "1.0.0",
        "id": "/",
        "scope": "/",
        "start_url": "/",
        "permissions_policy": $1
      })",
          {GetParam().permissions_policy}, nullptr));

  auto key_pair = web_package::test::Ed25519KeyPair::CreateRandom();
  auto bundle = web_package::test::WebBundleSigner::SignBundle(
      builder.CreateBundle(),
      std::vector<web_package::test::KeyPair>{key_pair});

  base::test::TestFuture<base::expected<InstallIsolatedWebAppCommandSuccess,
                                        InstallIsolatedWebAppCommandError>>
      future;

  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::ScopedTempFile bundle_file;
    ASSERT_TRUE(bundle_file.Create());
    ASSERT_TRUE(base::WriteFile(bundle_file.path(), bundle));

    auto url_info = IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
        web_package::SignedWebBundleId::CreateForPublicKey(
            key_pair.public_key));

    provider().scheduler().InstallIsolatedWebApp(
        url_info,
        IsolatedWebAppInstallSource::FromDevUi(IwaSourceBundleDevModeWithFileOp(
            bundle_file.path(), IwaSourceBundleDevFileOp::kCopy)),
        /*expected_version=*/std::nullopt,
        /*optional_keep_alive=*/nullptr,
        /*optional_profile_keep_alive=*/nullptr, future.GetCallback());

    auto result = future.Get();
    ASSERT_FALSE(result.has_value());
    EXPECT_THAT(
        result.error().message,
        HasSubstr("App is not installable: The manifest could not be fetched, "
                  "parsed, or the document is on an opaque origin"));
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    IsolatedWebAppInvalidPermissionsPolicyBrowserTest,
    testing::Values(
        InvalidPermissionsPolicyTestCase{
            .test_name = "NotAnObject",
            .permissions_policy = R"("not-an-object")"},
        InvalidPermissionsPolicyTestCase{.test_name = "AnArray",
                                         .permissions_policy = R"([])"},
        InvalidPermissionsPolicyTestCase{
            .test_name = "FeatureNotAnArray",
            .permissions_policy = R"({"camera": "not-an-array"})"},
        InvalidPermissionsPolicyTestCase{
            .test_name = "FeatureAnObject",
            .permissions_policy = R"({"camera": {}})"},
        InvalidPermissionsPolicyTestCase{
            .test_name = "AllowlistContainsNonString",
            .permissions_policy =
                R"({"camera": ["https://example.com", 123]})"},
        InvalidPermissionsPolicyTestCase{
            .test_name = "AllowlistContainsAnObject",
            .permissions_policy = R"({"camera": [{}]})"}),
    [](const testing::TestParamInfo<InvalidPermissionsPolicyTestCase>& info) {
      return info.param.test_name;
    });

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
