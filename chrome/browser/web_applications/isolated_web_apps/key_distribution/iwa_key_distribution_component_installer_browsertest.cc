// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/iwa_key_distribution_component_installer.h"

#include <array>
#include <string>

#include "base/base64.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "base/version.h"
#include "chrome/browser/web_applications/isolated_web_apps/key_distribution/iwa_key_distribution_info_provider.h"
#include "chrome/browser/web_applications/isolated_web_apps/key_distribution/proto/key_distribution.pb.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/key_distribution/test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
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

  return key_distribution;
}

}  // namespace

class IwaKeyDistributionComponentInstallBrowserTest
    : public InProcessBrowserTest {
 private:
  base::test::ScopedFeatureList features_{
      component_updater::kIwaKeyDistributionComponent};
};

IN_PROC_BROWSER_TEST_F(
    IwaKeyDistributionComponentInstallBrowserTest,
    CallComponentReadyWhenRegistrationFindsExistingComponent) {
  EXPECT_THAT(test::InstallIwaKeyDistributionComponent(base::Version("2.0.0"),
                                                       CreateValidData()),
              HasValue());
  EXPECT_THAT(test::InstallIwaKeyDistributionComponent(base::Version("1.0.0"),
                                                       CreateValidData()),
              ErrorIs(Eq(IwaKeyDistributionInfoProvider::ComponentUpdateError::
                             kStaleVersion)));
  EXPECT_THAT(test::InstallIwaKeyDistributionComponent(base::Version("2.1.0"),
                                                       CreateValidData()),
              HasValue());
}

}  // namespace web_app
