// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/gmock_expected_support.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/runtime_data/chrome_iwa_runtime_data_provider.h"
#include "chrome/browser/web_applications/isolated_web_apps/runtime_data/iwa_entitlements.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/chrome_iwa_runtime_data_provider_mixin.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/fake_chrome_iwa_runtime_data_provider.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features_generated.h"

namespace web_app {

namespace {

using PermissionsPolicyFeature = network::mojom::PermissionsPolicyFeature;
using IwaRuntimeAllowlistData =
    ChromeIwaRuntimeDataProvider::UserInstallAllowlistItemData;
using IwaEntitlementProto = IwaAccessControl::UserInstallAllowlistItemData;

constexpr std::string_view kEntitledFeatures[] = {
    "direct-sockets",   "direct-sockets-private", "direct-sockets-multicast",
    "sub-apps",         "usb-unrestricted",       "web-printing",
    "controlled-frame",
#if defined(ENABLE_SMART_CARD)
    "smart-card",
#endif
};

constexpr PermissionsPolicyFeature kEntitledPermissions[] = {
    PermissionsPolicyFeature::kDirectSockets,
    PermissionsPolicyFeature::kDirectSocketsPrivate,
    PermissionsPolicyFeature::kMulticastInDirectSockets,
    PermissionsPolicyFeature::kSubApps,
    PermissionsPolicyFeature::kUsbUnrestricted,
    PermissionsPolicyFeature::kWebPrinting,
    PermissionsPolicyFeature::kControlledFrame,
#if defined(ENABLE_SMART_CARD)
    PermissionsPolicyFeature::kSmartCard,
#endif
};

constexpr IwaEntitlement kEntitlements[] = {
    IwaEntitlementProto::DIRECT_SOCKETS,
    IwaEntitlementProto::DIRECT_SOCKETS_PRIVATE,
    IwaEntitlementProto::DIRECT_SOCKETS_MULTICAST,
    IwaEntitlementProto::SUB_APPS,
    IwaEntitlementProto::UNRESTRICTED_WEBUSB,
    IwaEntitlementProto::WEB_PRINTING,
    IwaEntitlementProto::CONTROLLED_FRAME,
#if defined(ENABLE_SMART_CARD)
    IwaEntitlementProto::SMART_CARD,
#endif
};

}  // namespace

class IsolatedWebAppEntitlementsBrowserTest
    : public IsolatedWebAppBrowserTestHarness {
 public:
  IsolatedWebAppEntitlementsBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {
            blink::features::kSubApps,
            blink::features::kWebPrinting,
#if defined(ENABLE_SMART_CARD)
            blink::features::kSmartCard,
#endif
        },
        {});
  }

  bool IsFeatureAllowed(content::RenderFrameHost* const frame,
                        std::string_view feature) {
    return content::EvalJs(frame, "document.featurePolicy.allowsFeature('" +
                                      std::string(feature) + "')")
        .ExtractBool();
  }

 protected:
  TypedIwaRuntimeDataProviderMixin<FakeIwaRuntimeDataProvider>
      iwa_runtime_data_mixin_{&mixin_host_};

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(IsolatedWebAppEntitlementsBrowserTest,
                       UserInstalled_Enforced_NoEntitlement) {
  ManifestBuilder manifest_builder;
  for (auto permission : kEntitledPermissions) {
    manifest_builder.AddPermissionsPolicyWildcard(permission);
  }

  std::unique_ptr<ScopedBundledIsolatedWebApp> app =
      IsolatedWebAppBuilder(manifest_builder).BuildBundle();

  iwa_runtime_data_mixin_->Update([&](auto& update) {
    update.AddToUserInstallAllowlist(
        app->web_bundle_id(), IwaRuntimeAllowlistData("Test Enterprise", {}));
  });

  const auto url_info =
      app->InstallChecked(profile(), BundledIsolatedWebApp::DoNotTrustKey{});
  content::RenderFrameHost* const frame = OpenApp(url_info.app_id());
  ASSERT_TRUE(frame);

  for (auto feature : kEntitledFeatures) {
    EXPECT_FALSE(IsFeatureAllowed(frame, feature)) << feature;
  }
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppEntitlementsBrowserTest,
                       UserInstalled_Enforced_WithEntitlement) {
  ManifestBuilder manifest_builder;
  for (auto permission : kEntitledPermissions) {
    manifest_builder.AddPermissionsPolicyWildcard(permission);
  }

  std::unique_ptr<ScopedBundledIsolatedWebApp> app =
      IsolatedWebAppBuilder(manifest_builder).BuildBundle();

  iwa_runtime_data_mixin_->Update([&](auto& update) {
    IwaEntitlementsSet set;
    set.entitlements.assign(std::begin(kEntitlements), std::end(kEntitlements));
    update.AddToUserInstallAllowlist(
        app->web_bundle_id(),
        IwaRuntimeAllowlistData("Test Enterprise", {set}));
  });

  const auto url_info =
      app->InstallChecked(profile(), BundledIsolatedWebApp::DoNotTrustKey{});
  content::RenderFrameHost* const frame = OpenApp(url_info.app_id());
  ASSERT_TRUE(frame);

  for (auto feature : kEntitledFeatures) {
    EXPECT_TRUE(IsFeatureAllowed(frame, feature)) << feature;
  }
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppEntitlementsBrowserTest,
                       PolicyInstalled_NotEnforced) {
  std::unique_ptr<ScopedBundledIsolatedWebApp> app =
      IsolatedWebAppBuilder(ManifestBuilder().AddPermissionsPolicyWildcard(
                                PermissionsPolicyFeature::kDirectSockets))
          .BuildBundle();
  app->TrustSigningKey();

  const auto url_info =
      app->InstallWithSource(profile(),
                             &IsolatedWebAppInstallSource::FromExternalPolicy)
          .value();

  content::RenderFrameHost* const frame = OpenApp(url_info.app_id());
  ASSERT_TRUE(frame);
  EXPECT_TRUE(IsFeatureAllowed(frame, "direct-sockets"));
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppEntitlementsBrowserTest,
                       DevMode_NotEnforced) {
  std::unique_ptr<ScopedBundledIsolatedWebApp> app =
      IsolatedWebAppBuilder(ManifestBuilder().AddPermissionsPolicyWildcard(
                                PermissionsPolicyFeature::kDirectSockets))
          .BuildBundle();

  const auto url_info =
      app->InstallWithSource(profile(),
                             &IsolatedWebAppInstallSource::FromDevCommandLine)
          .value();

  content::RenderFrameHost* const frame = OpenApp(url_info.app_id());
  ASSERT_TRUE(frame);
  EXPECT_TRUE(IsFeatureAllowed(frame, "direct-sockets"));
}

IN_PROC_BROWSER_TEST_F(
    IsolatedWebAppEntitlementsBrowserTest,
    UserInstalled_IsolatedContextFeatureWithoutEntitlement_Disallowed) {
  std::unique_ptr<ScopedBundledIsolatedWebApp> app =
      IsolatedWebAppBuilder(ManifestBuilder().AddPermissionsPolicyWildcard(
                                PermissionsPolicyFeature::kAllScreensCapture))
          .BuildBundle();

  iwa_runtime_data_mixin_->Update([&](auto& update) {
    update.AddToUserInstallAllowlist(
        app->web_bundle_id(), IwaRuntimeAllowlistData("Test Enterprise", {}));
  });

  const auto url_info =
      app->InstallChecked(profile(), BundledIsolatedWebApp::DoNotTrustKey{});
  content::RenderFrameHost* const frame = OpenApp(url_info.app_id());
  ASSERT_TRUE(frame);

  EXPECT_FALSE(IsFeatureAllowed(frame, "all-screens-capture"));
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppEntitlementsBrowserTest,
                       UserInstalled_OpenWebFeatures_Allowed) {
  std::unique_ptr<ScopedBundledIsolatedWebApp> app =
      IsolatedWebAppBuilder(
          ManifestBuilder()
              .AddPermissionsPolicyWildcard(PermissionsPolicyFeature::kCamera)
              .AddPermissionsPolicyWildcard(
                  PermissionsPolicyFeature::kFullscreen))
          .BuildBundle();

  iwa_runtime_data_mixin_->Update([&](auto& update) {
    update.AddToUserInstallAllowlist(
        app->web_bundle_id(), IwaRuntimeAllowlistData("Test Enterprise", {}));
  });

  const auto url_info =
      app->InstallChecked(profile(), BundledIsolatedWebApp::DoNotTrustKey{});
  content::RenderFrameHost* const frame = OpenApp(url_info.app_id());
  ASSERT_TRUE(frame);

  EXPECT_TRUE(IsFeatureAllowed(frame, "camera"));
  EXPECT_TRUE(IsFeatureAllowed(frame, "fullscreen"));
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppEntitlementsBrowserTest,
                       UserInstalled_VersionInRange_Allowed) {
  std::unique_ptr<ScopedBundledIsolatedWebApp> app =
      IsolatedWebAppBuilder(
          ManifestBuilder().SetVersion("2.0.0").AddPermissionsPolicyWildcard(
              PermissionsPolicyFeature::kDirectSockets))
          .BuildBundle();

  iwa_runtime_data_mixin_->Update([&](auto& update) {
    IwaEntitlementsSet set;
    set.entitlements.push_back(IwaEntitlementProto::DIRECT_SOCKETS);
    set.version_range.set_begin("1.0.0");
    set.version_range.set_end("3.0.0");
    update.AddToUserInstallAllowlist(
        app->web_bundle_id(),
        IwaRuntimeAllowlistData("Test Enterprise", {set}));
  });

  const auto url_info =
      app->InstallChecked(profile(), BundledIsolatedWebApp::DoNotTrustKey{});
  content::RenderFrameHost* const frame = OpenApp(url_info.app_id());
  ASSERT_TRUE(frame);

  EXPECT_TRUE(IsFeatureAllowed(frame, "direct-sockets"));
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppEntitlementsBrowserTest,
                       UserInstalled_VersionBelowRange_Disallowed) {
  std::unique_ptr<ScopedBundledIsolatedWebApp> app =
      IsolatedWebAppBuilder(
          ManifestBuilder().SetVersion("1.0.0").AddPermissionsPolicyWildcard(
              PermissionsPolicyFeature::kDirectSockets))
          .BuildBundle();

  iwa_runtime_data_mixin_->Update([&](auto& update) {
    IwaEntitlementsSet set;
    set.entitlements.push_back(IwaEntitlementProto::DIRECT_SOCKETS);
    set.version_range.set_begin("2.0.0");
    update.AddToUserInstallAllowlist(
        app->web_bundle_id(),
        IwaRuntimeAllowlistData("Test Enterprise", {set}));
  });

  const auto url_info =
      app->InstallChecked(profile(), BundledIsolatedWebApp::DoNotTrustKey{});
  content::RenderFrameHost* const frame = OpenApp(url_info.app_id());
  ASSERT_TRUE(frame);

  EXPECT_FALSE(IsFeatureAllowed(frame, "direct-sockets"));
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppEntitlementsBrowserTest,
                       UserInstalled_VersionAboveRange_Disallowed) {
  std::unique_ptr<ScopedBundledIsolatedWebApp> app =
      IsolatedWebAppBuilder(
          ManifestBuilder().SetVersion("3.0.0").AddPermissionsPolicyWildcard(
              PermissionsPolicyFeature::kDirectSockets))
          .BuildBundle();

  iwa_runtime_data_mixin_->Update([&](auto& update) {
    IwaEntitlementsSet set;
    set.entitlements.push_back(IwaEntitlementProto::DIRECT_SOCKETS);
    set.version_range.set_end("3.0.0");
    update.AddToUserInstallAllowlist(
        app->web_bundle_id(),
        IwaRuntimeAllowlistData("Test Enterprise", {set}));
  });

  const auto url_info =
      app->InstallChecked(profile(), BundledIsolatedWebApp::DoNotTrustKey{});
  content::RenderFrameHost* const frame = OpenApp(url_info.app_id());
  ASSERT_TRUE(frame);

  EXPECT_FALSE(IsFeatureAllowed(frame, "direct-sockets"));
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppEntitlementsBrowserTest,
                       UserInstalled_VersionExactMatchBegin_Allowed) {
  std::unique_ptr<ScopedBundledIsolatedWebApp> app =
      IsolatedWebAppBuilder(
          ManifestBuilder().SetVersion("2.0.0").AddPermissionsPolicyWildcard(
              PermissionsPolicyFeature::kDirectSockets))
          .BuildBundle();

  iwa_runtime_data_mixin_->Update([&](auto& update) {
    IwaEntitlementsSet set;
    set.entitlements.push_back(IwaEntitlementProto::DIRECT_SOCKETS);
    set.version_range.set_begin("2.0.0");
    set.version_range.set_end("3.0.0");
    update.AddToUserInstallAllowlist(
        app->web_bundle_id(),
        IwaRuntimeAllowlistData("Test Enterprise", {set}));
  });

  const auto url_info =
      app->InstallChecked(profile(), BundledIsolatedWebApp::DoNotTrustKey{});
  content::RenderFrameHost* const frame = OpenApp(url_info.app_id());
  ASSERT_TRUE(frame);

  EXPECT_TRUE(IsFeatureAllowed(frame, "direct-sockets"));
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppEntitlementsBrowserTest,
                       UserInstalled_OpenBeginRange_Allowed) {
  std::unique_ptr<ScopedBundledIsolatedWebApp> app =
      IsolatedWebAppBuilder(
          ManifestBuilder().SetVersion("1.0.0").AddPermissionsPolicyWildcard(
              PermissionsPolicyFeature::kDirectSockets))
          .BuildBundle();

  iwa_runtime_data_mixin_->Update([&](auto& update) {
    IwaEntitlementsSet set;
    set.entitlements.push_back(IwaEntitlementProto::DIRECT_SOCKETS);
    set.version_range.set_end("2.0.0");
    update.AddToUserInstallAllowlist(
        app->web_bundle_id(),
        IwaRuntimeAllowlistData("Test Enterprise", {set}));
  });

  const auto url_info =
      app->InstallChecked(profile(), BundledIsolatedWebApp::DoNotTrustKey{});
  content::RenderFrameHost* const frame = OpenApp(url_info.app_id());
  ASSERT_TRUE(frame);

  EXPECT_TRUE(IsFeatureAllowed(frame, "direct-sockets"));
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppEntitlementsBrowserTest,
                       UserInstalled_OpenEndRange_Allowed) {
  std::unique_ptr<ScopedBundledIsolatedWebApp> app =
      IsolatedWebAppBuilder(
          ManifestBuilder().SetVersion("3.0.0").AddPermissionsPolicyWildcard(
              PermissionsPolicyFeature::kDirectSockets))
          .BuildBundle();

  iwa_runtime_data_mixin_->Update([&](auto& update) {
    IwaEntitlementsSet set;
    set.entitlements.push_back(IwaEntitlementProto::DIRECT_SOCKETS);
    set.version_range.set_begin("2.0.0");
    update.AddToUserInstallAllowlist(
        app->web_bundle_id(),
        IwaRuntimeAllowlistData("Test Enterprise", {set}));
  });

  const auto url_info =
      app->InstallChecked(profile(), BundledIsolatedWebApp::DoNotTrustKey{});
  content::RenderFrameHost* const frame = OpenApp(url_info.app_id());
  ASSERT_TRUE(frame);

  EXPECT_TRUE(IsFeatureAllowed(frame, "direct-sockets"));
}

IN_PROC_BROWSER_TEST_F(
    IsolatedWebAppEntitlementsBrowserTest,
    UserInstalled_MultipleEntitlementSets_OneMatches_Allowed) {
  std::unique_ptr<ScopedBundledIsolatedWebApp> app =
      IsolatedWebAppBuilder(
          ManifestBuilder().SetVersion("3.0.0").AddPermissionsPolicyWildcard(
              PermissionsPolicyFeature::kDirectSockets))
          .BuildBundle();

  iwa_runtime_data_mixin_->Update([&](auto& update) {
    IwaEntitlementsSet set1;
    set1.entitlements.push_back(IwaEntitlementProto::DIRECT_SOCKETS);
    set1.version_range.set_end("2.0.0");

    IwaEntitlementsSet set2;
    set2.entitlements.push_back(IwaEntitlementProto::DIRECT_SOCKETS);
    set2.version_range.set_begin("3.0.0");

    update.AddToUserInstallAllowlist(
        app->web_bundle_id(),
        IwaRuntimeAllowlistData("Test Enterprise", {set1, set2}));
  });

  const auto url_info =
      app->InstallChecked(profile(), BundledIsolatedWebApp::DoNotTrustKey{});
  content::RenderFrameHost* const frame = OpenApp(url_info.app_id());
  ASSERT_TRUE(frame);

  EXPECT_TRUE(IsFeatureAllowed(frame, "direct-sockets"));
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppEntitlementsBrowserTest,
                       UserInstalled_LogsViolationToConsole) {
  ManifestBuilder manifest_builder;
  manifest_builder.AddPermissionsPolicyWildcard(
      PermissionsPolicyFeature::kDirectSockets);
  manifest_builder.AddPermissionsPolicyWildcard(
      PermissionsPolicyFeature::kCamera);

  std::unique_ptr<ScopedBundledIsolatedWebApp> app =
      IsolatedWebAppBuilder(manifest_builder).BuildBundle();

  iwa_runtime_data_mixin_->Update([&](auto& update) {
    update.AddToUserInstallAllowlist(
        app->web_bundle_id(), IwaRuntimeAllowlistData("Test Enterprise", {}));
  });

  const auto url_info =
      app->InstallChecked(profile(), BundledIsolatedWebApp::DoNotTrustKey{});

  content::RenderFrameHost* frame = OpenApp(url_info.app_id());
  ASSERT_TRUE(frame);

  content::WebContentsConsoleObserver console_observer(
      content::WebContents::FromRenderFrameHost(frame));
  console_observer.SetPattern(
      "IWA entitlement violation: feature 'direct-sockets' is not granted "
      "to " +
      url_info.origin().GetURL().spec() + ".");

  EXPECT_TRUE(content::ExecJs(frame, "location.reload();"));

  ASSERT_TRUE(console_observer.Wait());
  EXPECT_EQ(1u, console_observer.messages().size());
}

}  // namespace web_app
