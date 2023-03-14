// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/sub_app_install_command.h"

#include <memory>
#include <ostream>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/web_applications/test/fake_data_retriever.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/test_web_app_url_loader.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_contents/web_app_data_retriever.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/subapps/sub_apps_service.mojom-shared.h"

namespace web_app {
namespace {

class SubAppInstallCommandTest : public WebAppTest {
 public:
  SubAppInstallCommandTest() = default;
  ~SubAppInstallCommandTest() override = default;

  void SetUp() override {
    WebAppTest::SetUp();
    FakeWebAppProvider* provider = FakeWebAppProvider::Get(profile());
    auto command_manager_url_loader = std::make_unique<TestWebAppUrlLoader>();
    command_manager_url_loader_ = command_manager_url_loader.get();
    provider->GetCommandManager().SetUrlLoaderForTesting(
        std::move(command_manager_url_loader));
    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

 protected:
  std::unique_ptr<SubAppInstallCommand> CreateCommand(
      AppId& parent_app_id,
      std::vector<std::pair<UnhashedAppId, GURL>> sub_app_data,
      SubAppInstallResultCallback callback,
      std::unique_ptr<WebAppUrlLoader> url_loader,
      std::unique_ptr<WebAppDataRetriever> data_retriever) {
    return std::make_unique<SubAppInstallCommand>(
        parent_app_id, sub_app_data, std::move(callback), profile(),
        std::move(url_loader), std::move(data_retriever));
  }

  AppInstallResults InstallSubAppAndWait(
      AppId& parent_app_id,
      std::pair<UnhashedAppId, GURL> sub_app_data,
      std::unique_ptr<WebAppDataRetriever> data_retriever,
      bool dialog_not_accepted = false,
      WebAppUrlLoader::Result url_load_result =
          WebAppUrlLoader::Result::kUrlLoaded) {
    auto url_loader = std::make_unique<TestWebAppUrlLoader>();
    url_loader->SetNextLoadUrlResult(sub_app_data.second, url_load_result);

    base::RunLoop run_loop;
    AppInstallResults results;
    std::unique_ptr<SubAppInstallCommand> command = CreateCommand(
        parent_app_id, {sub_app_data},
        base::BindLambdaForTesting([&](AppInstallResults output_results) {
          results = std::move(output_results);
          run_loop.Quit();
        }),
        std::move(url_loader), std::move(data_retriever));

    if (dialog_not_accepted) {
      command->SetDialogNotAcceptedForTesting();
    }

    provider()->command_manager().ScheduleCommand(std::move(command));
    run_loop.Run();
    return results;
  }

  AppId InstallParentApp() {
    auto web_app_info = std::make_unique<WebAppInstallInfo>();
    web_app_info->start_url = parent_url_;
    web_app_info->scope = parent_url_.GetWithoutFilename();
    web_app_info->user_display_mode = mojom::UserDisplayMode::kStandalone;
    web_app_info->title = u"Web App";
    return web_app::test::InstallWebApp(profile(), std::move(web_app_info));
  }

  std::unique_ptr<WebAppInstallInfo> GetWebAppInstallInfoForSubApp(
      const GURL& url) {
    auto web_app_info = std::make_unique<WebAppInstallInfo>();
    web_app_info->start_url = url;
    web_app_info->scope = url.GetWithoutFilename();
    web_app_info->user_display_mode = mojom::UserDisplayMode::kStandalone;
    web_app_info->title = u"Sub App";
    return web_app_info;
  }

  std::unique_ptr<FakeDataRetriever> GetDataRetrieverWithInfoAndManifest(
      const GURL& url,
      bool disable_web_app_info = false) {
    auto data_retriever = std::make_unique<FakeDataRetriever>();
    data_retriever->BuildDefaultDataToRetrieve(url, url.GetWithoutFilename());
    if (disable_web_app_info) {
      data_retriever->SetRendererWebAppInstallInfo(nullptr);
    } else {
      data_retriever->SetRendererWebAppInstallInfo(
          GetWebAppInstallInfoForSubApp(url));
    }
    return data_retriever;
  }

  std::vector<AppId> GetAllSubAppIds(const AppId& parent_app_id) {
    return registrar().GetAllSubAppIds(parent_app_id);
  }

  GURL sub_app_url() { return sub_app_url_; }
  WebAppProvider* provider() { return WebAppProvider::GetForTest(profile()); }
  WebAppRegistrar& registrar() { return provider()->registrar_unsafe(); }
  TestWebAppUrlLoader& command_manager_url_loader() const {
    return *command_manager_url_loader_;
  }

 private:
  const GURL parent_url_{"http://www.foo.bar/web_apps/basic.html"};
  const GURL sub_app_url_{"http://www.foo.bar/web_apps/standalone/basic.html"};

  base::raw_ptr<TestWebAppUrlLoader> command_manager_url_loader_;
};

TEST_F(SubAppInstallCommandTest, InstallSingleAppSuccess) {
  AppId parent_app_id = InstallParentApp();
  EXPECT_EQ(0ul, GetAllSubAppIds(parent_app_id).size());

  UnhashedAppId unhashed_sub_app_id =
      GenerateAppIdUnhashed(/*manifest_id=*/absl::nullopt, sub_app_url());
  AppId sub_app_id = GenerateAppIdFromUnhashed(unhashed_sub_app_id);

  std::pair<UnhashedAppId, GURL> data(unhashed_sub_app_id, sub_app_url());

  command_manager_url_loader().AddPrepareForLoadResults(
      {WebAppUrlLoader::Result::kUrlLoaded});
  AppInstallResults command_result = InstallSubAppAndWait(
      parent_app_id, data, GetDataRetrieverWithInfoAndManifest(sub_app_url()));

  std::pair<AppId, blink::mojom::SubAppsServiceResultCode> expected_result(
      unhashed_sub_app_id, blink::mojom::SubAppsServiceResultCode::kSuccess);

  // Verify command works fine, single sub_app is installed.
  EXPECT_EQ(1u, command_result.size());
  EXPECT_EQ(expected_result, command_result[0]);
  EXPECT_EQ(1ul, GetAllSubAppIds(parent_app_id).size());
  EXPECT_TRUE(registrar().IsInstalled(sub_app_id));
}

TEST_F(SubAppInstallCommandTest, InstallSingleAppAlreadyInstalled) {
  AppId parent_app_id = InstallParentApp();
  EXPECT_EQ(0ul, GetAllSubAppIds(parent_app_id).size());

  UnhashedAppId unhashed_sub_app_id =
      GenerateAppIdUnhashed(/*manifest_id=*/absl::nullopt, sub_app_url());
  AppId sub_app_id = GenerateAppIdFromUnhashed(unhashed_sub_app_id);

  std::pair<UnhashedAppId, GURL> data(unhashed_sub_app_id, sub_app_url());

  command_manager_url_loader().AddPrepareForLoadResults(
      {WebAppUrlLoader::Result::kUrlLoaded,
       WebAppUrlLoader::Result::kUrlLoaded});

  // Install first app as sub_app.
  AppInstallResults command_result = InstallSubAppAndWait(
      parent_app_id, data, GetDataRetrieverWithInfoAndManifest(sub_app_url()));
  std::pair<AppId, blink::mojom::SubAppsServiceResultCode> expected_result(
      unhashed_sub_app_id, blink::mojom::SubAppsServiceResultCode::kSuccess);
  EXPECT_EQ(1u, command_result.size());
  EXPECT_EQ(expected_result, command_result[0]);
  EXPECT_EQ(1ul, GetAllSubAppIds(parent_app_id).size());
  EXPECT_TRUE(registrar().IsInstalled(sub_app_id));

  // Reinstalling the same app as a sub_app should return a kSuccess.
  command_result = InstallSubAppAndWait(
      parent_app_id, data, GetDataRetrieverWithInfoAndManifest(sub_app_url()));
  std::pair<AppId, blink::mojom::SubAppsServiceResultCode>
      expected_result_installed(
          unhashed_sub_app_id,
          blink::mojom::SubAppsServiceResultCode::kSuccess);
  EXPECT_EQ(1u, command_result.size());
  EXPECT_EQ(expected_result_installed, command_result[0]);
  // No extra app is installed, old app is still installed.
  EXPECT_EQ(1ul, GetAllSubAppIds(parent_app_id).size());
  EXPECT_TRUE(registrar().IsInstalled(sub_app_id));
}

TEST_F(SubAppInstallCommandTest, InstallFailIfDialogNotAccepted) {
  AppId parent_app_id = InstallParentApp();
  EXPECT_EQ(0ul, GetAllSubAppIds(parent_app_id).size());

  UnhashedAppId unhashed_sub_app_id =
      GenerateAppIdUnhashed(/*manifest_id=*/absl::nullopt, sub_app_url());
  AppId sub_app_id = GenerateAppIdFromUnhashed(unhashed_sub_app_id);

  std::pair<UnhashedAppId, GURL> data(unhashed_sub_app_id, sub_app_url());

  command_manager_url_loader().AddPrepareForLoadResults(
      {WebAppUrlLoader::Result::kUrlLoaded});
  AppInstallResults command_result = InstallSubAppAndWait(
      parent_app_id, data, GetDataRetrieverWithInfoAndManifest(sub_app_url()),
      /*dialog_not_accepted=*/true);

  std::pair<AppId, blink::mojom::SubAppsServiceResultCode> expected_result(
      unhashed_sub_app_id, blink::mojom::SubAppsServiceResultCode::kFailure);

  // Verify command works and returns a kFailure.
  EXPECT_EQ(1u, command_result.size());
  EXPECT_EQ(expected_result, command_result[0]);
  EXPECT_EQ(0ul, GetAllSubAppIds(parent_app_id).size());
  EXPECT_FALSE(registrar().IsInstalled(sub_app_id));
}

TEST_F(SubAppInstallCommandTest, InstallFailIfExpectedAppIdCheckFails) {
  AppId parent_app_id = InstallParentApp();
  EXPECT_EQ(0ul, GetAllSubAppIds(parent_app_id).size());

  UnhashedAppId unhashed_sub_app_id =
      GenerateAppIdUnhashed(/*manifest_id=*/absl::nullopt, sub_app_url());
  AppId sub_app_id = GenerateAppIdFromUnhashed(unhashed_sub_app_id);

  std::pair<UnhashedAppId, GURL> data("http://abc.com/", sub_app_url());

  command_manager_url_loader().AddPrepareForLoadResults(
      {WebAppUrlLoader::Result::kUrlLoaded});
  AppInstallResults command_result = InstallSubAppAndWait(
      parent_app_id, data, GetDataRetrieverWithInfoAndManifest(sub_app_url()));

  std::pair<AppId, blink::mojom::SubAppsServiceResultCode> expected_result(
      "http://abc.com/", blink::mojom::SubAppsServiceResultCode::kFailure);

  // Verify command works and returns a kFailure.
  EXPECT_EQ(1u, command_result.size());
  EXPECT_EQ(expected_result, command_result[0]);
  EXPECT_EQ(0ul, GetAllSubAppIds(parent_app_id).size());
  EXPECT_FALSE(registrar().IsInstalled(sub_app_id));
}

TEST_F(SubAppInstallCommandTest, InstallFailsIfNoParentApp) {
  UnhashedAppId unhashed_sub_app_id =
      GenerateAppIdUnhashed(/*manifest_id=*/absl::nullopt, sub_app_url());
  AppId sub_app_id = GenerateAppIdFromUnhashed(unhashed_sub_app_id);
  AppId parent_app_id = "random_app";

  std::pair<UnhashedAppId, GURL> data(unhashed_sub_app_id, sub_app_url());

  command_manager_url_loader().AddPrepareForLoadResults(
      {WebAppUrlLoader::Result::kUrlLoaded});
  AppInstallResults command_result = InstallSubAppAndWait(
      parent_app_id, data, GetDataRetrieverWithInfoAndManifest(sub_app_url()));

  std::pair<AppId, blink::mojom::SubAppsServiceResultCode> expected_result(
      unhashed_sub_app_id, blink::mojom::SubAppsServiceResultCode::kFailure);

  // Verify command works and returns a kFailure.
  EXPECT_EQ(1u, command_result.size());
  EXPECT_EQ(expected_result, command_result[0]);
  EXPECT_EQ(0ul, GetAllSubAppIds(parent_app_id).size());
  EXPECT_FALSE(registrar().IsInstalled(sub_app_id));
}

TEST_F(SubAppInstallCommandTest, InstallFailsForUrlLoadingFailure) {
  AppId parent_app_id = InstallParentApp();
  EXPECT_EQ(0ul, GetAllSubAppIds(parent_app_id).size());

  UnhashedAppId unhashed_sub_app_id =
      GenerateAppIdUnhashed(/*manifest_id=*/absl::nullopt, sub_app_url());
  AppId sub_app_id = GenerateAppIdFromUnhashed(unhashed_sub_app_id);

  std::pair<UnhashedAppId, GURL> data(unhashed_sub_app_id, sub_app_url());

  command_manager_url_loader().AddPrepareForLoadResults(
      {WebAppUrlLoader::Result::kUrlLoaded});
  AppInstallResults command_result = InstallSubAppAndWait(
      parent_app_id, data, GetDataRetrieverWithInfoAndManifest(sub_app_url()),
      /*dialog_not_accepted=*/false,
      /*url_load_result=*/WebAppUrlLoader::Result::kRedirectedUrlLoaded);

  std::pair<AppId, blink::mojom::SubAppsServiceResultCode> expected_result(
      unhashed_sub_app_id, blink::mojom::SubAppsServiceResultCode::kFailure);

  // Verify command works and returns a kFailure.
  EXPECT_EQ(1u, command_result.size());
  EXPECT_EQ(expected_result, command_result[0]);
  EXPECT_EQ(0ul, GetAllSubAppIds(parent_app_id).size());
  EXPECT_FALSE(registrar().IsInstalled(sub_app_id));
}

TEST_F(SubAppInstallCommandTest, InstallFailsForWebAppInfoNotFound) {
  AppId parent_app_id = InstallParentApp();
  EXPECT_EQ(0ul, GetAllSubAppIds(parent_app_id).size());

  UnhashedAppId unhashed_sub_app_id =
      GenerateAppIdUnhashed(/*manifest_id=*/absl::nullopt, sub_app_url());
  AppId sub_app_id = GenerateAppIdFromUnhashed(unhashed_sub_app_id);

  std::pair<UnhashedAppId, GURL> data(unhashed_sub_app_id, sub_app_url());

  command_manager_url_loader().AddPrepareForLoadResults(
      {WebAppUrlLoader::Result::kUrlLoaded});
  AppInstallResults command_result =
      InstallSubAppAndWait(parent_app_id, data,
                           GetDataRetrieverWithInfoAndManifest(
                               sub_app_url(), /*disable_web_app_info=*/true));

  std::pair<AppId, blink::mojom::SubAppsServiceResultCode> expected_result(
      unhashed_sub_app_id, blink::mojom::SubAppsServiceResultCode::kFailure);

  // Verify command works and returns a kFailure.
  EXPECT_EQ(1u, command_result.size());
  EXPECT_EQ(expected_result, command_result[0]);
  EXPECT_EQ(0ul, GetAllSubAppIds(parent_app_id).size());
  EXPECT_FALSE(registrar().IsInstalled(sub_app_id));
}

}  // namespace
}  // namespace web_app
