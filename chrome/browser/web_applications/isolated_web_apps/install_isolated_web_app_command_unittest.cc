// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/install_isolated_web_app_command.h"

#include <map>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/functional/overloaded.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_builder.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_install_command_helper.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_location.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_response_reader_factory.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_trust_checker.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_validator.h"
#include "chrome/browser/web_applications/isolated_web_apps/pending_install_info.h"
#include "chrome/browser/web_applications/locks/lock.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/fake_web_contents_manager.h"
#include "chrome/browser/web_applications/test/mock_data_retriever.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_contents/web_app_url_loader.h"
#include "chrome/common/chrome_features.h"
#include "components/web_package/signed_web_bundles/ed25519_public_key.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "net/http/http_status_code.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-shared.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "url/gurl.h"

namespace web_app {
namespace {

using ::base::BucketsAre;
using ::base::test::ErrorIs;
using ::base::test::HasValue;
using ::base::test::IsNotNullCallback;
using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::AllOf;
using ::testing::DoAll;
using ::testing::Eq;
using ::testing::Field;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::IsFalse;
using ::testing::IsNull;
using ::testing::IsTrue;
using ::testing::NiceMock;
using ::testing::Not;
using ::testing::NotNull;
using ::testing::Optional;
using ::testing::Pair;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::ResultOf;
using ::testing::UnorderedElementsAre;
using ::testing::VariantWith;
using ::testing::WithArg;

constexpr base::StringPiece kManifestPath =
    "/.well-known/_generated_install_page.html";
constexpr base::StringPiece kIconPath = "/icon.png";

IsolatedWebAppUrlInfo CreateRandomIsolatedWebAppUrlInfo() {
  web_package::SignedWebBundleId signed_web_bundle_id =
      web_package::SignedWebBundleId::CreateRandomForDevelopment();
  return IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
      signed_web_bundle_id);
}

IsolatedWebAppUrlInfo CreateEd25519IsolatedWebAppUrlInfo() {
  web_package::SignedWebBundleId signed_web_bundle_id =
      web_package::SignedWebBundleId::CreateForEd25519PublicKey(
          web_package::Ed25519PublicKey::Create(
              base::make_span(kTestPublicKey)));
  return IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
      signed_web_bundle_id);
}

IsolatedWebAppLocation CreateDevProxyLocation(
    base::StringPiece dev_mode_proxy_url = "http://default-proxy-url.org/") {
  return DevModeProxy{.proxy_url =
                          url::Origin::Create(GURL(dev_mode_proxy_url))};
}

blink::mojom::ManifestPtr CreateDefaultManifest(const GURL& application_url) {
  auto manifest = blink::mojom::Manifest::New();
  manifest->id = application_url.DeprecatedGetOriginAsURL();
  manifest->scope = application_url.Resolve("/");
  manifest->start_url = application_url.Resolve("/testing-start-url.html");
  manifest->display = DisplayMode::kStandalone;
  manifest->short_name = u"test short manifest name";
  manifest->version = u"1.0.0";

  blink::Manifest::ImageResource icon;
  icon.src = application_url.Resolve(kIconPath);
  icon.purpose = {blink::mojom::ManifestImageResource_Purpose::ANY};
  icon.type = u"image/png";
  icon.sizes = {gfx::Size(256, 256)};
  manifest->icons.push_back(icon);

  return manifest;
}

GURL CreateDefaultManifestURL(const GURL& application_url) {
  return application_url.Resolve("/manifest.webmanifest");
}

class InstallIsolatedWebAppCommandTest : public WebAppTest {
 public:
  InstallIsolatedWebAppCommandTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kIsolatedWebApps, features::kIsolatedWebAppDevMode}, {});
  }

  void SetUp() override {
    WebAppTest::SetUp();
    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  WebAppRegistrar& web_app_registrar() {
    return fake_provider().registrar_unsafe();
  }

  WebAppIconManager& web_app_icon_manager() {
    return fake_provider().icon_manager();
  }

  FakeWebContentsManager& web_contents_manager() {
    return static_cast<FakeWebContentsManager&>(
        fake_provider().web_contents_manager());
  }

  std::pair<FakeWebContentsManager::FakePageState&,
            FakeWebContentsManager::FakeIconState&>
  SetUpPageAndIconStates(const IsolatedWebAppUrlInfo& url_info) {
    GURL application_url = url_info.origin().GetURL();
    auto& page_state = web_contents_manager().GetOrCreatePageState(
        application_url.Resolve(kManifestPath));
    page_state.url_load_result = WebAppUrlLoader::Result::kUrlLoaded;
    page_state.error_code = webapps::InstallableStatusCode::NO_ERROR_DETECTED;

    page_state.manifest_url = CreateDefaultManifestURL(application_url);
    page_state.valid_manifest_for_web_app = true;
    page_state.opt_manifest = CreateDefaultManifest(application_url);

    auto& icon_state = web_contents_manager().GetOrCreateIconState(
        application_url.Resolve(kIconPath));
    icon_state.bitmaps = {web_app::CreateSquareIcon(32, SK_ColorRED)};

    return {page_state, icon_state};
  }

  struct Parameters {
    IsolatedWebAppUrlInfo url_info;
    absl::optional<IsolatedWebAppLocation> location;
    absl::optional<base::Version> expected_version;
  };

  base::expected<InstallIsolatedWebAppCommandSuccess,
                 InstallIsolatedWebAppCommandError>
  ExecuteCommand(Parameters parameters) {
    base::test::TestFuture<base::expected<InstallIsolatedWebAppCommandSuccess,
                                          InstallIsolatedWebAppCommandError>>
        test_future;
    fake_provider().scheduler().InstallIsolatedWebApp(
        parameters.url_info,
        parameters.location.value_or(CreateDevProxyLocation()),
        parameters.expected_version, /* optional_keep_alive=*/nullptr,
        /*optional_profile_keep_alive=*/nullptr, test_future.GetCallback());
    return test_future.Take();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(InstallIsolatedWebAppCommandTest, PropagateErrorWhenURLLoaderFails) {
  IsolatedWebAppUrlInfo url_info = CreateRandomIsolatedWebAppUrlInfo();
  auto [page_state, icon_state] = SetUpPageAndIconStates(url_info);
  page_state.url_load_result = WebAppUrlLoader::Result::kFailedErrorPageLoaded;

  EXPECT_THAT(ExecuteCommand(Parameters{.url_info = url_info}),
              ErrorIs(Field(&InstallIsolatedWebAppCommandError::message,
                            HasSubstr("Error during URL loading: "))));
}

TEST_F(InstallIsolatedWebAppCommandTest,
       PropagateErrorWhenURLLoaderFailsWithDestroyedWebContentsError) {
  IsolatedWebAppUrlInfo url_info = CreateRandomIsolatedWebAppUrlInfo();
  auto [page_state, icon_state] = SetUpPageAndIconStates(url_info);
  page_state.url_load_result =
      WebAppUrlLoader::Result::kFailedWebContentsDestroyed;

  EXPECT_THAT(
      ExecuteCommand(Parameters{.url_info = url_info}),
      ErrorIs(Field(
          &InstallIsolatedWebAppCommandError::message,
          HasSubstr("Error during URL loading: FailedWebContentsDestroyed"))));
}

TEST_F(InstallIsolatedWebAppCommandTest,
       InstallationSucceedesWhenFinalizerReturnSuccessNewInstall) {
  IsolatedWebAppUrlInfo url_info = CreateRandomIsolatedWebAppUrlInfo();
  SetUpPageAndIconStates(url_info);

  EXPECT_THAT(ExecuteCommand(Parameters{.url_info = url_info}), HasValue());
}

TEST_F(InstallIsolatedWebAppCommandTest,
       InstallationFailsWhenDevModeIsDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(features::kIsolatedWebAppDevMode);

  IsolatedWebAppUrlInfo url_info = CreateRandomIsolatedWebAppUrlInfo();
  SetUpPageAndIconStates(url_info);
  EXPECT_THAT(
      ExecuteCommand(Parameters{.url_info = url_info}),
      ErrorIs(
          Field(&InstallIsolatedWebAppCommandError::message,
                HasSubstr("Isolated Web App Developer Mode is not enabled"))));
}

TEST_F(InstallIsolatedWebAppCommandTest,
       InstallationFinalizedWithIsolatedWebAppDevInstallInstallSource) {
  IsolatedWebAppUrlInfo url_info = CreateRandomIsolatedWebAppUrlInfo();
  SetUpPageAndIconStates(url_info);

  EXPECT_THAT(ExecuteCommand(Parameters{.url_info = url_info}), HasValue());

  using InstallSource = webapps::WebappInstallSource;

  const WebApp* web_app = web_app_registrar().GetAppById(url_info.app_id());
  ASSERT_THAT(web_app, NotNull());

  EXPECT_TRUE(web_app->GetSources().Has(WebAppManagement::kCommandLine));

  EXPECT_THAT(web_app->latest_install_source(),
              Optional(Eq(InstallSource::ISOLATED_APP_DEV_INSTALL)));
}

TEST_F(InstallIsolatedWebAppCommandTest,
       InstallationFailsWhenAppIsNotInstallable) {
  IsolatedWebAppUrlInfo url_info = CreateRandomIsolatedWebAppUrlInfo();
  auto [page_state, icon_state] = SetUpPageAndIconStates(url_info);
  page_state.manifest_url = GURL("http://test-url-example.com/manifest.json");
  page_state.opt_manifest = blink::mojom::Manifest::New();
  page_state.error_code = webapps::InstallableStatusCode::NO_MANIFEST;

  EXPECT_THAT(ExecuteCommand(Parameters{.url_info = url_info}),
              ErrorIs(Field(&InstallIsolatedWebAppCommandError::message,
                            HasSubstr("App is not installable"))));
}

TEST_F(InstallIsolatedWebAppCommandTest, PendingUpdateInfoIsEmpty) {
  IsolatedWebAppUrlInfo url_info = CreateRandomIsolatedWebAppUrlInfo();
  SetUpPageAndIconStates(url_info);

  EXPECT_THAT(ExecuteCommand(Parameters{.url_info = url_info}), HasValue());
  EXPECT_THAT(web_app_registrar().GetAppById(url_info.app_id()),
              Pointee(Property(
                  &WebApp::isolation_data,
                  Optional(Property(&WebApp::IsolationData::pending_update_info,
                                    Eq(absl::nullopt))))));
}

TEST_F(InstallIsolatedWebAppCommandTest,
       InstallationFailsWhenAppVersionDoesNotMatchExpectedVersion) {
  IsolatedWebAppUrlInfo url_info = CreateRandomIsolatedWebAppUrlInfo();
  SetUpPageAndIconStates(url_info);

  EXPECT_THAT(
      ExecuteCommand(Parameters{.url_info = url_info,
                                .expected_version = base::Version("99.99.99")}),
      ErrorIs(Field(
          &InstallIsolatedWebAppCommandError::message,
          HasSubstr("does not match the version provided in the manifest"))));
}

TEST_F(InstallIsolatedWebAppCommandTest, CommandLocksOnAppId) {
  IsolatedWebAppUrlInfo url_info = CreateRandomIsolatedWebAppUrlInfo();
  SetUpPageAndIconStates(url_info);

  base::test::TestFuture<base::expected<InstallIsolatedWebAppCommandSuccess,
                                        InstallIsolatedWebAppCommandError>>
      test_future;
  auto command_helper = std::make_unique<IsolatedWebAppInstallCommandHelper>(
      url_info, web_contents_manager().CreateDataRetriever(),
      IsolatedWebAppInstallCommandHelper::CreateDefaultResponseReaderFactory(
          *profile()->GetPrefs()));

  auto command = std::make_unique<InstallIsolatedWebAppCommand>(
      url_info, CreateDevProxyLocation(), /*expected_version=*/absl::nullopt,
      content::WebContents::Create(
          content::WebContents::CreateParams(profile())),
      /*optional_keep_alive=*/nullptr,
      /*optional_profile_keep_alive=*/nullptr, test_future.GetCallback(),
      std::move(command_helper));

  EXPECT_THAT(
      command->lock_description(),
      AllOf(Property(&LockDescription::type, Eq(LockDescription::Type::kApp)),
            Property(&LockDescription::app_ids,
                     UnorderedElementsAre(url_info.app_id()))));
}

TEST_F(InstallIsolatedWebAppCommandTest, LocationSentToFinalizer) {
  IsolatedWebAppUrlInfo url_info = CreateRandomIsolatedWebAppUrlInfo();
  SetUpPageAndIconStates(url_info);

  EXPECT_THAT(
      ExecuteCommand(Parameters{
          .url_info = url_info,
          .location = DevModeProxy{.proxy_url = url::Origin::Create(GURL(
                                       "http://some-testing-proxy-url.com/"))},
      }),
      HasValue());

  EXPECT_THAT(web_app_registrar().GetAppById(url_info.app_id()),
              Pointee(AllOf(Property(
                  "isolation_data", &WebApp::isolation_data,
                  Optional(Field(
                      "location", &WebApp::IsolationData::location,
                      VariantWith<DevModeProxy>(Field(
                          "proxy_url", &DevModeProxy::proxy_url,
                          Eq(url::Origin::Create(GURL(
                              "http://some-testing-proxy-url.com/")))))))))));
}

TEST_F(InstallIsolatedWebAppCommandTest,
       CreatesStorageParitionDuringInstallation) {
  IsolatedWebAppUrlInfo url_info = CreateRandomIsolatedWebAppUrlInfo();
  SetUpPageAndIconStates(url_info);

  EXPECT_THAT(ExecuteCommand(Parameters{.url_info = url_info}), HasValue());

  EXPECT_THAT(profile()->GetStoragePartition(
                  url_info.storage_partition_config(profile()),
                  /*can_create=*/false),
              NotNull());
}

TEST_F(InstallIsolatedWebAppCommandTest, UsersCanDeleteIsolatedApp) {
  IsolatedWebAppUrlInfo url_info = CreateRandomIsolatedWebAppUrlInfo();
  SetUpPageAndIconStates(url_info);

  EXPECT_THAT(ExecuteCommand(Parameters{.url_info = url_info}), HasValue());

  EXPECT_THAT(web_app_registrar().GetAppById(url_info.app_id()),
              Pointee(Property("CanUserUninstallWebApp",
                               &WebApp::CanUserUninstallWebApp, IsTrue())));
}

TEST_F(InstallIsolatedWebAppCommandTest,
       CreatesStoragePartitionBeforeUrlLoading) {
  IsolatedWebAppUrlInfo url_info = CreateRandomIsolatedWebAppUrlInfo();
  SetUpPageAndIconStates(url_info);

  content::StoragePartition* storage_partition_during_url_loading = nullptr;
  web_contents_manager().TrackLoadUrlCalls(base::BindLambdaForTesting(
      [&](content::NavigationController::LoadURLParams& load_url_params,
          content::WebContents* unused_web_contents,
          WebAppUrlLoader::UrlComparison unused_url_comparison) {
        EXPECT_THAT(load_url_params.url,
                    Eq(url_info.origin().GetURL().Resolve(kManifestPath)));
        storage_partition_during_url_loading = profile()->GetStoragePartition(
            url_info.storage_partition_config(profile()),
            /*can_create=*/false);
      }));

  EXPECT_THAT(profile()->GetStoragePartition(
                  url_info.storage_partition_config(profile()),
                  /*can_create=*/false),
              IsNull());

  EXPECT_THAT(ExecuteCommand({.url_info = url_info}), HasValue());

  EXPECT_THAT(storage_partition_during_url_loading, NotNull());
}

using InstallIsolatedWebAppCommandManifestTest =
    InstallIsolatedWebAppCommandTest;

TEST_F(InstallIsolatedWebAppCommandManifestTest,
       PassesManifestIdToFinalizerWhenManifestIdIsEmpty) {
  IsolatedWebAppUrlInfo url_info = CreateRandomIsolatedWebAppUrlInfo();
  SetUpPageAndIconStates(url_info);

  EXPECT_THAT(ExecuteCommand(Parameters{.url_info = url_info}), HasValue());

  EXPECT_THAT(web_app_registrar().GetAppById(url_info.app_id()), NotNull());
}

TEST_F(InstallIsolatedWebAppCommandManifestTest,
       FailsWhenManifestIdIsNotEmpty) {
  IsolatedWebAppUrlInfo url_info = CreateRandomIsolatedWebAppUrlInfo();
  auto [page_state, icon_state] = SetUpPageAndIconStates(url_info);
  page_state.opt_manifest->id =
      url_info.origin().GetURL().Resolve("/test-manifest-id");

  EXPECT_THAT(ExecuteCommand(Parameters{.url_info = url_info}),
              ErrorIs(Field(&InstallIsolatedWebAppCommandError::message,
                            HasSubstr(R"(Manifest `id` must be "/")"))));

  EXPECT_THAT(web_app_registrar().GetAppById(url_info.app_id()), IsNull());
}

TEST_F(InstallIsolatedWebAppCommandManifestTest,
       InstalledApplicationScopeIsResolvedToRootWhenManifestScopeIsSlash) {
  IsolatedWebAppUrlInfo url_info = CreateRandomIsolatedWebAppUrlInfo();
  auto [page_state, icon_state] = SetUpPageAndIconStates(url_info);
  page_state.opt_manifest->scope = url_info.origin().GetURL().Resolve("/");

  EXPECT_THAT(ExecuteCommand(Parameters{.url_info = url_info}), HasValue());

  EXPECT_THAT(web_app_registrar().GetAppById(url_info.app_id()),
              Pointee(Property("scope", &WebApp::scope,
                               Eq(url_info.origin().GetURL()))));
}

TEST_F(InstallIsolatedWebAppCommandManifestTest,
       PassesManifestNameAsUntranslatedName) {
  IsolatedWebAppUrlInfo url_info = CreateRandomIsolatedWebAppUrlInfo();
  auto [page_state, icon_state] = SetUpPageAndIconStates(url_info);
  page_state.opt_manifest->name = u"test application name";

  EXPECT_THAT(ExecuteCommand(Parameters{.url_info = url_info}), HasValue());

  EXPECT_THAT(web_app_registrar().GetAppById(url_info.app_id()),
              Pointee(Property("untranslated_name", &WebApp::untranslated_name,
                               Eq("test application name"))));
}

TEST_F(InstallIsolatedWebAppCommandManifestTest,
       UseShortNameAsUntranslatedNameWhenNameIsNotPresent) {
  IsolatedWebAppUrlInfo url_info = CreateRandomIsolatedWebAppUrlInfo();
  auto [page_state, icon_state] = SetUpPageAndIconStates(url_info);
  page_state.opt_manifest->name = absl::nullopt;
  page_state.opt_manifest->short_name = u"test short name";

  EXPECT_THAT(ExecuteCommand(Parameters{.url_info = url_info}), HasValue());

  EXPECT_THAT(web_app_registrar().GetAppById(url_info.app_id()),
              Pointee(Property("untranslated_name", &WebApp::untranslated_name,
                               Eq("test short name"))));
}

TEST_F(InstallIsolatedWebAppCommandManifestTest,
       UseShortNameAsTitleWhenManifestNameIsEmpty) {
  IsolatedWebAppUrlInfo url_info = CreateRandomIsolatedWebAppUrlInfo();
  auto [page_state, icon_state] = SetUpPageAndIconStates(url_info);
  page_state.opt_manifest->name = u"";
  page_state.opt_manifest->short_name = u"other test short name";

  EXPECT_THAT(ExecuteCommand(Parameters{.url_info = url_info}), HasValue());

  EXPECT_THAT(web_app_registrar().GetAppById(url_info.app_id()),
              Pointee(Property("untranslated_name", &WebApp::untranslated_name,
                               Eq("other test short name"))));
}

using InstallIsolatedWebAppCommandManifestIconsTest =
    InstallIsolatedWebAppCommandManifestTest;

TEST_F(InstallIsolatedWebAppCommandManifestIconsTest,
       ManifestIconIsDownloaded) {
  IsolatedWebAppUrlInfo url_info = CreateRandomIsolatedWebAppUrlInfo();
  SetUpPageAndIconStates(url_info);

  EXPECT_THAT(ExecuteCommand(Parameters{.url_info = url_info}), HasValue());

  base::test::TestFuture<std::map<SquareSizePx, SkBitmap>> test_future;
  web_app_icon_manager().ReadIconAndResize(url_info.app_id(), IconPurpose::ANY,
                                           SquareSizePx{1},
                                           test_future.GetCallback());

  std::map<SquareSizePx, SkBitmap> icon_bitmaps = test_future.Get();

  EXPECT_THAT(icon_bitmaps,
              UnorderedElementsAre(Pair(_, ResultOf(
                                               "bitmap.color.at.0.0",
                                               [](const SkBitmap& bitmap) {
                                                 return bitmap.getColor(0, 0);
                                               },
                                               Eq(SK_ColorRED)))));

  EXPECT_THAT(web_app_registrar().GetAppById(url_info.app_id()),
              Pointee(Property("manifest_icons", &WebApp::manifest_icons,
                               UnorderedElementsAre(_))));
}

TEST_F(InstallIsolatedWebAppCommandManifestIconsTest,
       InstallationFailsWhenIconDownloadingFails) {
  IsolatedWebAppUrlInfo url_info = CreateRandomIsolatedWebAppUrlInfo();
  auto [page_state, icon_state] = SetUpPageAndIconStates(url_info);
  icon_state.http_status_code = net::HttpStatusCode::HTTP_NOT_FOUND;
  icon_state.bitmaps = {};

  EXPECT_THAT(
      ExecuteCommand(Parameters{.url_info = url_info}),
      ErrorIs(Field(
          &InstallIsolatedWebAppCommandError::message,
          HasSubstr("Error during icon downloading: AbortedDueToFailure"))));
}

using InstallIsolatedWebAppCommandMetricsTest =
    InstallIsolatedWebAppCommandTest;

TEST_F(InstallIsolatedWebAppCommandMetricsTest,
       ReportSuccessWhenFinishedSuccessfully) {
  IsolatedWebAppUrlInfo url_info = CreateRandomIsolatedWebAppUrlInfo();
  SetUpPageAndIconStates(url_info);

  base::HistogramTester histogram_tester;

  EXPECT_THAT(ExecuteCommand(Parameters{.url_info = url_info}), HasValue());

  EXPECT_THAT(histogram_tester.GetAllSamples("WebApp.Install.Result"),
              BucketsAre(base::Bucket(true, 1)));
}

TEST_F(InstallIsolatedWebAppCommandMetricsTest, ReportErrorWhenUrlLoaderFails) {
  IsolatedWebAppUrlInfo url_info = CreateRandomIsolatedWebAppUrlInfo();
  auto [page_state, icon_state] = SetUpPageAndIconStates(url_info);
  page_state.url_load_result = WebAppUrlLoader::Result::kFailedErrorPageLoaded;

  base::HistogramTester histogram_tester;

  EXPECT_THAT(ExecuteCommand(Parameters{.url_info = url_info}),
              Not(HasValue()));

  EXPECT_THAT(histogram_tester.GetAllSamples("WebApp.Install.Result"),
              BucketsAre(base::Bucket(false, 1)));
}

TEST_F(InstallIsolatedWebAppCommandMetricsTest,
       ReportFailureWhenAppIsNotInstallable) {
  IsolatedWebAppUrlInfo url_info = CreateRandomIsolatedWebAppUrlInfo();
  auto [page_state, icon_state] = SetUpPageAndIconStates(url_info);
  page_state.manifest_url = GURL{"http://test-url-example.com/manifest.json"};
  page_state.opt_manifest = blink::mojom::Manifest::New();
  page_state.error_code = webapps::InstallableStatusCode::NO_MANIFEST;

  base::HistogramTester histogram_tester;

  EXPECT_THAT(ExecuteCommand(Parameters{.url_info = url_info}),
              Not(HasValue()));

  EXPECT_THAT(histogram_tester.GetAllSamples("WebApp.Install.Result"),
              BucketsAre(base::Bucket(false, 1)));
}

TEST_F(InstallIsolatedWebAppCommandMetricsTest,
       ReportFailureWhenManifestIsNull) {
  IsolatedWebAppUrlInfo url_info = CreateRandomIsolatedWebAppUrlInfo();
  auto [page_state, icon_state] = SetUpPageAndIconStates(url_info);
  page_state.opt_manifest = nullptr;
  page_state.error_code = webapps::InstallableStatusCode::NO_MANIFEST;

  base::HistogramTester histogram_tester;

  EXPECT_THAT(ExecuteCommand(Parameters{.url_info = url_info}),
              Not(HasValue()));

  EXPECT_THAT(histogram_tester.GetAllSamples("WebApp.Install.Result"),
              BucketsAre(base::Bucket(false, 1)));
}

TEST_F(InstallIsolatedWebAppCommandMetricsTest,
       ReportFailureWhenManifestIdIsNotEmpty) {
  IsolatedWebAppUrlInfo url_info = CreateRandomIsolatedWebAppUrlInfo();
  auto [page_state, icon_state] = SetUpPageAndIconStates(url_info);
  page_state.opt_manifest->id =
      url_info.origin().GetURL().Resolve("/test manifest id");

  base::HistogramTester histogram_tester;

  EXPECT_THAT(ExecuteCommand(Parameters{.url_info = url_info}),
              Not(HasValue()));
  EXPECT_THAT(histogram_tester.GetAllSamples("WebApp.Install.Result"),
              BucketsAre(base::Bucket(false, 1)));
}

struct BundleTestInfo {
  bool has_error;
  bool not_trusted;
  bool want_success;
};

class InstallIsolatedWebAppCommandBundleTest
    : public InstallIsolatedWebAppCommandTest,
      public ::testing::WithParamInterface<std::tuple<bool, BundleTestInfo>> {
 public:
  InstallIsolatedWebAppCommandBundleTest()
      : is_dev_mode_(std::get<0>(GetParam())),
        bundle_info_(std::get<1>(GetParam())) {
    if (is_dev_mode_) {
      if (bundle_info_.not_trusted) {
        // For a dev mode bundle to not be trusted, disable developer mode.
        scoped_feature_list_.InitAndDisableFeature(
            features::kIsolatedWebAppDevMode);
      } else {
        // Otherwise enable developer mode.
        scoped_feature_list_.InitAndEnableFeature(
            features::kIsolatedWebAppDevMode);
      }
    } else {  // !is_dev_mode_
      // Disable developer mode so that the bundle is not automatically trusted.
      scoped_feature_list_.InitAndDisableFeature(
          features::kIsolatedWebAppDevMode);
      if (bundle_info_.not_trusted) {
        SetTrustedWebBundleIdsForTesting({});
      } else {
        SetTrustedWebBundleIdsForTesting({url_info_.web_bundle_id()});
      }
    }
  }

  void SetUp() override {
    base::FilePath bundle_path;
    ASSERT_NO_FATAL_FAILURE(WriteWebBundle(bundle_path));
    if (is_dev_mode_) {
      location_ = IsolatedWebAppLocation(DevModeBundle{
          .path = bundle_path,
      });
    } else {
      location_ = IsolatedWebAppLocation(InstalledBundle{
          .path = bundle_path,
      });
    }

    InstallIsolatedWebAppCommandTest::SetUp();
  }

  void WriteWebBundle(base::FilePath& bundle_path) {
    ASSERT_THAT(temp_dir_.CreateUniqueTempDir(), IsTrue());
    bundle_path = temp_dir_.GetPath().AppendASCII("bundle.swbn");

    TestSignedWebBundleBuilder::BuildOptions build_options;
    if (bundle_info_.has_error) {
      build_options.SetErrorsForTesting(
          {web_package::WebBundleSigner::ErrorForTesting::
               kInvalidIntegrityBlockStructure});
    }

    TestSignedWebBundleBuilder builder;
    TestSignedWebBundle bundle = builder.BuildDefault(std::move(build_options));
    ASSERT_THAT(base::WriteFile(bundle_path, bundle.data), IsTrue());
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  base::ScopedTempDir temp_dir_;
  bool is_dev_mode_;
  BundleTestInfo bundle_info_;
  IsolatedWebAppLocation location_;
  IsolatedWebAppUrlInfo url_info_ = CreateEd25519IsolatedWebAppUrlInfo();
};

TEST_P(InstallIsolatedWebAppCommandBundleTest, InstallsWhenThereIsNoError) {
  SetUpPageAndIconStates(url_info_);

  auto result = ExecuteCommand(Parameters{
      .url_info = url_info_,
      .location = location_,
  });

  const base::FilePath iwa_root_dir = profile()->GetPath().Append(kIwaDirName);

  if (bundle_info_.want_success) {
    EXPECT_THAT(result, HasValue());
    absl::visit(base::Overloaded{
                    [&iwa_root_dir](const InstalledBundle& installed_bundle) {
                      EXPECT_TRUE(DirectoryExists(iwa_root_dir));
                      EXPECT_TRUE(PathExists(installed_bundle.path));
                    },
                    [&iwa_root_dir](const DevModeBundle&) {
                      EXPECT_FALSE(DirectoryExists(iwa_root_dir));
                    },
                    [](const DevModeProxy&) {}},
                result->location);
  } else {
    EXPECT_THAT(result, Not(HasValue()));
    // Wait till IWA directory is removed.
    task_environment()->RunUntilIdle();
    absl::visit(
        base::Overloaded{[&iwa_root_dir](const InstalledBundle&) {
                           EXPECT_TRUE(DirectoryExists(iwa_root_dir));
                           EXPECT_TRUE(base::IsDirectoryEmpty(iwa_root_dir));
                         },
                         [&iwa_root_dir](const DevModeBundle&) {
                           EXPECT_FALSE(DirectoryExists(iwa_root_dir));
                         },
                         [](const DevModeProxy&) {}},
        location_);
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    InstallIsolatedWebAppCommandBundleTest,
    ::testing::Combine(::testing::Bool(),  // is_dev_mode
                       ::testing::Values(BundleTestInfo{.has_error = false,
                                                        .not_trusted = false,
                                                        .want_success = true},
                                         BundleTestInfo{.has_error = true,
                                                        .not_trusted = false,
                                                        .want_success = false},
                                         BundleTestInfo{
                                             .has_error = false,
                                             .not_trusted = true,
                                             .want_success = false})),
    [](::testing::TestParamInfo<std::tuple<bool, BundleTestInfo>> param_info) {
      return base::StrCat(
          {std::get<0>(param_info.param) ? "DevModeBundle" : "InstalledBundle",
           "_",
           std::get<1>(param_info.param).has_error ? "has_error" : "no_error",
           "_",
           std::get<1>(param_info.param).not_trusted ? "not_trusted"
                                                     : "is_trusted"});
    });

}  // namespace
}  // namespace web_app
