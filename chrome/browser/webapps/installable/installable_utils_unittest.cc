// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webapps/installable/installable_utils.h"

#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
// This only tests the desktop implementation.
// TODO(crbug.com/354971473): Add unit tests for the android implementation.
class InstallableUtilsTest : public WebAppTest {
 public:
  void SetUp() override {
    WebAppTest::SetUp();
    web_app::test::AwaitStartWebAppProviderAndSubsystems(profile());
  }
};

TEST_F(InstallableUtilsTest, DoesOriginContainAnyInstalledWebApp) {
  web_app::test::InstallDummyWebApp(profile(), "abc",
                                    GURL("https://www.example.com/app"));

  EXPECT_TRUE(DoesOriginContainAnyInstalledWebApp(
      profile(), GURL("https://www.example.com")));
  EXPECT_FALSE(DoesOriginContainAnyInstalledWebApp(
      profile(), GURL("https://www.example2.com")));
}

TEST_F(InstallableUtilsTest, GetOriginsWithInstalledWebApps) {
  web_app::test::InstallDummyWebApp(profile(), "abc",
                                    GURL("https://www.example.com/app"));
  web_app::test::InstallDummyWebApp(profile(), "abc",
                                    GURL("https://www.example2.com/app"));
  EXPECT_THAT(GetOriginsWithInstalledWebApps(profile()),
              testing::UnorderedElementsAre(GURL("https://www.example.com/"),
                                            GURL("https://www.example2.com")));
}

}  // namespace
