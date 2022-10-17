// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

namespace web_app {
namespace {

class UpdateFileHandlerCommandTest : public WebAppTest {
 public:
  const char* kTestAppName = "test app";
  const GURL kTestAppUrl = GURL("https://example.com");

  UpdateFileHandlerCommandTest() {
    scoped_feature_list_.InitAndEnableFeature(
        blink::features::kFileHandlingAPI);
  }

  ~UpdateFileHandlerCommandTest() override = default;

  void SetUp() override {
    WebAppTest::SetUp();
    provider_ = FakeWebAppProvider::Get(profile());
    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  WebAppProvider* provider() { return provider_; }

  void EnableFileHandlingAPI() {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitAndEnableFeature(
        blink::features::kFileHandlingAPI);
  }

  void DisableFileHandlingAPI() {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitAndDisableFeature(
        blink::features::kFileHandlingAPI);
  }

 private:
  raw_ptr<FakeWebAppProvider> provider_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(UpdateFileHandlerCommandTest, UserChoiceAllowPersisted) {
  const AppId app_id =
      test::InstallDummyWebApp(profile(), kTestAppName, kTestAppUrl);
  EXPECT_EQ(provider()->registrar().GetAppFileHandlerApprovalState(app_id),
            ApiApprovalState::kRequiresPrompt);

  base::RunLoop run_loop;
  provider()->scheduler().PersistFileHandlersUserChoice(
      app_id, /*allowed=*/true, run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_EQ(provider()->registrar().GetAppFileHandlerApprovalState(app_id),
            ApiApprovalState::kAllowed);
  EXPECT_TRUE(provider()->registrar().ExpectThatFileHandlersAreRegisteredWithOs(
      app_id));
}

TEST_F(UpdateFileHandlerCommandTest, UserChoiceDisallowPersisted) {
  const AppId app_id =
      test::InstallDummyWebApp(profile(), kTestAppName, kTestAppUrl);
  EXPECT_EQ(provider()->registrar().GetAppFileHandlerApprovalState(app_id),
            ApiApprovalState::kRequiresPrompt);

  base::RunLoop run_loop;
  provider()->scheduler().PersistFileHandlersUserChoice(
      app_id, /*allowed=*/false, run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_EQ(provider()->registrar().GetAppFileHandlerApprovalState(app_id),
            ApiApprovalState::kDisallowed);
  EXPECT_FALSE(
      provider()->registrar().ExpectThatFileHandlersAreRegisteredWithOs(
          app_id));
}

TEST_F(UpdateFileHandlerCommandTest, UpdateFileHandler) {
  const AppId app_id =
      test::InstallDummyWebApp(profile(), kTestAppName, kTestAppUrl);
  EXPECT_EQ(provider()->registrar().GetAppFileHandlerApprovalState(app_id),
            ApiApprovalState::kRequiresPrompt);

  DisableFileHandlingAPI();

  base::RunLoop run_loop;
  provider()->scheduler().UpdateFileHandlerOsIntegration(
      app_id, run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_EQ(provider()->registrar().GetAppFileHandlerApprovalState(app_id),
            ApiApprovalState::kRequiresPrompt);
  EXPECT_FALSE(
      provider()->registrar().ExpectThatFileHandlersAreRegisteredWithOs(
          app_id));

  EnableFileHandlingAPI();

  base::RunLoop run_loop_2;
  provider()->scheduler().UpdateFileHandlerOsIntegration(
      app_id, run_loop_2.QuitClosure());
  run_loop_2.Run();
  EXPECT_TRUE(provider()->registrar().ExpectThatFileHandlersAreRegisteredWithOs(
      app_id));
}

}  // namespace
}  // namespace web_app
