// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/fetch_install_info_from_install_url_command.h"

#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/web_applications/test/fake_data_retriever.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/fake_web_contents_manager.h"
#include "chrome/browser/web_applications/test/test_web_app_url_loader.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {
namespace {

class FetchInstallInfoFromInstallUrlCommandTest : public WebAppTest {
 public:
  FetchInstallInfoFromInstallUrlCommandTest() = default;
  ~FetchInstallInfoFromInstallUrlCommandTest() override = default;

  void SetUp() override {
    WebAppTest::SetUp();
    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  void TearDown() override {
    provider().Shutdown();
    WebAppTest::TearDown();
  }

 protected:
  std::unique_ptr<WebAppInstallInfo> CreateAndRunCommand(
      GURL install_url,
      webapps::ManifestId manifest_id,
      bool disable_web_app_info = false,
      bool valid_manifest = true,
      WebAppUrlLoader::Result url_load_result =
          WebAppUrlLoader::Result::kUrlLoaded) {
    auto& install_page_state =
        web_contents_manager().GetOrCreatePageState(install_url);
    install_page_state.url_load_result = url_load_result;
    install_page_state.return_null_info = disable_web_app_info;

    install_page_state.has_service_worker = true;
    install_page_state.valid_manifest_for_web_app = valid_manifest;

    base::test::TestFuture<std::unique_ptr<WebAppInstallInfo>> install_future;
    std::unique_ptr<WebAppInstallInfo> results;
    std::unique_ptr<FetchInstallInfoFromInstallUrlCommand> command =
        std::make_unique<FetchInstallInfoFromInstallUrlCommand>(
            manifest_id, install_url, install_future.GetCallback());

    provider().command_manager().ScheduleCommand(std::move(command));
    EXPECT_TRUE(install_future.Wait());
    return install_future.Take();
  }

  GURL app_url() { return app_url_; }
  GURL manifest_id() { return manifest_id_; }

  WebAppProvider& provider() { return *WebAppProvider::GetForTest(profile()); }

  FakeWebContentsManager& web_contents_manager() {
    return static_cast<FakeWebContentsManager&>(
        provider().web_contents_manager());
  }

  std::string GetCommandErrorFromLog() {
    auto command_manager_logs =
        provider().command_manager().ToDebugValue().TakeDict();

    base::Value::List* command_logs =
        command_manager_logs.FindList("command_log");
    EXPECT_NE(command_logs, nullptr);

    auto command_log_info =
        base::ranges::find_if(*command_logs, [](const base::Value& value) {
          const base::Value::Dict* dict = value.GetIfDict();
          return dict && dict->FindString("name") &&
                 !dict->FindString("name")->compare(
                     "FetchInstallInfoFromInstallUrlCommand");
        });
    EXPECT_NE(command_log_info, command_logs->end());

    auto* command_log = command_log_info->GetDict().FindDict("value");
    EXPECT_NE(command_log, nullptr);

    std::string* command_error = command_log->FindString("command_result");
    EXPECT_NE(command_error, nullptr);

    return *command_error;
  }

 private:
  const GURL app_url_{"http://www.foo.bar/web_apps/basic.html"};
  const GURL manifest_id_ = GenerateManifestIdFromStartUrlOnly(app_url_);
};

TEST_F(FetchInstallInfoFromInstallUrlCommandTest, RetrievesCorrectly) {
  std::unique_ptr<WebAppInstallInfo> command_result =
      CreateAndRunCommand(app_url(), manifest_id());
  EXPECT_TRUE(command_result);
  EXPECT_EQ(GetCommandErrorFromLog(), "kAppInfoObtained");
}

TEST_F(FetchInstallInfoFromInstallUrlCommandTest, InvalidInstallUrl) {
  std::unique_ptr<WebAppInstallInfo> command_result =
      CreateAndRunCommand(GURL("invalid_url"), manifest_id());
  EXPECT_FALSE(command_result);
  EXPECT_EQ(GetCommandErrorFromLog(), "kInstallUrlInvalid");
}

TEST_F(FetchInstallInfoFromInstallUrlCommandTest, InvalidManifestId) {
  std::unique_ptr<WebAppInstallInfo> command_result =
      CreateAndRunCommand(app_url(), GURL("invalid_manifest_id"));
  EXPECT_FALSE(command_result);
  EXPECT_EQ(GetCommandErrorFromLog(), "kManifestIdInvalid");
}

TEST_F(FetchInstallInfoFromInstallUrlCommandTest, UrlLoadingFailure) {
  std::unique_ptr<WebAppInstallInfo> command_result = CreateAndRunCommand(
      app_url(), manifest_id(),
      /*disable_web_app_info=*/false, /*valid_manifest=*/true,
      WebAppUrlLoader::Result::kRedirectedUrlLoaded);
  EXPECT_FALSE(command_result);
  EXPECT_EQ(GetCommandErrorFromLog(), "kUrlLoadingFailure");
}

TEST_F(FetchInstallInfoFromInstallUrlCommandTest, NoValidManifest) {
  std::unique_ptr<WebAppInstallInfo> command_result = CreateAndRunCommand(
      app_url(), manifest_id(), /*disable_web_app_info=*/false,
      /*valid_manifest=*/false);
  EXPECT_FALSE(command_result);
  EXPECT_EQ(GetCommandErrorFromLog(), "kNoValidManifest");
}

TEST_F(FetchInstallInfoFromInstallUrlCommandTest, WrongManifestId) {
  std::unique_ptr<WebAppInstallInfo> command_result =
      CreateAndRunCommand(app_url(), GURL("https://www.wrong_manifest_id.com"));
  EXPECT_FALSE(command_result);
  EXPECT_EQ(GetCommandErrorFromLog(), "kWrongManifestId");
}

TEST_F(FetchInstallInfoFromInstallUrlCommandTest, WebAppInfoNotFound) {
  std::unique_ptr<WebAppInstallInfo> command_result = CreateAndRunCommand(
      app_url(), manifest_id(), /*disable_web_app_info=*/true);
  EXPECT_FALSE(command_result);
  EXPECT_EQ(GetCommandErrorFromLog(), "kFailure");
}

}  // namespace
}  // namespace web_app
