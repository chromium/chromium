// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_trust_checker.h"

#include <memory>

#include "base/check_deref.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/strings/strcat.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/types/cxx23_to_underlying.h"
#include "build/build_config.h"
#include "chrome/browser/policy/developer_tools_policy_handler.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/web_package/mojom/web_bundle_parser.mojom.h"
#include "components/web_package/signed_web_bundles/ed25519_public_key.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_integrity_block.h"
#include "components/webapps/isolated_web_apps/scheme.h"
#include "content/public/common/content_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/url_constants.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_features.h"
#include "base/values.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_policy_constants.h"
#include "chrome/common/chromeos/extensions/chromeos_system_extension_info.h"  // nogncheck
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace web_app {

namespace {

using ::testing::_;

constexpr std::array<uint8_t, 32> kPublicKeyBytes1 = {
    0x01, 0x23, 0x43, 0x43, 0x33, 0x42, 0x7A, 0x14, 0x42, 0x14, 0xa2,
    0xb6, 0xc2, 0xd9, 0xf2, 0x02, 0x03, 0x42, 0x18, 0x10, 0x12, 0x26,
    0x62, 0x88, 0xf6, 0xa3, 0xa5, 0x47, 0x14, 0x69, 0x00, 0x73};

constexpr std::array<uint8_t, 32> kPublicKeyBytes2 = {
    0x02, 0x23, 0x43, 0x43, 0x33, 0x42, 0x7A, 0x14, 0x42, 0x14, 0xa2,
    0xb6, 0xc2, 0xd9, 0xf2, 0x02, 0x03, 0x42, 0x18, 0x10, 0x12, 0x26,
    0x62, 0x88, 0xf6, 0xa3, 0xa5, 0x47, 0x14, 0x69, 0x00, 0x73};

#if BUILDFLAG(IS_CHROMEOS)
constexpr std::array<uint8_t, 32> kShimless3pDiagnosticsDevPublicKeyBytes = {
    0x7c, 0xf4, 0x9c, 0x48, 0x1f, 0xc5, 0x37, 0xaf, 0x33, 0x42, 0x0d,
    0x3a, 0xc1, 0x13, 0x91, 0x88, 0x13, 0x53, 0x50, 0x06, 0x8b, 0x9b,
    0x19, 0x42, 0xcd, 0xe8, 0xce, 0x10, 0x45, 0x12, 0xf1, 0x00};
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace

class IsolatedWebAppTrustCheckerTest : public WebAppTest {
 public:
  IsolatedWebAppTrustCheckerTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kIsolatedWebApps);
  }

  PrefService& pref_service() { return *profile()->GetPrefs(); }

  const web_package::Ed25519PublicKey kPublicKey1 =
      web_package::Ed25519PublicKey::Create(base::span(kPublicKeyBytes1));
  const web_package::Ed25519PublicKey kPublicKey2 =
      web_package::Ed25519PublicKey::Create(base::span(kPublicKeyBytes2));

  const web_package::SignedWebBundleId kWebBundleId1 =
      web_package::SignedWebBundleId::CreateForPublicKey(kPublicKey1);
  const web_package::SignedWebBundleId kWebBundleId2 =
      web_package::SignedWebBundleId::CreateForPublicKey(kPublicKey2);

  const GURL kStartUrl1 =
      GURL(std::string(webapps::kIsolatedAppScheme) +
           url::kStandardSchemeSeparator + kWebBundleId1.id());
  const GURL kStartUrl2 =
      GURL(std::string(webapps::kIsolatedAppScheme) +
           url::kStandardSchemeSeparator + kWebBundleId2.id());

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(IsolatedWebAppTrustCheckerTest, DevWebBundleId) {
  EXPECT_THAT(IsolatedWebAppTrustChecker::IsTrusted(
                  *profile(),
                  web_package::SignedWebBundleId::CreateRandomForProxyMode(),
                  /*is_dev_mode_bundle=*/false),
              base::test::ErrorIs(_));
}

TEST_F(IsolatedWebAppTrustCheckerTest, UntrustedByDefault) {
  EXPECT_THAT(IsolatedWebAppTrustChecker::IsTrusted(
                  *profile(), kWebBundleId1, /*is_dev_mode_bundle=*/false),
              base::test::ErrorIs(_));

  EXPECT_THAT(IsolatedWebAppTrustChecker::IsTrusted(
                  *profile(), kWebBundleId2, /*is_dev_mode_bundle=*/false),
              base::test::ErrorIs(_));
}

#if BUILDFLAG(IS_CHROMEOS)

TEST_F(IsolatedWebAppTrustCheckerTest, TrustedViaPolicy) {
  base::Value::List force_install_list;
  {
    base::Value::Dict force_install_entry;
    force_install_entry.Set(kPolicyWebBundleIdKey, "not a web bundle id");
    force_install_entry.Set(kPolicyUpdateManifestUrlKey,
                            "https://example.com/update-manifest.json");
    force_install_list.Append(std::move(force_install_entry));
  }
  {
    base::Value::Dict force_install_entry;
    force_install_entry.Set(kPolicyWebBundleIdKey, kWebBundleId1.id());
    force_install_entry.Set(kPolicyUpdateManifestUrlKey,
                            "https://example.com/update-manifest.json");
    force_install_list.Append(std::move(force_install_entry));
  }
  pref_service().SetList(prefs::kIsolatedWebAppInstallForceList,
                         std::move(force_install_list));

  EXPECT_THAT(IsolatedWebAppTrustChecker::IsTrusted(
                  *profile(), kWebBundleId1, /*is_dev_mode_bundle=*/false),
              base::test::HasValue());

  EXPECT_THAT(IsolatedWebAppTrustChecker::IsTrusted(
                  *profile(), kWebBundleId2, /*is_dev_mode_bundle=*/false),
              base::test::ErrorIs(_));
}

#endif  // BUILDFLAG(IS_CHROMEOS)

TEST_F(IsolatedWebAppTrustCheckerTest,
       DevModeDoesNotAutomaticallyTrustAllApps) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kIsolatedWebAppDevMode);

  EXPECT_THAT(IsolatedWebAppTrustChecker::IsTrusted(
                  *profile(), kWebBundleId1, /*is_dev_mode_bundle=*/false),
              base::test::ErrorIs(_));
}

TEST_F(IsolatedWebAppTrustCheckerTest, TrustedViaDevMode) {
  EXPECT_THAT(IsolatedWebAppTrustChecker::IsTrusted(
                  *profile(), kWebBundleId1, /*is_dev_mode_bundle=*/true),
              base::test::ErrorIs(_));

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kIsolatedWebAppDevMode);
  EXPECT_THAT(IsolatedWebAppTrustChecker::IsTrusted(
                  *profile(), kWebBundleId1, /*is_dev_mode_bundle=*/true),
              base::test::HasValue());

  pref_service().SetInteger(
      prefs::kDevToolsAvailability,
      base::to_underlying(
          policy::DeveloperToolsPolicyHandler::Availability::kDisallowed));
  EXPECT_THAT(IsolatedWebAppTrustChecker::IsTrusted(
                  *profile(), kWebBundleId1, /*is_dev_mode_bundle=*/true),
              base::test::ErrorIs(_));
}

TEST_F(IsolatedWebAppTrustCheckerTest, TrustedWebBundleIDsForTesting) {
  SetTrustedWebBundleIdsForTesting({kWebBundleId1});

  EXPECT_THAT(IsolatedWebAppTrustChecker::IsTrusted(
                  *profile(), kWebBundleId1, /*is_dev_mode_bundle=*/false),
              base::test::HasValue());

  EXPECT_THAT(IsolatedWebAppTrustChecker::IsTrusted(
                  *profile(), kWebBundleId2, /*is_dev_mode_bundle=*/false),
              base::test::ErrorIs(_));
}

#if BUILDFLAG(IS_CHROMEOS)

class ShimlessProfileIsolatedWebAppTrustCheckerTest : public ::testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    TestingProfile::Builder profile_builder;
    profile_builder.SetPath(temp_dir_.GetPath().AppendASCII(
        ash::kShimlessRmaAppBrowserContextBaseName));
    shimless_profile_ = profile_builder.Build();
  }

  TestingProfile& shimless_profile() { return *shimless_profile_; }

  const web_package::Ed25519PublicKey k3pDiagnosticsDevPublicKey =
      web_package::Ed25519PublicKey::Create(
          base::span(kShimless3pDiagnosticsDevPublicKeyBytes));
  const web_package::SignedWebBundleId k3pDiagnosticsDevWebBundleId =
      web_package::SignedWebBundleId::CreateForPublicKey(
          k3pDiagnosticsDevPublicKey);

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestingProfile> shimless_profile_;
  base::ScopedTempDir temp_dir_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
};

TEST_F(ShimlessProfileIsolatedWebAppTrustCheckerTest,
       TrustedVia3pDiagnosticsApp) {
  auto scoped_info =
      chromeos::ScopedChromeOSSystemExtensionInfo::CreateForTesting();
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatureStates({});
  scoped_info->ApplyCommandLineSwitchesForTesting();
  // Does not trust the key if the dev key is not allowlisted via feature flag.
  EXPECT_THAT(IsolatedWebAppTrustChecker::IsTrusted(
                  shimless_profile(), k3pDiagnosticsDevWebBundleId,
                  /*is_dev_mode_bundle=*/false),
              base::test::ErrorIs(_));

  feature_list.Reset();
  feature_list.InitWithFeatureStates(
      {{ash::features::kShimlessRMA3pDiagnosticsDevMode, true}});
  scoped_info->ApplyCommandLineSwitchesForTesting();
  EXPECT_THAT(IsolatedWebAppTrustChecker::IsTrusted(
                  shimless_profile(), k3pDiagnosticsDevWebBundleId,
                  /*is_dev_mode_bundle=*/false),
              base::test::HasValue());
}

#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace web_app
