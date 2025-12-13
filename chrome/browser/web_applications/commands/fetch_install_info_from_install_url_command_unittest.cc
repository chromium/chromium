// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/fetch_install_info_from_install_url_command.h"

#include <optional>

#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
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

 protected:
  std::unique_ptr<WebAppInstallInfo> CreateAndRunCommand(
      GURL install_url,
      webapps::ManifestId manifest_id,
      std::optional<webapps::ManifestId> parent_manifest_id,
      bool disable_web_app_info = false,
      bool valid_manifest = true,
      webapps::WebAppUrlLoaderResult url_load_result =
          webapps::WebAppUrlLoaderResult::kUrlLoaded) {
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
            manifest_id, install_url, parent_manifest_id,
            install_future.GetCallback());

    provider().command_manager().ScheduleCommand(std::move(command));
    EXPECT_TRUE(install_future.Wait());
    return install_future.Take();
  }

  GURL app_url() const { return app_url_; }
  GURL manifest_id() const { return manifest_id_; }
  GURL parent_app_url() const { return parent_app_url_; }
  GURL parent_manifest_id() const { return parent_manifest_id_; }
  GURL wrong_parent_manifest_id() const { return wrong_parent_manifest_id_; }
  WebAppProvider& provider() { return *WebAppProvider::GetForTest(profile()); }

  FakeWebContentsManager& web_contents_manager() {
    return static_cast<FakeWebContentsManager&>(
        provider().web_contents_manager());
  }

 private:
  const GURL app_url_{"http://www.foo.bar/web_apps/basic.html"};
  const GURL parent_app_url_{"http://www.foo.bar/basic.html"};
  const webapps::ManifestId manifest_id_ =
      GenerateManifestIdFromStartUrlOnly(app_url_);
  const webapps::ManifestId parent_manifest_id_ =
      GenerateManifestIdFromStartUrlOnly(parent_app_url_);
  const webapps::ManifestId wrong_parent_manifest_id_ =
      GenerateManifestIdFromStartUrlOnly(
          GURL("http://other.origin.com/basic.html"));
};

TEST_F(FetchInstallInfoFromInstallUrlCommandTest, RetrievesCorrectly) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<WebAppInstallInfo> command_result =
      CreateAndRunCommand(app_url(), manifest_id(), parent_manifest_id());
  EXPECT_TRUE(command_result);
  histogram_tester.ExpectUniqueSample(
      "WebApp.Install.FetchInstallInfoFromInstallUrlResult",
      FetchInstallInfoResult::kAppInfoObtained, 1);
}

TEST_F(FetchInstallInfoFromInstallUrlCommandTest,
       RetrievesCorrectlyWhenParentManifestIsNull) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<WebAppInstallInfo> command_result =
      CreateAndRunCommand(app_url(), manifest_id(), std::nullopt);
  EXPECT_TRUE(command_result);
  histogram_tester.ExpectUniqueSample(
      "WebApp.Install.FetchInstallInfoFromInstallUrlResult",
      FetchInstallInfoResult::kAppInfoObtained, 1);
}

TEST_F(FetchInstallInfoFromInstallUrlCommandTest, UrlLoadingFailure) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<WebAppInstallInfo> command_result = CreateAndRunCommand(
      app_url(), manifest_id(), parent_manifest_id(),
      /*disable_web_app_info=*/false, /*valid_manifest=*/true,
      webapps::WebAppUrlLoaderResult::kRedirectedUrlLoaded);
  EXPECT_FALSE(command_result);
  histogram_tester.ExpectUniqueSample(
      "WebApp.Install.FetchInstallInfoFromInstallUrlResult",
      FetchInstallInfoResult::kUrlLoadingFailure, 1);
}

TEST_F(FetchInstallInfoFromInstallUrlCommandTest, NoValidManifest) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<WebAppInstallInfo> command_result =
      CreateAndRunCommand(app_url(), manifest_id(), parent_manifest_id(),
                          /*disable_web_app_info=*/false,
                          /*valid_manifest=*/false);
  EXPECT_FALSE(command_result);
  histogram_tester.ExpectUniqueSample(
      "WebApp.Install.FetchInstallInfoFromInstallUrlResult",
      FetchInstallInfoResult::kNoValidManifest, 1);
}

TEST_F(FetchInstallInfoFromInstallUrlCommandTest, WebAppInfoNotFound) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<WebAppInstallInfo> command_result =
      CreateAndRunCommand(app_url(), manifest_id(), parent_manifest_id(),
                          /*disable_web_app_info=*/true);
  EXPECT_FALSE(command_result);
  histogram_tester.ExpectUniqueSample(
      "WebApp.Install.FetchInstallInfoFromInstallUrlResult",
      FetchInstallInfoResult::kFailure, 1);
}

}  // namespace
}  // namespace web_app
