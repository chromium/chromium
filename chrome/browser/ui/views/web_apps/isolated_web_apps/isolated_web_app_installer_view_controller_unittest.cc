// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/isolated_web_apps/isolated_web_app_installer_view_controller.h"

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/version.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/ui/views/web_apps/isolated_web_apps/isolated_web_app_installer_model.h"
#include "chrome/browser/ui/views/web_apps/isolated_web_apps/isolated_web_app_installer_view.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_location.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/signed_web_bundle_metadata.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/fake_web_app_ui_manager.h"
#include "chrome/browser/web_applications/test/fake_web_contents_manager.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_task_environment.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/app_restore/full_restore_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/extensions/extension_keeplist_chromeos.h"
#include "chrome/browser/web_applications/app_service/test/loopback_crosapi_app_service_proxy.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace web_app {
namespace {

using ::testing::AllOf;
using ::testing::Exactly;
using ::testing::ExplainMatchResult;
using ::testing::Field;
using ::testing::IgnoreResult;
using ::testing::Invoke;
using ::testing::Property;
using DialogContent = IsolatedWebAppInstallerModel::DialogContent;

constexpr base::StringPiece kIconPath = "/icon.png";

MATCHER_P3(WithMetadata, app_id, app_name, version, "") {
  return ExplainMatchResult(
      AllOf(Property("app_id", &SignedWebBundleMetadata::app_id, app_id),
            Property("app_name", &SignedWebBundleMetadata::app_name, app_name),
            Property("version", &SignedWebBundleMetadata::version,
                     base::Version(version))),
      arg, result_listener);
}

MATCHER_P3(WithContents, is_error, message_id, details_id, "") {
  return ExplainMatchResult(
      AllOf(Field("is_error", &DialogContent::is_error, is_error),
            Field("message_id", &DialogContent::message, message_id),
            Field("details_id", &DialogContent::details, details_id)),
      arg, result_listener);
}

IsolatedWebAppUrlInfo CreateAndWriteTestBundle(
    const base::FilePath& bundle_path,
    const std::string& version) {
  TestSignedWebBundleBuilder::BuildOptions bundle_options =
      TestSignedWebBundleBuilder::BuildOptions().SetVersion(
          base::Version(version));
  auto bundle = TestSignedWebBundleBuilder::BuildDefault(bundle_options);
  base::ScopedAllowBlockingForTesting allow_blocking;
  CHECK(base::WriteFile(bundle_path, bundle.data));
  return IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(bundle.id);
}

SignedWebBundleMetadata CreateMetadata(const std::u16string& app_name,
                                       const std::string& version) {
  auto url_info = IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
      web_package::SignedWebBundleId::CreateRandomForDevelopment());
  return SignedWebBundleMetadata::CreateForTesting(
      url_info, DevModeBundle(base::FilePath()), app_name,
      base::Version(version), IconBitmaps());
}

IsolatedWebAppInstallerModel::DialogContent CreateDummyDialog() {
  return IsolatedWebAppInstallerModel::DialogContent(
      /*is_error=*/false, /*message=*/0, /*details=*/0);
}

blink::mojom::ManifestPtr CreateDefaultManifest(const GURL& iwa_url,
                                                const base::Version version) {
  auto manifest = blink::mojom::Manifest::New();
  manifest->id = iwa_url;
  manifest->scope = iwa_url.Resolve("/");
  manifest->start_url = iwa_url.Resolve("/testing-start-url.html");
  manifest->display = DisplayMode::kStandalone;
  manifest->short_name = u"test app name";
  manifest->version = base::UTF8ToUTF16(version.GetString());

  blink::Manifest::ImageResource icon;
  icon.src = iwa_url.Resolve(kIconPath);
  icon.purpose = {blink::mojom::ManifestImageResource_Purpose::ANY};
  icon.type = u"image/png";
  icon.sizes = {gfx::Size(256, 256)};
  manifest->icons.push_back(icon);

  return manifest;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
std::unique_ptr<KeyedService> NullServiceFactory(content::BrowserContext*) {
  return nullptr;
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

class MockView : public IsolatedWebAppInstallerView {
 public:
  explicit MockView(IsolatedWebAppInstallerView::Delegate* delegate)
      : IsolatedWebAppInstallerView(delegate) {}

  MOCK_METHOD(void, ShowDisabledScreen, (), (override));
  MOCK_METHOD(void, ShowGetMetadataScreen, (), (override));
  MOCK_METHOD(void,
              UpdateGetMetadataProgress,
              (double percent, int minutes_remaining),
              (override));
  MOCK_METHOD(void,
              ShowMetadataScreen,
              (const SignedWebBundleMetadata& bundle_metadata),
              (override));
  MOCK_METHOD(void,
              ShowInstallScreen,
              (const SignedWebBundleMetadata& bundle_metadata),
              (override));
  MOCK_METHOD(void,
              UpdateInstallProgress,
              (double percent, int minutes_remaining),
              (override));
  MOCK_METHOD(void,
              ShowInstallSuccessScreen,
              (const SignedWebBundleMetadata& bundle_metadata),
              (override));
  MOCK_METHOD(
      void,
      ShowDialog,
      (const IsolatedWebAppInstallerModel::DialogContent& dialog_content),
      (override));
};

}  // namespace

class IsolatedWebAppInstallerViewControllerTest : public ::testing::Test {
 public:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {features::kIsolatedWebApps, features::kIsolatedWebAppDevMode}, {});
    ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());

    TestingProfile::Builder profile_builder;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    profile_builder.SetIsMainProfile(true);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
    profile_ = profile_builder.Build();

#if BUILDFLAG(IS_CHROMEOS_ASH)
    ash::full_restore::FullRestoreServiceFactory::GetInstance()
        ->SetTestingFactory(profile_.get(),
                            base::BindRepeating(&NullServiceFactory));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
    // Set up Lacros so the AppService -> LaunchWebAppCommand plumbing works.
    extensions::SetEmptyAshKeeplistForTest();
    app_service_proxy_ =
        std::make_unique<LoopbackCrosapiAppServiceProxy>(profile_.get());
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  TestingProfile* profile() { return profile_.get(); }

  FakeWebAppProvider* fake_provider() {
    return FakeWebAppProvider::Get(profile());
  }

  base::FilePath CreateBundlePath(const std::string& bundle_filename) {
    return scoped_temp_dir_.GetPath().Append(
        base::FilePath::FromASCII(bundle_filename));
  }

  void MockIconAndPageState(const IsolatedWebAppUrlInfo& url_info,
                            const std::string& version = "7.7.7") {
    GURL iwa_url = url_info.origin().GetURL();
    auto& fake_web_contents_manager = static_cast<FakeWebContentsManager&>(
        fake_provider()->web_contents_manager());
    auto& icon_state = fake_web_contents_manager.GetOrCreateIconState(
        iwa_url.Resolve(kIconPath));
    icon_state.bitmaps = {CreateSquareIcon(32, SK_ColorWHITE)};

    GURL url(
        base::StrCat({chrome::kIsolatedAppScheme, url::kStandardSchemeSeparator,
                      kTestEd25519WebBundleId,
                      "/.well-known/_generated_install_page.html"}));
    auto& page_state = fake_web_contents_manager.GetOrCreatePageState(url);

    page_state.url_load_result = WebAppUrlLoaderResult::kUrlLoaded;
    page_state.error_code = webapps::InstallableStatusCode::NO_ERROR_DETECTED;
    page_state.manifest_url = iwa_url.Resolve("manifest.webmanifest");
    page_state.valid_manifest_for_web_app = true;
    page_state.opt_manifest =
        CreateDefaultManifest(iwa_url, base::Version(version));
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::ScopedTempDir scoped_temp_dir_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  std::unique_ptr<TestingProfile> profile_;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  std::unique_ptr<LoopbackCrosapiAppServiceProxy> app_service_proxy_;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
};

TEST_F(IsolatedWebAppInstallerViewControllerTest,
       ValidBundleTransitionsToShowMetadataScreen) {
  base::FilePath bundle_path = CreateBundlePath("test_bundle.swbn");
  IsolatedWebAppUrlInfo url_info = CreateAndWriteTestBundle(bundle_path, "1.0");
  MockIconAndPageState(url_info);

  IsolatedWebAppInstallerModel model(bundle_path);
  IsolatedWebAppInstallerViewController controller(profile(), fake_provider(),
                                                   &model);
  testing::StrictMock<MockView> view(&controller);
  controller.SetViewForTesting(&view);

  base::test::TestFuture<void> callback;
  EXPECT_CALL(view, ShowGetMetadataScreen());
  EXPECT_CALL(
      view, ShowMetadataScreen(WithMetadata("hoealecpbefphiclhampllbdbdpfmfpi",
                                            u"test app name", "7.7.7")))
      .WillOnce(Invoke(&callback, &base::test::TestFuture<void>::SetValue));

  controller.Start();

  EXPECT_TRUE(callback.Wait());
  EXPECT_EQ(model.step(), IsolatedWebAppInstallerModel::Step::kShowMetadata);
}

TEST_F(IsolatedWebAppInstallerViewControllerTest,
       InvalidBundleShowsErrorDialog) {
  base::FilePath bundle_path = CreateBundlePath("test_bundle.swbn");
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    CHECK(base::WriteFile(bundle_path, "not a valid bundle"));
  }
  auto url_info = IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
      web_package::SignedWebBundleId::CreateRandomForDevelopment());
  MockIconAndPageState(url_info);

  IsolatedWebAppInstallerModel model(bundle_path);
  IsolatedWebAppInstallerViewController controller(profile(), fake_provider(),
                                                   &model);
  testing::StrictMock<MockView> view(&controller);
  controller.SetViewForTesting(&view);

  base::test::TestFuture<void> callback;
  EXPECT_CALL(view, ShowGetMetadataScreen()).Times(Exactly(2));
  EXPECT_CALL(view,
              ShowDialog(WithContents(
                  /*is_error=*/true, IDS_IWA_INSTALLER_VERIFICATION_ERROR_TITLE,
                  IDS_IWA_INSTALLER_VERIFICATION_ERROR_SUBTITLE)))
      .WillOnce(Invoke(&callback, &base::test::TestFuture<void>::SetValue));

  controller.Start();

  EXPECT_TRUE(callback.Wait());
  EXPECT_EQ(model.step(), IsolatedWebAppInstallerModel::Step::kGetMetadata);
}

TEST_F(IsolatedWebAppInstallerViewControllerTest,
       InstallButtonLaunchesConfirmationDialog) {
  IsolatedWebAppInstallerModel model(CreateBundlePath("test_bundle.swbn"));
  IsolatedWebAppInstallerViewController controller(profile(), fake_provider(),
                                                   &model);
  testing::StrictMock<MockView> view(&controller);
  controller.SetViewForTesting(&view);

  SignedWebBundleMetadata metadata = CreateMetadata(u"Test App", "0.0.1");
  model.SetSignedWebBundleMetadata(metadata);
  model.SetStep(IsolatedWebAppInstallerModel::Step::kShowMetadata);

  base::test::TestFuture<void> callback;
  EXPECT_CALL(view, ShowMetadataScreen(metadata));
  EXPECT_CALL(view, ShowDialog(WithContents(
                        /*is_error=*/false, IDS_IWA_INSTALLER_CONFIRM_TITLE,
                        IDS_IWA_INSTALLER_CONFIRM_SUBTITLE)))
      .WillOnce(Invoke(&callback, &base::test::TestFuture<void>::SetValue));

  controller.OnAccept();

  EXPECT_TRUE(callback.Wait());
}

TEST_F(IsolatedWebAppInstallerViewControllerTest,
       ConfirmationDialogMovesToInstallScreen) {
  IsolatedWebAppInstallerModel model(CreateBundlePath("test_bundle.swbn"));
  IsolatedWebAppInstallerViewController controller(profile(), fake_provider(),
                                                   &model);
  testing::StrictMock<MockView> view(&controller);
  controller.SetViewForTesting(&view);

  SignedWebBundleMetadata metadata = CreateMetadata(u"Test App", "0.0.1");
  model.SetSignedWebBundleMetadata(metadata);
  model.SetStep(IsolatedWebAppInstallerModel::Step::kShowMetadata);
  model.SetDialogContent(CreateDummyDialog());

  base::test::TestFuture<void> callback;
  EXPECT_CALL(view, ShowInstallScreen(metadata))
      .WillOnce(Invoke(&callback, &base::test::TestFuture<void>::SetValue));

  controller.OnChildDialogAccepted();

  EXPECT_TRUE(callback.Wait());
}

TEST_F(IsolatedWebAppInstallerViewControllerTest,
       SuccessfulInstallationMovesToSuccessScreen) {
  base::FilePath bundle_path = CreateBundlePath("test_bundle.swbn");
  IsolatedWebAppUrlInfo url_info = CreateAndWriteTestBundle(bundle_path, "1.0");
  MockIconAndPageState(url_info, "1.0");

  IsolatedWebAppInstallerModel model(bundle_path);
  IsolatedWebAppInstallerViewController controller(profile(), fake_provider(),
                                                   &model);
  testing::StrictMock<MockView> view(&controller);
  controller.SetViewForTesting(&view);

  auto metadata = SignedWebBundleMetadata::CreateForTesting(
      url_info, InstalledBundle(bundle_path), u"app name", base::Version("1.0"),
      IconBitmaps());
  model.SetSignedWebBundleMetadata(metadata);
  model.SetStep(IsolatedWebAppInstallerModel::Step::kShowMetadata);
  model.SetDialogContent(CreateDummyDialog());

  base::test::TestFuture<void> callback;
  EXPECT_CALL(view, ShowInstallScreen(metadata));
  EXPECT_CALL(view, ShowInstallSuccessScreen(metadata))
      .WillOnce(Invoke(&callback, &base::test::TestFuture<void>::SetValue));

  controller.OnChildDialogAccepted();

  EXPECT_TRUE(callback.Wait());
  EXPECT_TRUE(
      fake_provider()->registrar_unsafe().IsInstalled(url_info.app_id()));
}

TEST_F(IsolatedWebAppInstallerViewControllerTest, CanLaunchAppAfterInstall) {
  base::FilePath bundle_path = CreateBundlePath("test_bundle.swbn");
  IsolatedWebAppUrlInfo url_info = CreateAndWriteTestBundle(bundle_path, "1.0");
  MockIconAndPageState(url_info, "1.0");

  IsolatedWebAppInstallerModel model(bundle_path);
  IsolatedWebAppInstallerViewController controller(profile(), fake_provider(),
                                                   &model);
  testing::StrictMock<MockView> view(&controller);
  controller.SetViewForTesting(&view);

  auto metadata = SignedWebBundleMetadata::CreateForTesting(
      url_info, InstalledBundle(bundle_path), u"app name", base::Version("1.0"),
      IconBitmaps());
  model.SetSignedWebBundleMetadata(metadata);
  model.SetStep(IsolatedWebAppInstallerModel::Step::kShowMetadata);
  model.SetDialogContent(CreateDummyDialog());

  EXPECT_CALL(view, ShowInstallScreen(metadata));
  EXPECT_CALL(view, ShowInstallSuccessScreen(metadata))
      .WillOnce(IgnoreResult(Invoke(
          &controller, &IsolatedWebAppInstallerViewController::OnAccept)));

  base::test::TestFuture<apps::AppLaunchParams, LaunchWebAppWindowSetting>
      future;
  static_cast<FakeWebAppUiManager*>(&fake_provider()->ui_manager())
      ->SetOnLaunchWebAppCallback(future.GetRepeatingCallback());

  controller.OnChildDialogAccepted();

  EXPECT_EQ(future.Get<0>().app_id, metadata.app_id());
}

TEST_F(IsolatedWebAppInstallerViewControllerTest,
       InstallationErrorShowsErrorDialog) {
  base::FilePath bundle_path = CreateBundlePath("test_bundle.swbn");
  IsolatedWebAppUrlInfo url_info = CreateAndWriteTestBundle(bundle_path, "1.0");
  MockIconAndPageState(url_info, "1.0");

  IsolatedWebAppInstallerModel model(bundle_path);
  IsolatedWebAppInstallerViewController controller(profile(), fake_provider(),
                                                   &model);
  testing::StrictMock<MockView> view(&controller);
  controller.SetViewForTesting(&view);

  auto metadata = SignedWebBundleMetadata::CreateForTesting(
      url_info, InstalledBundle(bundle_path), u"app name", base::Version("2.0"),
      IconBitmaps());
  model.SetSignedWebBundleMetadata(metadata);
  model.SetStep(IsolatedWebAppInstallerModel::Step::kShowMetadata);
  model.SetDialogContent(CreateDummyDialog());

  base::test::TestFuture<void> callback;
  EXPECT_CALL(view, ShowInstallScreen(metadata)).Times(Exactly(2));
  EXPECT_CALL(view,
              ShowDialog(WithContents(
                  /*is_error=*/true, IDS_IWA_INSTALLER_INSTALL_FAILED_TITLE,
                  IDS_IWA_INSTALLER_INSTALL_FAILED_SUBTITLE)))
      .WillOnce(Invoke(&callback, &base::test::TestFuture<void>::SetValue));

  controller.OnChildDialogAccepted();

  EXPECT_TRUE(callback.Wait());
  EXPECT_FALSE(
      fake_provider()->registrar_unsafe().IsInstalled(url_info.app_id()));
}

TEST_F(IsolatedWebAppInstallerViewControllerTest,
       InstallationErrorRetryRestartsFlow) {
  IsolatedWebAppInstallerModel model(CreateBundlePath("test_bundle.swbn"));
  IsolatedWebAppInstallerViewController controller(profile(), fake_provider(),
                                                   &model);
  testing::StrictMock<MockView> view(&controller);
  controller.SetViewForTesting(&view);

  SignedWebBundleMetadata metadata = CreateMetadata(u"Test App", "0.0.1");
  model.SetSignedWebBundleMetadata(metadata);
  model.SetStep(IsolatedWebAppInstallerModel::Step::kInstall);
  model.SetDialogContent(CreateDummyDialog());

  base::test::TestFuture<void> callback;
  EXPECT_CALL(view, ShowGetMetadataScreen())
      .WillOnce(Invoke(&callback, &base::test::TestFuture<void>::SetValue));

  controller.OnChildDialogAccepted();

  EXPECT_TRUE(callback.Wait());
}

}  // namespace web_app
