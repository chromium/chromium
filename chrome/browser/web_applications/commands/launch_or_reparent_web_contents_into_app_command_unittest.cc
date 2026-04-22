// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/launch_or_reparent_web_contents_into_app_command.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/fake_web_app_ui_manager.h"
#include "chrome/browser/web_applications/test/fake_web_contents_manager.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {
namespace {

class LaunchOrReparentWebContentsIntoAppCommandTest : public WebAppTest {
 public:
  void SetUp() override {
    WebAppTest::SetUp();
    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  FakeWebAppUiManager& fake_ui_manager() {
    return static_cast<FakeWebAppUiManager&>(fake_provider().ui_manager());
  }
};

TEST_F(LaunchOrReparentWebContentsIntoAppCommandTest, ReparentsWhenInScope) {
  // Install a dummy app so it's in the registrar.
  const webapps::AppId app_id = test::InstallDummyWebApp(
      profile(), "Test App", GURL("https://example.com/"));

  auto web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  fake_web_contents_manager().SetUrlLoaded(web_contents.get(),
                                           GURL("https://example.com/"));

  base::HistogramTester histogram_tester;
  base::test::TestFuture<LaunchOrReparentResult> future;
  provider().scheduler().LaunchOrReparentWebContentsIntoApp(
      app_id, web_contents->GetWeakPtr(), future.GetCallback());

  EXPECT_EQ(LaunchOrReparentResult::kReparented, future.Get());
  EXPECT_EQ(1, fake_ui_manager().num_reparent_tab_calls());

  // Verify metrics
  histogram_tester.ExpectUniqueSample("WebApp.Command.LaunchOrReparentResult",
                                      LaunchOrReparentResult::kReparented, 1);
}

TEST_F(LaunchOrReparentWebContentsIntoAppCommandTest, LaunchesWhenOutOfScope) {
  const webapps::AppId app_id = test::InstallDummyWebApp(
      profile(), "Test App", GURL("https://example.com/"));

  auto web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  // Set a different app ID or none.
  fake_web_contents_manager().SetUrlLoaded(
      web_contents.get(), GURL("https://different-example.com/"));

  base::HistogramTester histogram_tester;
  base::test::TestFuture<LaunchOrReparentResult> future;
  provider().scheduler().LaunchOrReparentWebContentsIntoApp(
      app_id, web_contents->GetWeakPtr(), future.GetCallback());

  EXPECT_EQ(LaunchOrReparentResult::kLaunched, future.Get());
  EXPECT_EQ(0, fake_ui_manager().num_reparent_tab_calls());

  histogram_tester.ExpectUniqueSample("WebApp.Command.LaunchOrReparentResult",
                                      LaunchOrReparentResult::kLaunched, 1);
}

TEST_F(LaunchOrReparentWebContentsIntoAppCommandTest,
       SkippedWhenWebContentsIsDestroyed) {
  const webapps::AppId app_id = test::InstallDummyWebApp(
      profile(), "Test App", GURL("https://example.com/"));

  auto web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  base::WeakPtr<content::WebContents> weak_web_contents =
      web_contents->GetWeakPtr();

  // Destroy the web contents.
  web_contents.reset();

  base::HistogramTester histogram_tester;
  base::test::TestFuture<LaunchOrReparentResult> future;
  provider().scheduler().LaunchOrReparentWebContentsIntoApp(
      app_id, weak_web_contents, future.GetCallback());

  EXPECT_EQ(LaunchOrReparentResult::kWebContentsGone, future.Get());
  EXPECT_EQ(0, fake_ui_manager().num_reparent_tab_calls());

  histogram_tester.ExpectUniqueSample("WebApp.Command.LaunchOrReparentResult",
                                      LaunchOrReparentResult::kWebContentsGone,
                                      1);
}

}  // namespace
}  // namespace web_app
