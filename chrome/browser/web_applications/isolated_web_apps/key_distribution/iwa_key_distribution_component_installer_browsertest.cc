// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/iwa_key_distribution_component_installer.h"

#include <array>
#include <string>

#include "base/base64.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "base/version.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/key_distribution/test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/webapps/isolated_web_apps/iwa_key_distribution_histograms.h"
#include "components/webapps/isolated_web_apps/iwa_key_distribution_info_provider.h"
#include "components/webapps/isolated_web_apps/proto/key_distribution.pb.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"

namespace web_app {

namespace {

using base::test::ErrorIs;
using base::test::HasValue;
using testing::_;
using testing::Eq;

constexpr std::array<uint8_t, 4> kExpectedKey = {0x00, 0x00, 0x00, 0x00};
constexpr char kWebBundleId[] =
    "aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaac";

IwaKeyDistribution CreateValidData() {
  IwaKeyDistribution key_distribution;

  IwaKeyRotations key_rotations;
  IwaKeyRotations::KeyRotationInfo kr_info;

  kr_info.set_expected_key(base::Base64Encode(kExpectedKey));
  key_rotations.mutable_key_rotations()->emplace(kWebBundleId,
                                                 std::move(kr_info));
  *key_distribution.mutable_key_rotation_data() = std::move(key_rotations);

  IwaSpecialAppPermissions special_app_permissions;
  IwaSpecialAppPermissions::SpecialAppPermissions special_app_permissions_info;
  special_app_permissions_info.mutable_multi_screen_capture()
      ->set_skip_capture_started_notification(true);
  special_app_permissions.mutable_special_app_permissions()->emplace(
      kWebBundleId, std::move(special_app_permissions_info));

  return key_distribution;
}

}  // namespace

class IwaKeyDistributionComponentInstallBrowserTest
    : public InProcessBrowserTest {
 private:
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  base::test::ScopedFeatureList features_{
      component_updater::kIwaKeyDistributionComponent};
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

// TODO(crbug.com/393102554): Remove this after launch.
#if BUILDFLAG(IS_WIN)
  base::test::ScopedFeatureList features_{features::kIsolatedWebApps};
#endif  // BUILDFLAG(IS_WIN)
};

IN_PROC_BROWSER_TEST_F(
    IwaKeyDistributionComponentInstallBrowserTest,
    CallComponentReadyWhenRegistrationFindsExistingComponent) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  // Override the pre-install component directory and its alternative directory
  // so that the component update will not find the pre-loaded component.
  base::ScopedPathOverride preinstalled_dir_override(
      component_updater::DIR_COMPONENT_PREINSTALLED);
  base::ScopedPathOverride preinstalled_alt_dir_override(
      component_updater::DIR_COMPONENT_PREINSTALLED_ALT);

  EXPECT_THAT(test::InstallIwaKeyDistributionComponent(base::Version("2.0.0"),
                                                       CreateValidData()),
              HasValue());
  EXPECT_THAT(test::InstallIwaKeyDistributionComponent(base::Version("1.0.0"),
                                                       CreateValidData()),
              ErrorIs(Eq(IwaComponentUpdateError::kStaleVersion)));
  EXPECT_THAT(test::InstallIwaKeyDistributionComponent(base::Version("2.1.0"),
                                                       CreateValidData()),
              HasValue());
}

IN_PROC_BROWSER_TEST_F(IwaKeyDistributionComponentInstallBrowserTest,
                       PreloadedComponent) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  // Override the user-wide component directory to make sure there is no
  // downloaded component.
  base::ScopedPathOverride user_dir_override(
      component_updater::DIR_COMPONENT_USER);

  base::HistogramTester ht;

  // Trigger a call to GetKeyRotationInfo() to ensure the correctness of logged
  // UMAs.
  IwaKeyDistributionInfoProvider::GetInstance()->GetKeyRotationInfo("anything");

  EXPECT_THAT(ht.GetAllSamples(kIwaKeyRotationInfoSource),
              base::BucketsAre(base::Bucket(KeyRotationInfoSource::kNone, 1)));

  ASSERT_OK_AND_ASSIGN(
      (auto [version, is_preloaded]),
      test::RegisterIwaKeyDistributionComponentAndWaitForLoad());
  ASSERT_TRUE(is_preloaded);

  // Trigger a call to GetKeyRotationInfo() to ensure the correctness of logged
  // UMAs.
  IwaKeyDistributionInfoProvider::GetInstance()->GetKeyRotationInfo("anything");

  EXPECT_THAT(
      ht.GetAllSamples(kIwaKeyRotationInfoSource),
      base::BucketsAre(base::Bucket(KeyRotationInfoSource::kNone, 1),
                       base::Bucket(KeyRotationInfoSource::kPreloaded, 1)));
}

}  // namespace web_app
