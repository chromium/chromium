// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/os_integration/web_app_file_handler_manager.h"
#include "chrome/browser/web_applications/os_integration/web_app_protocol_handler_manager.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace web_app {
namespace {

class UpdateFileHandlerCommandTest : public WebAppTest {
 public:
  const char* kTestAppName = "test app";
  const GURL kTestAppUrl = GURL("https://example.com");

  UpdateFileHandlerCommandTest() = default;
  ~UpdateFileHandlerCommandTest() override = default;

  void SetUp() override {
    WebAppTest::SetUp();
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      test_override_ = OsIntegrationTestOverrideImpl::OverrideForTesting();
    }
    provider_ = FakeWebAppProvider::Get(profile());

    auto file_handler_manager =
        std::make_unique<WebAppFileHandlerManager>(profile());
    auto protocol_handler_manager =
        std::make_unique<WebAppProtocolHandlerManager>(profile());
    auto os_integration_manager = std::make_unique<OsIntegrationManager>(
        profile(), std::move(file_handler_manager),
        std::move(protocol_handler_manager));

    provider_->SetOsIntegrationManager(std::move(os_integration_manager));
    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  void TearDown() override {
    // Blocking required due to file operations in the shortcut override
    // destructor.
    test::UninstallAllWebApps(profile());
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      test_override_.reset();
    }
    WebAppTest::TearDown();
  }

  WebAppProvider* provider() { return provider_; }

 private:
  raw_ptr<FakeWebAppProvider, DanglingUntriaged> provider_ = nullptr;
  std::unique_ptr<OsIntegrationTestOverrideImpl::BlockingRegistration>
      test_override_;
};

TEST_F(UpdateFileHandlerCommandTest, UserChoiceAllowPersisted) {
  const webapps::AppId app_id =
      test::InstallDummyWebApp(profile(), kTestAppName, kTestAppUrl);
  EXPECT_EQ(
      provider()->registrar_unsafe().GetAppFileHandlerApprovalState(app_id),
      ApiApprovalState::kRequiresPrompt);

  base::RunLoop run_loop;
  provider()->scheduler().PersistFileHandlersUserChoice(
      app_id, /*allowed=*/true, run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_EQ(
      provider()->registrar_unsafe().GetAppFileHandlerApprovalState(app_id),
      ApiApprovalState::kAllowed);
  EXPECT_TRUE(
      provider()->registrar_unsafe().ExpectThatFileHandlersAreRegisteredWithOs(
          app_id));
}

TEST_F(UpdateFileHandlerCommandTest, UserChoiceDisallowPersisted) {
  const webapps::AppId app_id =
      test::InstallDummyWebApp(profile(), kTestAppName, kTestAppUrl);
  EXPECT_EQ(
      provider()->registrar_unsafe().GetAppFileHandlerApprovalState(app_id),
      ApiApprovalState::kRequiresPrompt);

  base::RunLoop run_loop;
  provider()->scheduler().PersistFileHandlersUserChoice(
      app_id, /*allowed=*/false, run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_EQ(
      provider()->registrar_unsafe().GetAppFileHandlerApprovalState(app_id),
      ApiApprovalState::kDisallowed);
  EXPECT_FALSE(
      provider()->registrar_unsafe().ExpectThatFileHandlersAreRegisteredWithOs(
          app_id));
}

}  // namespace
}  // namespace web_app
