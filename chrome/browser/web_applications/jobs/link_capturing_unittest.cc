// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/jobs/link_capturing.h"

#include "base/test/test_future.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/webapps/common/web_app_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace web_app {
namespace {

class LinkCapturingJobTest : public WebAppTest {
 public:
  const GURL kTestAppUrl = GURL("https://example.com/index.html");
  const GURL kTestOverlappingAppUrl = GURL("https://example.com/index2.html");
  const GURL kTestAppCapturablePage = GURL("https://example.com/page.html");

  LinkCapturingJobTest() = default;
  ~LinkCapturingJobTest() override = default;

  void SetUp() override {
    WebAppTest::SetUp();
    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  WebAppProvider* provider() { return WebAppProvider::GetForTest(profile()); }
};

TEST_F(LinkCapturingJobTest, SingleAppEnabled) {
  webapps::AppId app_id = test::InstallWebApp(
      profile(), WebAppInstallInfo::CreateWithStartUrlForTesting(kTestAppUrl));

  EXPECT_TRUE(provider()->registrar_unsafe().IsLinkCapturableByApp(
      app_id, kTestAppCapturablePage));
  EXPECT_FALSE(provider()->registrar_unsafe().CapturesLinksInScope(app_id));
  EXPECT_EQ(absl::nullopt,
            provider()->registrar_unsafe().FindAppThatCapturesLinksInScope(
                kTestAppCapturablePage));

  base::test::TestFuture<void> preference_set;
  provider()->scheduler().SetAppCapturesSupportedLinksDisableOverlapping(
      app_id, true, preference_set.GetCallback());
  ASSERT_TRUE(preference_set.Wait());

  EXPECT_TRUE(provider()->registrar_unsafe().IsLinkCapturableByApp(
      app_id, kTestAppCapturablePage));
  EXPECT_TRUE(provider()->registrar_unsafe().CapturesLinksInScope(app_id));
  EXPECT_EQ(app_id,
            provider()->registrar_unsafe().FindAppThatCapturesLinksInScope(
                kTestAppCapturablePage));
}

TEST_F(LinkCapturingJobTest, DisablesOtherApps) {
  webapps::AppId app1_id = test::InstallWebApp(
      profile(), WebAppInstallInfo::CreateWithStartUrlForTesting(kTestAppUrl));
  webapps::AppId app2_id = test::InstallWebApp(
      profile(),
      WebAppInstallInfo::CreateWithStartUrlForTesting(kTestOverlappingAppUrl));

  {
    base::test::TestFuture<void> preference_set;
    provider()->scheduler().SetAppCapturesSupportedLinksDisableOverlapping(
        app1_id, true, preference_set.GetCallback());
    ASSERT_TRUE(preference_set.Wait());
  }

  EXPECT_TRUE(provider()->registrar_unsafe().CapturesLinksInScope(app1_id));

  // This should disable link capturing on the first app, and enable it on the
  // second app.
  {
    base::test::TestFuture<void> preference_set;
    provider()->scheduler().SetAppCapturesSupportedLinksDisableOverlapping(
        app2_id, true, preference_set.GetCallback());
    ASSERT_TRUE(preference_set.Wait());
  }

  // The first app should have this disabled.
  EXPECT_FALSE(provider()->registrar_unsafe().CapturesLinksInScope(app1_id));

  // It should be set up on the second app.
  EXPECT_TRUE(provider()->registrar_unsafe().CapturesLinksInScope(app2_id));
  EXPECT_EQ(app2_id,
            provider()->registrar_unsafe().FindAppThatCapturesLinksInScope(
                kTestAppCapturablePage));
}

}  // namespace
}  // namespace web_app
