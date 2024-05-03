// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/fetch_installability_for_chrome_management.h"

#include <memory>
#include <optional>

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "chrome/browser/web_applications/test/fake_data_retriever.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/test_web_app_url_loader.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_contents/web_app_data_retriever.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "components/webapps/browser/web_contents/web_app_url_loader.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {
namespace {

using testing::AllOf;
using testing::Eq;
using testing::Field;

class FetchInstallabilityForChromeManagementTest : public WebAppTest {
 public:
  const GURL kWebAppUrl = GURL("https://example.com/path/index.html");
  const GURL kWebAppScope = GURL("https://example.com/path/");
  const std::string kWebAppName = "Example App";
  const webapps::AppId kWebAppId =
      GenerateAppId(/*manifest_id=*/std::nullopt, kWebAppUrl);

  FetchInstallabilityForChromeManagementTest() = default;
  ~FetchInstallabilityForChromeManagementTest() override = default;

  blink::mojom::ManifestPtr CreateManifest() {
    auto manifest = blink::mojom::Manifest::New();
    manifest->start_url = kWebAppUrl;
    manifest->id = GenerateManifestIdFromStartUrlOnly(kWebAppUrl);
    manifest->scope = kWebAppScope;
    manifest->short_name = base::ASCIIToUTF16(kWebAppName);
    return manifest;
  }

  std::unique_ptr<WebAppInstallInfo> CreateWebAppInfo() {
    auto install_info =
        WebAppInstallInfo::CreateWithStartUrlForTesting(kWebAppUrl);
    install_info->scope = kWebAppScope;
    install_info->title = base::ASCIIToUTF16(kWebAppName);
    return install_info;
  }

  struct FetchResult {
    InstallableCheckResult result = InstallableCheckResult::kInstallable;
    std::optional<webapps::AppId> app_id = std::nullopt;
  };

  FetchResult ScheduleCommandAndWait(
      const GURL& url,
      base::WeakPtr<content::WebContents> web_contents,
      std::unique_ptr<webapps::WebAppUrlLoader> url_loader,
      std::unique_ptr<WebAppDataRetriever> data_retriever) {
    base::RunLoop run_loop;
    FetchResult output;
    WebAppProvider* provider = WebAppProvider::GetForTest(profile());
    provider->command_manager().ScheduleCommand(
        std::make_unique<FetchInstallabilityForChromeManagement>(
            url, std::move(web_contents), std::move(url_loader),
            std::move(data_retriever),
            base::BindLambdaForTesting(
                [&](InstallableCheckResult result,
                    std::optional<webapps::AppId> app_id) {
                  output.result = result;
                  output.app_id = app_id;
                  run_loop.Quit();
                })));
    run_loop.Run();
    return output;
  }

  void SetUp() override {
    WebAppTest::SetUp();
    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  WebAppProvider* provider() { return WebAppProvider::GetForTest(profile()); }
};

TEST_F(FetchInstallabilityForChromeManagementTest, UrlLoadError) {
  auto url_loader = std::make_unique<TestWebAppUrlLoader>();
  auto data_retriever = std::make_unique<FakeDataRetriever>();

  // Url loading fails.
  url_loader->SetNextLoadUrlResult(
      kWebAppUrl, webapps::WebAppUrlLoaderResult::kFailedUnknownReason);
  FetchResult result =
      ScheduleCommandAndWait(kWebAppUrl, web_contents()->GetWeakPtr(),
                             std::move(url_loader), std::move(data_retriever));
  EXPECT_THAT(result, AllOf(Field(&FetchResult::result,
                                  Eq(InstallableCheckResult::kNotInstallable)),
                            Field(&FetchResult::app_id, Eq(std::nullopt))));
}

TEST_F(FetchInstallabilityForChromeManagementTest, NotInstallable) {
  auto url_loader = std::make_unique<TestWebAppUrlLoader>();
  auto data_retriever = std::make_unique<FakeDataRetriever>();

  // Url loading succeeds, but manifest fetch says not installable.
  url_loader->SetNextLoadUrlResult(kWebAppUrl,
                                   webapps::WebAppUrlLoaderResult::kUrlLoaded);
  data_retriever->SetManifest(blink::mojom::ManifestPtr(),
                              webapps::InstallableStatusCode::NO_MANIFEST);

  FetchResult result =
      ScheduleCommandAndWait(kWebAppUrl, web_contents()->GetWeakPtr(),
                             std::move(url_loader), std::move(data_retriever));
  EXPECT_THAT(result, AllOf(Field(&FetchResult::result,
                                  Eq(InstallableCheckResult::kNotInstallable)),
                            Field(&FetchResult::app_id, Eq(std::nullopt))));
}

TEST_F(FetchInstallabilityForChromeManagementTest, Installable) {
  auto url_loader = std::make_unique<TestWebAppUrlLoader>();
  auto data_retriever = std::make_unique<FakeDataRetriever>();
  // Url loading succeeds and manifest loads. No apps installed yet, so succeed!
  url_loader->SetNextLoadUrlResult(kWebAppUrl,
                                   webapps::WebAppUrlLoaderResult::kUrlLoaded);
  data_retriever->SetManifest(
      CreateManifest(), webapps::InstallableStatusCode::NO_ERROR_DETECTED);

  FetchResult result =
      ScheduleCommandAndWait(kWebAppUrl, web_contents()->GetWeakPtr(),
                             std::move(url_loader), std::move(data_retriever));
  EXPECT_THAT(result, AllOf(Field(&FetchResult::result,
                                  Eq(InstallableCheckResult::kInstallable)),
                            Field(&FetchResult::app_id, Eq(kWebAppId))));
}

TEST_F(FetchInstallabilityForChromeManagementTest, AlreadyInstalled) {
  auto url_loader = std::make_unique<TestWebAppUrlLoader>();
  auto data_retriever = std::make_unique<FakeDataRetriever>();
  // Url loading succeeds and manifest loads. No apps installed yet, so succeed!
  url_loader->SetNextLoadUrlResult(kWebAppUrl,
                                   webapps::WebAppUrlLoaderResult::kUrlLoaded);
  data_retriever->SetManifest(
      CreateManifest(), webapps::InstallableStatusCode::NO_ERROR_DETECTED);

  test::InstallWebApp(profile(), CreateWebAppInfo());

  FetchResult result =
      ScheduleCommandAndWait(kWebAppUrl, web_contents()->GetWeakPtr(),
                             std::move(url_loader), std::move(data_retriever));
  EXPECT_THAT(result,
              AllOf(Field(&FetchResult::result,
                          Eq(InstallableCheckResult::kAlreadyInstalled)),
                    Field(&FetchResult::app_id, Eq(kWebAppId))));
}

}  // namespace
}  // namespace web_app
