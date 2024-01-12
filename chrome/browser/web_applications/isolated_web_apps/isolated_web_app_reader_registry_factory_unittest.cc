// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_reader_registry_factory.h"

#include "base/test/with_feature_override.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "content/public/common/content_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {

class IsolatedWebAppReaderRegistryFactoryTest
    : public base::test::WithFeatureOverride,
      public WebAppTest {
 public:
  IsolatedWebAppReaderRegistryFactoryTest()
      : base::test::WithFeatureOverride(features::kIsolatedWebApps) {}
};

TEST_P(IsolatedWebAppReaderRegistryFactoryTest, GuardedBehindFeatureFlag) {
  auto* registry =
      IsolatedWebAppReaderRegistryFactory::GetForProfile(profile());
  if (IsParamFeatureEnabled()) {
    ASSERT_TRUE(registry);
  } else {
    ASSERT_FALSE(registry);
  }
}

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(
    IsolatedWebAppReaderRegistryFactoryTest);

}  // namespace web_app
