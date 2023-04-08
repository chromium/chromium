// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/install_from_sync_command.h"

#include <memory>
#include <ostream>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/mock_data_retriever.h"
#include "chrome/browser/web_applications/test/test_web_app_url_loader.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/browser/web_applications/web_contents/web_app_data_retriever.h"
#include "components/services/app_service/public/cpp/icon_info.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "components/webapps/browser/installable/installable_manager.h"
#include "content/public/browser/web_contents_user_data.h"
#include "net/http/http_status_code.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "url/gurl.h"

namespace apps {
// Required for gmock matchers.
void PrintTo(const IconInfo& info, std::ostream* os) {
  *os << info.AsDebugValue().DebugString();
}
}  // namespace apps

namespace web_app {
namespace {

using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Eq;

SkBitmap CreateTestBitmap(SkColor color, int size) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(size, size);
  bitmap.eraseColor(color);
  return bitmap;
}

class InstallFromSyncTest : public WebAppTest {
 public:
  const int kIconSize = 96;
  const GURL kWebAppUrl = GURL("https://example.com/path/index.html");
  const GURL kOtherWebAppUrl = GURL("https://example.com/path2/index.html");

  const GURL kWebAppManifestStartUrl =
      GURL("https://example.com/path/index.html");
  const std::u16string kManifestName = u"Manifest Name";
  const GURL kWebAppManifestUrl =
      GURL("https://example.com/path/manifest.json");
  const GURL kManifestIconUrl =
      GURL("https://example.com/path/manifest_icon.png");
  const SkColor kManifestIconColor = SK_ColorBLACK;

  const std::string kFallbackTitle = "Fallback Title";
  const GURL kFallbackIconUrl =
      GURL("https://example.com/path/fallback_icon.png");
  const SkColor kFallbackIconColor = SK_ColorBLUE;

  const std::u16string kDocumentTitle = u"Document Title";
  const GURL kDocumentIconUrl =
      GURL("https://example.com/path/document_icon.png");
  const SkColor kDocumentIconColor = SK_ColorRED;

  InstallFromSyncTest() = default;
  ~InstallFromSyncTest() override = default;

  void SetUp() override {
    WebAppTest::SetUp();
    FakeWebAppProvider* provider = FakeWebAppProvider::Get(profile());
    auto command_url_loader = std::make_unique<TestWebAppUrlLoader>();
    command_manager_url_loader_ = command_url_loader.get();
    provider->GetCommandManager().SetUrlLoaderForTesting(
        std::move(command_url_loader));
    url_loader_ = std::make_unique<TestWebAppUrlLoader>();
    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  void TearDown() override { WebAppTest::TearDown(); }

 protected:
  struct InstallResult {
    AppId installed_app_id;
    webapps::InstallResultCode install_code;
    absl::optional<webapps::InstallResultCode> install_code_before_fallback;
  };

  InstallFromSyncCommand::Params CreateParams(AppId app_id, GURL url) {
    return InstallFromSyncCommand::Params(
        app_id, /*manifest_id=*/absl::nullopt, url, kFallbackTitle,
        url.GetWithoutFilename(), /*theme_color=*/absl::nullopt,
        mojom::UserDisplayMode::kStandalone, /*icons=*/
        {apps::IconInfo(kFallbackIconUrl, kIconSize)});
  }

  std::unique_ptr<InstallFromSyncCommand> CreateCommand(
      std::unique_ptr<WebAppDataRetriever> data_retriever,
      InstallFromSyncCommand::Params params,
      OnceInstallCallback install_callback) {
    return std::make_unique<InstallFromSyncCommand>(
        &url_loader(), profile(), std::move(data_retriever), params,
        std::move(install_callback));
  }

  InstallResult InstallFromSyncAndWait(
      std::unique_ptr<WebAppDataRetriever> data_retriever,
      absl::optional<GURL> opt_url = absl::nullopt) {
    GURL url = opt_url.value_or(kWebAppUrl);
    const AppId app_id = GenerateAppId(/*manifest_id=*/absl::nullopt, url);

    InstallResult result;
    base::RunLoop run_loop;
    std::unique_ptr<InstallFromSyncCommand> command = CreateCommand(
        std::move(data_retriever), CreateParams(app_id, url),
        base::BindLambdaForTesting(
            [&](const AppId& id, webapps::InstallResultCode code) {
              result.installed_app_id = id;
              result.install_code = code;
              run_loop.Quit();
            }));
    command->SetFallbackTriggeredForTesting(
        base::BindLambdaForTesting([&](webapps::InstallResultCode code) {
          result.install_code_before_fallback = code;
        }));
    command_manager().ScheduleCommand(std::move(command));
    run_loop.Run();
    return result;
  }

  WebAppProvider* provider() { return WebAppProvider::GetForTest(profile()); }

  WebAppCommandManager& command_manager() {
    return provider()->command_manager();
  }

  WebAppRegistrar& registrar() { return provider()->registrar_unsafe(); }

  TestWebAppUrlLoader& command_manager_url_loader() const {
    return *command_manager_url_loader_;
  }

  TestWebAppUrlLoader& url_loader() const { return *url_loader_; }

  enum class IconFetchSource { kFallback, kDocument, kManifest };

  void ExpectGetIcons(MockDataRetriever* data_retriever,
                      bool skip_page_favicons,
                      IconFetchSource source) {
    GURL icon_url;
    SkColor color;
    switch (source) {
      case IconFetchSource::kFallback:
        icon_url = kFallbackIconUrl;
        color = kFallbackIconColor;
        break;
      case IconFetchSource::kDocument:
        icon_url = kDocumentIconUrl;
        color = kDocumentIconColor;

        break;
      case IconFetchSource::kManifest:
        icon_url = kManifestIconUrl;
        color = kManifestIconColor;
        break;
    }

    IconsMap icons = {{icon_url, {CreateTestBitmap(color, kIconSize)}}};
    DownloadedIconsHttpResults http_result = {
        {icon_url, net::HttpStatusCode::HTTP_OK}};
    EXPECT_CALL(*data_retriever,
                GetIcons(testing::_, ElementsAre(icon_url), skip_page_favicons,
                         base::test::IsNotNullCallback()))
        .WillOnce(base::test::RunOnceCallback<3>(
            IconsDownloadedResult::kCompleted, std::move(icons), http_result));
  }

  std::unique_ptr<WebAppInstallInfo> CreateSiteInstallInfo(
      absl::optional<GURL> opt_url = absl::nullopt) {
    GURL url = opt_url.value_or(kWebAppUrl);
    std::unique_ptr<WebAppInstallInfo> info =
        std::make_unique<WebAppInstallInfo>();
    info->title = kDocumentTitle;
    info->start_url = url;
    info->manifest_icons = {apps::IconInfo(kDocumentIconUrl, kIconSize)};
    return info;
  }

  blink::mojom::ManifestPtr CreateManifest(
      bool icons,
      absl::optional<GURL> opt_url = absl::nullopt) {
    GURL url = opt_url.value_or(kWebAppManifestStartUrl);
    blink::mojom::ManifestPtr manifest = blink::mojom::Manifest::New();
    manifest->name = kManifestName;
    manifest->start_url = url;
    if (icons) {
      blink::Manifest::ImageResource primary_icon;
      primary_icon.type = u"image/png";
      primary_icon.sizes.emplace_back(gfx::Size(kIconSize, kIconSize));
      primary_icon.purpose.push_back(
          blink::mojom::ManifestImageResource_Purpose::ANY);
      primary_icon.src = GURL(kManifestIconUrl);
      manifest->icons.push_back(primary_icon);
    }
    return manifest;
  }

  std::u16string GetAppName(const AppId& app_id) {
    return base::UTF8ToUTF16(registrar().GetAppShortName(app_id));
  }

  base::raw_ptr<TestWebAppUrlLoader> command_manager_url_loader_;
  std::unique_ptr<TestWebAppUrlLoader> url_loader_;
};

TEST_F(InstallFromSyncTest, SuccessWithManifest) {
  const AppId app_id = GenerateAppId(/*manifest_id=*/absl::nullopt, kWebAppUrl);

  auto data_retriever =
      std::make_unique<testing::StrictMock<MockDataRetriever>>();

  command_manager_url_loader().AddPrepareForLoadResults(
      {WebAppUrlLoader::Result::kUrlLoaded});
  url_loader().SetNextLoadUrlResult(kWebAppUrl,
                                    WebAppUrlLoader::Result::kUrlLoaded);
  testing::InSequence sequence;
  EXPECT_CALL(*data_retriever,
              GetWebAppInstallInfo(testing::_, base::test::IsNotNullCallback()))
      .WillOnce(base::test::RunOnceCallback<1>(CreateSiteInstallInfo()));

  EXPECT_CALL(*data_retriever, CheckInstallabilityAndRetrieveManifest(
                                   testing::_, true,
                                   base::test::IsNotNullCallback(), testing::_))
      .WillOnce(base::test::RunOnceCallback<2>(
          CreateManifest(true), kWebAppManifestUrl, true,
          webapps::InstallableStatusCode::NO_ERROR_DETECTED));

  ExpectGetIcons(data_retriever.get(), /*skip_page_favicons=*/true,
                 IconFetchSource::kManifest);

  InstallResult result = InstallFromSyncAndWait(std::move(data_retriever));

  EXPECT_FALSE(result.install_code_before_fallback.has_value());
  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
            result.install_code);
  EXPECT_EQ(result.installed_app_id, app_id);
  EXPECT_TRUE(registrar().IsInstalled(app_id));

  // Check that the manifest info was installed.
  EXPECT_THAT(GetAppName(app_id), Eq(kManifestName));
  EXPECT_THAT(registrar().GetAppIconInfos(app_id),
              ElementsAre(apps::IconInfo(kManifestIconUrl, kIconSize)));
  SkColor icon_color = IconManagerReadAppIconPixel(provider()->icon_manager(),
                                                   app_id, kIconSize);
  EXPECT_THAT(icon_color, Eq(kManifestIconColor));
}

TEST_F(InstallFromSyncTest, SuccessWithoutManifest) {
  const AppId app_id = GenerateAppId(/*manifest_id=*/absl::nullopt, kWebAppUrl);

  auto data_retriever =
      std::make_unique<testing::StrictMock<MockDataRetriever>>();

  command_manager_url_loader().AddPrepareForLoadResults(
      {WebAppUrlLoader::Result::kUrlLoaded});
  url_loader().SetNextLoadUrlResult(kWebAppUrl,
                                    WebAppUrlLoader::Result::kUrlLoaded);
  testing::InSequence sequence;
  EXPECT_CALL(*data_retriever,
              GetWebAppInstallInfo(testing::_, base::test::IsNotNullCallback()))
      .WillOnce(base::test::RunOnceCallback<1>(CreateSiteInstallInfo()));

  EXPECT_CALL(*data_retriever, CheckInstallabilityAndRetrieveManifest(
                                   testing::_, true,
                                   base::test::IsNotNullCallback(), testing::_))
      .WillOnce(base::test::RunOnceCallback<2>(
          nullptr, kWebAppManifestUrl, true,
          webapps::InstallableStatusCode::NO_ERROR_DETECTED));

  ExpectGetIcons(data_retriever.get(), /*skip_page_favicons=*/true,
                 IconFetchSource::kDocument);

  InstallResult result = InstallFromSyncAndWait(std::move(data_retriever));

  EXPECT_FALSE(result.install_code_before_fallback.has_value());
  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
            result.install_code);
  EXPECT_EQ(result.installed_app_id, app_id);
  EXPECT_TRUE(registrar().IsInstalled(app_id));

  // Check that the document & fallback info was installed.
  EXPECT_THAT(registrar().GetAppShortName(app_id), Eq(kFallbackTitle));
  EXPECT_THAT(registrar().GetAppIconInfos(app_id),
              ElementsAre(apps::IconInfo(kDocumentIconUrl, kIconSize)));
  SkColor icon_color = IconManagerReadAppIconPixel(provider()->icon_manager(),
                                                   app_id, kIconSize);
  EXPECT_THAT(icon_color, Eq(kDocumentIconColor));
}

TEST_F(InstallFromSyncTest, SuccessManifestNoIcons) {
  const AppId app_id = GenerateAppId(/*manifest_id=*/absl::nullopt, kWebAppUrl);

  auto data_retriever =
      std::make_unique<testing::StrictMock<MockDataRetriever>>();

  command_manager_url_loader().AddPrepareForLoadResults(
      {WebAppUrlLoader::Result::kUrlLoaded});
  url_loader().SetNextLoadUrlResult(kWebAppUrl,
                                    WebAppUrlLoader::Result::kUrlLoaded);
  testing::InSequence sequence;
  EXPECT_CALL(*data_retriever,
              GetWebAppInstallInfo(testing::_, base::test::IsNotNullCallback()))
      .WillOnce(base::test::RunOnceCallback<1>(CreateSiteInstallInfo()));

  EXPECT_CALL(*data_retriever, CheckInstallabilityAndRetrieveManifest(
                                   testing::_, true,
                                   base::test::IsNotNullCallback(), testing::_))
      .WillOnce(base::test::RunOnceCallback<2>(
          CreateManifest(/*icons=*/false), kWebAppManifestUrl, true,
          webapps::InstallableStatusCode::NO_ERROR_DETECTED));

  ExpectGetIcons(data_retriever.get(), /*skip_page_favicons=*/true,
                 IconFetchSource::kDocument);

  InstallResult result = InstallFromSyncAndWait(std::move(data_retriever));

  EXPECT_FALSE(result.install_code_before_fallback.has_value());
  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
            result.install_code);
  EXPECT_EQ(result.installed_app_id, app_id);
  EXPECT_TRUE(registrar().IsInstalled(app_id));

  // Check that the manifest was used & document icons were used.
  EXPECT_THAT(GetAppName(app_id), Eq(kManifestName));
  EXPECT_THAT(registrar().GetAppIconInfos(app_id),
              ElementsAre(apps::IconInfo(kDocumentIconUrl, kIconSize)));
  SkColor icon_color = IconManagerReadAppIconPixel(provider()->icon_manager(),
                                                   app_id, kIconSize);
  EXPECT_THAT(icon_color, Eq(kDocumentIconColor));
}

TEST_F(InstallFromSyncTest, FallbackUrlRedirect) {
  const AppId app_id = GenerateAppId(/*manifest_id=*/absl::nullopt, kWebAppUrl);

  auto data_retriever =
      std::make_unique<testing::StrictMock<MockDataRetriever>>();

  command_manager_url_loader().AddPrepareForLoadResults(
      {WebAppUrlLoader::Result::kUrlLoaded});
  url_loader().SetNextLoadUrlResult(
      kWebAppUrl, WebAppUrlLoader::Result::kRedirectedUrlLoaded);
  ExpectGetIcons(data_retriever.get(), /*skip_page_favicons=*/true,
                 IconFetchSource::kFallback);

  InstallResult result = InstallFromSyncAndWait(std::move(data_retriever));

  // Error occurred.
  ASSERT_TRUE(result.install_code_before_fallback.has_value());
  EXPECT_EQ(webapps::InstallResultCode::kInstallURLRedirected,
            result.install_code_before_fallback.value());
  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
            result.install_code);
  EXPECT_EQ(result.installed_app_id, app_id);
  EXPECT_TRUE(registrar().IsInstalled(app_id));

  // Check that the fallback info was installed.
  EXPECT_THAT(registrar().GetAppShortName(app_id), Eq(kFallbackTitle));
  EXPECT_THAT(registrar().GetAppIconInfos(app_id),
              ElementsAre(apps::IconInfo(kFallbackIconUrl, kIconSize)));
  SkColor icon_color = IconManagerReadAppIconPixel(provider()->icon_manager(),
                                                   app_id, kIconSize);
  EXPECT_THAT(icon_color, Eq(kFallbackIconColor));
}

TEST_F(InstallFromSyncTest, FallbackWebAppInstallInfo) {
  const AppId app_id = GenerateAppId(/*manifest_id=*/absl::nullopt, kWebAppUrl);

  auto data_retriever =
      std::make_unique<testing::StrictMock<MockDataRetriever>>();

  command_manager_url_loader().AddPrepareForLoadResults(
      {WebAppUrlLoader::Result::kUrlLoaded});
  url_loader().SetNextLoadUrlResult(kWebAppUrl,
                                    WebAppUrlLoader::Result::kUrlLoaded);
  testing::InSequence sequence;
  EXPECT_CALL(*data_retriever,
              GetWebAppInstallInfo(testing::_, base::test::IsNotNullCallback()))
      .WillOnce(base::test::RunOnceCallback<1>(nullptr));
  ExpectGetIcons(data_retriever.get(), /*skip_page_favicons=*/true,
                 IconFetchSource::kFallback);

  InstallResult result = InstallFromSyncAndWait(std::move(data_retriever));

  // Error occurred.
  ASSERT_TRUE(result.install_code_before_fallback.has_value());
  EXPECT_EQ(webapps::InstallResultCode::kGetWebAppInstallInfoFailed,
            result.install_code_before_fallback.value());
  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
            result.install_code);
  EXPECT_EQ(result.installed_app_id, app_id);
  EXPECT_TRUE(registrar().IsInstalled(app_id));

  // Check that the fallback info was installed.
  EXPECT_THAT(registrar().GetAppShortName(app_id), Eq(kFallbackTitle));
  EXPECT_THAT(registrar().GetAppIconInfos(app_id),
              ElementsAre(apps::IconInfo(kFallbackIconUrl, kIconSize)));
  SkColor icon_color = IconManagerReadAppIconPixel(provider()->icon_manager(),
                                                   app_id, kIconSize);
  EXPECT_THAT(icon_color, Eq(kFallbackIconColor));
}

TEST_F(InstallFromSyncTest, FallbackManifestIdMismatch) {
  const AppId app_id = GenerateAppId(/*manifest_id=*/absl::nullopt, kWebAppUrl);

  auto data_retriever =
      std::make_unique<testing::StrictMock<MockDataRetriever>>();

  command_manager_url_loader().AddPrepareForLoadResults(
      {WebAppUrlLoader::Result::kUrlLoaded});
  url_loader().SetNextLoadUrlResult(kWebAppUrl,
                                    WebAppUrlLoader::Result::kUrlLoaded);
  testing::InSequence sequence;
  EXPECT_CALL(*data_retriever,
              GetWebAppInstallInfo(testing::_, base::test::IsNotNullCallback()))
      .WillOnce(base::test::RunOnceCallback<1>(CreateSiteInstallInfo()));

  auto manifest = CreateManifest(true);
  manifest->id = u"other_path/index.html";

  EXPECT_CALL(*data_retriever, CheckInstallabilityAndRetrieveManifest(
                                   testing::_, true,
                                   base::test::IsNotNullCallback(), testing::_))
      .WillOnce(base::test::RunOnceCallback<2>(
          std::move(manifest), kWebAppManifestUrl, true,
          webapps::InstallableStatusCode::NO_ERROR_DETECTED));

  ExpectGetIcons(data_retriever.get(), /*skip_page_favicons=*/true,
                 IconFetchSource::kDocument);

  InstallResult result = InstallFromSyncAndWait(std::move(data_retriever));

  // Error occurred.
  ASSERT_TRUE(result.install_code_before_fallback.has_value());
  EXPECT_EQ(webapps::InstallResultCode::kExpectedAppIdCheckFailed,
            result.install_code_before_fallback.value());
  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
            result.install_code);
  EXPECT_EQ(result.installed_app_id, app_id);
  EXPECT_TRUE(registrar().IsInstalled(app_id));

  // Check that the fallback info was installed.
  EXPECT_THAT(registrar().GetAppShortName(app_id), Eq(kFallbackTitle));
  EXPECT_THAT(registrar().GetAppIconInfos(app_id),
              ElementsAre(apps::IconInfo(kDocumentIconUrl, kIconSize)));
  SkColor icon_color = IconManagerReadAppIconPixel(provider()->icon_manager(),
                                                   app_id, kIconSize);
  EXPECT_THAT(icon_color, Eq(kDocumentIconColor));
}

TEST_F(InstallFromSyncTest, TwoInstalls) {
  auto data_retriever1 =
      std::make_unique<testing::StrictMock<MockDataRetriever>>();
  auto data_retriever2 =
      std::make_unique<testing::StrictMock<MockDataRetriever>>();

  // The test url loader requires all expectations up front, so set it for both
  // installs.
  command_manager_url_loader().AddPrepareForLoadResults(
      {WebAppUrlLoader::Result::kUrlLoaded,
       WebAppUrlLoader::Result::kUrlLoaded});
  {
    testing::InSequence sequence;

    // Set expectations for kWebAppUrl.
    url_loader().SetNextLoadUrlResult(kWebAppUrl,
                                      WebAppUrlLoader::Result::kUrlLoaded);
    EXPECT_CALL(
        *data_retriever1,
        GetWebAppInstallInfo(testing::_, base::test::IsNotNullCallback()))
        .WillOnce(
            base::test::RunOnceCallback<1>(CreateSiteInstallInfo(kWebAppUrl)));

    EXPECT_CALL(
        *data_retriever1,
        CheckInstallabilityAndRetrieveManifest(
            testing::_, true, base::test::IsNotNullCallback(), testing::_))
        .WillOnce(base::test::RunOnceCallback<2>(
            CreateManifest(true, kWebAppUrl), kWebAppManifestUrl, true,
            webapps::InstallableStatusCode::NO_ERROR_DETECTED));

    ExpectGetIcons(data_retriever1.get(), /*skip_page_favicons=*/true,
                   IconFetchSource::kManifest);

    // Set expectations for kOtherWebAppUrl.
    url_loader().SetNextLoadUrlResult(kOtherWebAppUrl,
                                      WebAppUrlLoader::Result::kUrlLoaded);
    EXPECT_CALL(
        *data_retriever2,
        GetWebAppInstallInfo(testing::_, base::test::IsNotNullCallback()))
        .WillOnce(base::test::RunOnceCallback<1>(
            CreateSiteInstallInfo(kOtherWebAppUrl)));

    EXPECT_CALL(
        *data_retriever2,
        CheckInstallabilityAndRetrieveManifest(
            testing::_, true, base::test::IsNotNullCallback(), testing::_))
        .WillOnce(base::test::RunOnceCallback<2>(
            CreateManifest(true, kOtherWebAppUrl), kWebAppManifestUrl, true,
            webapps::InstallableStatusCode::NO_ERROR_DETECTED));

    ExpectGetIcons(data_retriever2.get(), /*skip_page_favicons=*/true,
                   IconFetchSource::kManifest);
  }

  const AppId app_id1 =
      GenerateAppId(/*manifest_id=*/absl::nullopt, kWebAppUrl);
  const AppId app_id2 =
      GenerateAppId(/*manifest_id=*/absl::nullopt, kOtherWebAppUrl);

  base::RunLoop loop1;
  base::RunLoop loop2;
  enum class Event {
    kApp1Installed,
    kNotifyApp1Installed,
    kNotifyApp1InstalledWithHooks,
    kApp2Installed,
    kNotifyApp2Installed,
    kNotifyApp2InstalledWithHooks,
  };

  std::vector<Event> events;
  std::vector<webapps::InstallResultCode> codes;
  WebAppInstallManagerObserverAdapter observer(&provider()->install_manager());
  observer.SetWebAppInstalledDelegate(
      base::BindLambdaForTesting([&](const AppId& app_id) {
        if (app_id == app_id1) {
          events.push_back(Event::kNotifyApp1Installed);
        } else {
          DCHECK_EQ(app_id, app_id2);
          events.push_back(Event::kNotifyApp2Installed);
        }
      }));
  observer.SetWebAppInstalledWithOsHooksDelegate(
      base::BindLambdaForTesting([&](const AppId& app_id) {
        if (app_id == app_id1) {
          events.push_back(Event::kNotifyApp1InstalledWithHooks);
        } else {
          DCHECK_EQ(app_id, app_id2);
          events.push_back(Event::kNotifyApp2InstalledWithHooks);
        }
      }));

  command_manager().ScheduleCommand(CreateCommand(
      std::move(data_retriever1), CreateParams(app_id1, kWebAppUrl),
      base::BindLambdaForTesting(
          [&](const AppId& id, webapps::InstallResultCode code) {
            events.push_back(Event::kApp1Installed);
            codes.push_back(code);
            loop1.Quit();
          })));
  command_manager().ScheduleCommand(CreateCommand(
      std::move(data_retriever2), CreateParams(app_id2, kOtherWebAppUrl),
      base::BindLambdaForTesting(
          [&](const AppId& id, webapps::InstallResultCode code) {
            events.push_back(Event::kApp2Installed);
            codes.push_back(code);
            loop2.Quit();
          })));
  loop1.Run();
  EXPECT_TRUE(command_manager().has_web_contents_for_testing());
  loop2.Run();
  EXPECT_FALSE(command_manager().has_web_contents_for_testing());
  EXPECT_TRUE(registrar().IsInstalled(app_id1));
  EXPECT_TRUE(registrar().IsInstalled(app_id2));
  std::vector<Event> expected;
  if (AreAppsLocallyInstalledBySync()) {
    expected = {
        Event::kNotifyApp1Installed,
        Event::kApp1Installed,
        Event::kNotifyApp1InstalledWithHooks,
        Event::kNotifyApp2Installed,
        Event::kApp2Installed,
        Event::kNotifyApp2InstalledWithHooks,
    };
  } else {
    expected = {
        Event::kNotifyApp1Installed,
        Event::kApp1Installed,
        Event::kNotifyApp2Installed,
        Event::kApp2Installed,
    };
  }
  EXPECT_THAT(events, ElementsAreArray(expected));
  EXPECT_THAT(codes,
              ElementsAre(webapps::InstallResultCode::kSuccessNewInstall,
                          webapps::InstallResultCode::kSuccessNewInstall));
}

TEST_F(InstallFromSyncTest, Shutdown) {
  auto data_retriever =
      std::make_unique<testing::StrictMock<MockDataRetriever>>();

  command_manager_url_loader().AddPrepareForLoadResults(
      {WebAppUrlLoader::Result::kUrlLoaded});
  url_loader().SetNextLoadUrlResult(kWebAppUrl,
                                    WebAppUrlLoader::Result::kUrlLoaded);

  base::RunLoop loop;
  WebAppDataRetriever::GetWebAppInstallInfoCallback callback;
  EXPECT_CALL(*data_retriever,
              GetWebAppInstallInfo(testing::_, base::test::IsNotNullCallback()))
      .WillOnce(
          [&](content::WebContents* web_contents,
              WebAppDataRetriever::GetWebAppInstallInfoCallback arg_callback) {
            callback = std::move(arg_callback);
            loop.Quit();
          });

  const AppId app_id = GenerateAppId(/*manifest_id=*/absl::nullopt, kWebAppUrl);
  bool result_populated = false;
  InstallResult result;
  auto command =
      CreateCommand(std::move(data_retriever), CreateParams(app_id, kWebAppUrl),
                    base::BindLambdaForTesting(
                        [&](const AppId& id, webapps::InstallResultCode code) {
                          result_populated = true;
                          result.installed_app_id = id;
                          result.install_code = code;
                        }));
  command_manager().ScheduleCommand(std::move(command));
  loop.Run();
  command_manager().Shutdown();

  // Running this should do nothing.
  ASSERT_FALSE(callback.is_null());
  std::move(callback).Run(CreateSiteInstallInfo());

  ASSERT_TRUE(result_populated);
  EXPECT_EQ(webapps::InstallResultCode::kCancelledOnWebAppProviderShuttingDown,
            result.install_code);
  EXPECT_EQ(result.installed_app_id, app_id);
  EXPECT_FALSE(registrar().IsInstalled(app_id));
}

TEST_F(InstallFromSyncTest, ShutdownDoesNotCrash) {
  class CustomInstallableManager : public webapps::InstallableManager {
   public:
    CustomInstallableManager(content::WebContents* web_contents,
                             WebAppCommandManager* command_manager)
        : webapps::InstallableManager(web_contents),
          command_manager_(command_manager) {}
    ~CustomInstallableManager() override = default;

   private:
    // webapps::InstallableManager:
    void GetData(const webapps::InstallableParams& params,
                 webapps::InstallableCallback callback) override {
      command_manager_->Shutdown();
    }

    const raw_ptr<WebAppCommandManager> command_manager_;
  };

  class CustomWebAppDataRetriever : public WebAppDataRetriever {
   public:
    explicit CustomWebAppDataRetriever(WebAppCommandManager* command_manager)
        : command_manager_(command_manager) {}
    ~CustomWebAppDataRetriever() override = default;

   private:
    void GetWebAppInstallInfo(content::WebContents* web_contents,
                              GetWebAppInstallInfoCallback callback) override {
      web_contents->SetUserData(content::WebContentsUserData<
                                    webapps::InstallableManager>::UserDataKey(),
                                std::make_unique<CustomInstallableManager>(
                                    web_contents, command_manager_));

      std::move(callback).Run(std::make_unique<WebAppInstallInfo>());
    }

    const raw_ptr<WebAppCommandManager> command_manager_;
  };

  const AppId app_id = GenerateAppId(/*manifest_id=*/absl::nullopt, kWebAppUrl);
  command_manager_url_loader().AddPrepareForLoadResults(
      {WebAppUrlLoader::Result::kUrlLoaded});
  url_loader().SetNextLoadUrlResult(kWebAppUrl,
                                    WebAppUrlLoader::Result::kUrlLoaded);

  base::RunLoop loop;
  auto data_retriever =
      std::make_unique<CustomWebAppDataRetriever>(&command_manager());
  auto command = CreateCommand(
      std::move(data_retriever), CreateParams(app_id, kWebAppUrl),
      base::BindLambdaForTesting([&](const AppId& id,
                                     webapps::InstallResultCode code) {
        EXPECT_EQ(
            code,
            webapps::InstallResultCode::kCancelledOnWebAppProviderShuttingDown);
        loop.Quit();
      }));
  command_manager().ScheduleCommand(std::move(command));
  loop.Run();
}

TEST_F(InstallFromSyncTest, SyncUninstall) {
  auto data_retriever =
      std::make_unique<testing::StrictMock<MockDataRetriever>>();

  command_manager_url_loader().AddPrepareForLoadResults(
      {WebAppUrlLoader::Result::kUrlLoaded});
  url_loader().SetNextLoadUrlResult(kWebAppUrl,
                                    WebAppUrlLoader::Result::kUrlLoaded);

  base::RunLoop loop;
  WebAppDataRetriever::GetWebAppInstallInfoCallback callback;
  EXPECT_CALL(*data_retriever,
              GetWebAppInstallInfo(testing::_, base::test::IsNotNullCallback()))
      .WillOnce(
          [&](content::WebContents* web_contents,
              WebAppDataRetriever::GetWebAppInstallInfoCallback arg_callback) {
            callback = std::move(arg_callback);
            loop.Quit();
          });

  const AppId app_id = GenerateAppId(/*manifest_id=*/absl::nullopt, kWebAppUrl);
  bool result_populated = false;
  InstallResult result;
  auto command =
      CreateCommand(std::move(data_retriever), CreateParams(app_id, kWebAppUrl),
                    base::BindLambdaForTesting(
                        [&](const AppId& id, webapps::InstallResultCode code) {
                          result_populated = true;
                          result.installed_app_id = id;
                          result.install_code = code;
                        }));
  command_manager().ScheduleCommand(std::move(command));
  loop.Run();
  command_manager().NotifySyncSourceRemoved({app_id});

  // Running this should do nothing.
  ASSERT_FALSE(callback.is_null());
  std::move(callback).Run(CreateSiteInstallInfo());

  ASSERT_TRUE(result_populated);
  EXPECT_EQ(webapps::InstallResultCode::kHaltedBySyncUninstall,
            result.install_code);
  EXPECT_EQ(result.installed_app_id, app_id);
  EXPECT_FALSE(registrar().IsInstalled(app_id));
}

}  // namespace
}  // namespace web_app
