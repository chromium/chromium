// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/isolated_web_apps/isolated_web_app_installer_view_controller.h"

#include <memory>
#include <string>
#include <string_view>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/version.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/ui/views/web_apps/isolated_web_apps/isolated_web_app_installer_model.h"
#include "chrome/browser/ui/views/web_apps/isolated_web_apps/isolated_web_app_installer_view.h"
#include "chrome/browser/ui/views/web_apps/isolated_web_apps/test_isolated_web_app_installer_model_observer.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_source.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_storage_location.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_trust_checker.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolation_data.h"
#include "chrome/browser/web_applications/isolated_web_apps/signed_web_bundle_metadata.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/test_signed_web_bundle_builder.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/fake_web_app_ui_manager.h"
#include "chrome/browser/web_applications/test/fake_web_contents_manager.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_task_environment.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_pref_names.h"
#include "base/values.h"
#include "chrome/browser/ui/views/web_apps/isolated_web_apps/pref_observer.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/app_restore/full_restore_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/extensions/extension_keeplist_chromeos.h"
#include "chrome/browser/web_applications/app_service/test/loopback_crosapi_app_service_proxy.h"
#include "chromeos/crosapi/mojom/prefs.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace web_app {
namespace {

using ::testing::_;
using ::testing::AllOf;
using ::testing::AnyNumber;
using ::testing::Exactly;
using ::testing::ExplainMatchResult;
using ::testing::Field;
using ::testing::Invoke;
using ::testing::Property;
using ::testing::VariantWith;
using Step = IsolatedWebAppInstallerModel::Step;

constexpr std::string_view kIconPath = "/icon.png";

MATCHER_P3(WithMetadata, app_id, app_name, version, "") {
  return ExplainMatchResult(
      AllOf(Property("app_id", &SignedWebBundleMetadata::app_id, app_id),
            Property("app_name", &SignedWebBundleMetadata::app_name, app_name),
            Property("version", &SignedWebBundleMetadata::version,
                     base::Version(version))),
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
      web_package::SignedWebBundleId::CreateRandomForProxyMode());
  return SignedWebBundleMetadata::CreateForTesting(
      url_info, IwaSourceBundleProdMode(base::FilePath()), app_name,
      base::Version(version), IconBitmaps());
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
  MOCK_METHOD(void, ShowDisabledScreen, (), (override));
  MOCK_METHOD(void, ShowGetMetadataScreen, (), (override));
  MOCK_METHOD(void, UpdateGetMetadataProgress, (double progress), (override));
  MOCK_METHOD(void,
              ShowMetadataScreen,
              (const SignedWebBundleMetadata& bundle_metadata),
              (override));
  MOCK_METHOD(void,
              ShowInstallScreen,
              (const SignedWebBundleMetadata& bundle_metadata),
              (override));
  MOCK_METHOD(void, UpdateInstallProgress, (double progress), (override));
  MOCK_METHOD(void,
              ShowInstallSuccessScreen,
              (const SignedWebBundleMetadata& bundle_metadata),
              (override));
  MOCK_METHOD(views::Widget*,
              ShowDialog,
              (const IsolatedWebAppInstallerModel::Dialog& dialog),
              (override));
};

// Fake pref observer that mimics the behavior of an actual observer. i.e.
// posts callback to run:
// - Once on `Start()`.
// - Every time the pref value is changed.
class FakeIsolatedWebAppsEnabledPrefObserver
    : public IsolatedWebAppsEnabledPrefObserver {
 public:
  explicit FakeIsolatedWebAppsEnabledPrefObserver(bool initial_value) {
    // The pref only exists for ChromeOS, for all other OSs, we just post
    // callback with |true|.
#if !BUILDFLAG(IS_CHROMEOS)
    initial_value = true;
#endif
    value_ = initial_value;
  }

  void Start(PrefChangedCallback callback) override {
    CHECK(!callback_);
    callback_ = callback;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(callback_, value_));
  }

  void Reset() override { callback_.Reset(); }

  void UpdatePref(bool value) {
    if (value_ != value) {
      value_ = value;
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(callback_, value_));
    }
  }

 private:
  PrefChangedCallback callback_;
  bool value_;
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

    // Launching requires real os integration.
    fake_provider()->UseRealOsIntegrationManager();

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
                      test::GetDefaultEd25519WebBundleId().id(),
                      "/.well-known/_generated_install_page.html"}));
    auto& page_state = fake_web_contents_manager.GetOrCreatePageState(url);

    page_state.url_load_result = webapps::WebAppUrlLoaderResult::kUrlLoaded;
    page_state.error_code = webapps::InstallableStatusCode::NO_ERROR_DETECTED;
    page_state.manifest_url = iwa_url.Resolve("manifest.webmanifest");
    page_state.valid_manifest_for_web_app = true;
    page_state.manifest_before_default_processing =
        CreateDefaultManifest(iwa_url, base::Version(version));
  }

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  ScopedTestingLocalState local_state_{TestingBrowserProcess::GetGlobal()};
  web_app::OsIntegrationTestOverrideBlockingRegistration faked_os_integration_;
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
  SetTrustedWebBundleIdsForTesting({url_info.web_bundle_id()});
  MockIconAndPageState(url_info);

  IsolatedWebAppInstallerModel model{IwaSourceBundleProdMode(bundle_path)};
  auto pref_observer =
      std::make_unique<FakeIsolatedWebAppsEnabledPrefObserver>(true);
  IsolatedWebAppInstallerViewController controller(
      profile(), fake_provider(), &model, std::move(pref_observer));
  testing::StrictMock<MockView> view;
  controller.SetViewForTesting(&view);

  EXPECT_CALL(view, UpdateGetMetadataProgress(_)).Times(AnyNumber());
  EXPECT_CALL(view, ShowGetMetadataScreen());
  EXPECT_CALL(
      view, ShowMetadataScreen(WithMetadata("hoealecpbefphiclhampllbdbdpfmfpi",
                                            u"test app name", "7.7.7")));

  controller.Start(base::DoNothing(), base::DoNothing());

  TestIsolatedWebAppInstallerModelObserver(&model).WaitForStepChange(
      Step::kShowMetadata);
}

TEST_F(IsolatedWebAppInstallerViewControllerTest,
       InvalidBundleShowsErrorDialog) {
  base::FilePath bundle_path = CreateBundlePath("test_bundle.swbn");
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    CHECK(base::WriteFile(bundle_path, "not a valid bundle"));
  }
  auto url_info = IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
      web_package::SignedWebBundleId::CreateRandomForProxyMode());
  MockIconAndPageState(url_info);

  IsolatedWebAppInstallerModel model{IwaSourceBundleProdMode(bundle_path)};
  auto pref_observer =
      std::make_unique<FakeIsolatedWebAppsEnabledPrefObserver>(true);
  IsolatedWebAppInstallerViewController controller(
      profile(), fake_provider(), &model, std::move(pref_observer));
  testing::StrictMock<MockView> view;
  controller.SetViewForTesting(&view);

  EXPECT_CALL(view, UpdateGetMetadataProgress(_)).Times(AnyNumber());
  EXPECT_CALL(view, ShowGetMetadataScreen());
  EXPECT_CALL(
      view,
      ShowDialog(
          VariantWith<IsolatedWebAppInstallerModel::BundleInvalidDialog>(_)));

  controller.Start(base::DoNothing(), base::DoNothing());

  TestIsolatedWebAppInstallerModelObserver(&model).WaitForChildDialog();
  EXPECT_EQ(model.step(), Step::kGetMetadata);
}

TEST_F(IsolatedWebAppInstallerViewControllerTest,
       OutdatedBundleShowsAlreadyInstalledDialog) {
  base::FilePath bundle_path = CreateBundlePath("test_bundle.swbn");
  IsolatedWebAppUrlInfo url_info = CreateAndWriteTestBundle(bundle_path, "1.0");
  SetTrustedWebBundleIdsForTesting({url_info.web_bundle_id()});
  MockIconAndPageState(url_info, "1.0");

  AddDummyIsolatedAppToRegistry(
      profile(), url_info.origin().GetURL(), "app",
      IsolationData::Builder(
          IwaStorageOwnedBundle{/*dir_name_ascii=*/"", /*dev_mode=*/false},
          base::Version("2.0"))
          .Build());

  IsolatedWebAppInstallerModel model{IwaSourceBundleProdMode(bundle_path)};
  model.SetStep(Step::kGetMetadata);

  auto pref_observer =
      std::make_unique<FakeIsolatedWebAppsEnabledPrefObserver>(true);
  IsolatedWebAppInstallerViewController controller(
      profile(), fake_provider(), &model, std::move(pref_observer));
  testing::StrictMock<MockView> view;
  controller.SetViewForTesting(&view);

  EXPECT_CALL(view, UpdateGetMetadataProgress(_)).Times(AnyNumber());
  EXPECT_CALL(view, ShowGetMetadataScreen());
  EXPECT_CALL(
      view,
      ShowDialog(
          VariantWith<
              IsolatedWebAppInstallerModel::BundleAlreadyInstalledDialog>(_)));

  controller.Start(base::DoNothing(), base::DoNothing());

  TestIsolatedWebAppInstallerModelObserver(&model).WaitForChildDialog();
  EXPECT_EQ(model.step(), Step::kGetMetadata);
}

TEST_F(IsolatedWebAppInstallerViewControllerTest,
       NewerBundleShowsAlreadyInstalledDialog) {
  base::FilePath bundle_path = CreateBundlePath("test_bundle.swbn");
  IsolatedWebAppUrlInfo url_info = CreateAndWriteTestBundle(bundle_path, "2.0");
  SetTrustedWebBundleIdsForTesting({url_info.web_bundle_id()});
  MockIconAndPageState(url_info, "2.0");

  AddDummyIsolatedAppToRegistry(
      profile(), url_info.origin().GetURL(), "app",
      IsolationData::Builder(
          IwaStorageOwnedBundle{/*dir_name_ascii=*/"", /*dev_mode=*/false},
          base::Version("1.0"))
          .Build());

  IsolatedWebAppInstallerModel model{IwaSourceBundleProdMode(bundle_path)};
  model.SetStep(Step::kGetMetadata);

  auto pref_observer =
      std::make_unique<FakeIsolatedWebAppsEnabledPrefObserver>(true);
  IsolatedWebAppInstallerViewController controller(
      profile(), fake_provider(), &model, std::move(pref_observer));
  testing::StrictMock<MockView> view;
  controller.SetViewForTesting(&view);

  EXPECT_CALL(view, UpdateGetMetadataProgress(_)).Times(AnyNumber());
  EXPECT_CALL(view, ShowGetMetadataScreen());
  EXPECT_CALL(
      view,
      ShowDialog(
          VariantWith<
              IsolatedWebAppInstallerModel::BundleAlreadyInstalledDialog>(_)));

  controller.Start(base::DoNothing(), base::DoNothing());

  TestIsolatedWebAppInstallerModelObserver(&model).WaitForChildDialog();
  EXPECT_EQ(model.step(), Step::kGetMetadata);
}

TEST_F(IsolatedWebAppInstallerViewControllerTest,
       InstallButtonLaunchesConfirmationDialog) {
  base::FilePath bundle_path = CreateBundlePath("test_bundle.swbn");
  IsolatedWebAppInstallerModel model{IwaSourceBundleProdMode(bundle_path)};
  SignedWebBundleMetadata metadata = CreateMetadata(u"Test App", "0.0.1");
  model.SetSignedWebBundleMetadata(metadata);
  model.SetStep(Step::kShowMetadata);

  auto pref_observer =
      std::make_unique<FakeIsolatedWebAppsEnabledPrefObserver>(true);
  IsolatedWebAppInstallerViewController controller(
      profile(), fake_provider(), &model, std::move(pref_observer));
  testing::StrictMock<MockView> view;
  controller.SetViewForTesting(&view);

  EXPECT_CALL(
      view,
      ShowDialog(
          VariantWith<IsolatedWebAppInstallerModel::ConfirmInstallationDialog>(
              _)));

  controller.OnAccept();

  TestIsolatedWebAppInstallerModelObserver(&model).WaitForChildDialog();
}

TEST_F(IsolatedWebAppInstallerViewControllerTest,
       ConfirmationDialogMovesToInstallScreen) {
  base::FilePath bundle_path = CreateBundlePath("test_bundle.swbn");
  IsolatedWebAppInstallerModel model{IwaSourceBundleProdMode(bundle_path)};
  SignedWebBundleMetadata metadata = CreateMetadata(u"Test App", "0.0.1");
  model.SetSignedWebBundleMetadata(metadata);
  model.SetStep(Step::kShowMetadata);
  model.SetDialog(IsolatedWebAppInstallerModel::ConfirmInstallationDialog{
      base::DoNothing()});

  auto pref_observer =
      std::make_unique<FakeIsolatedWebAppsEnabledPrefObserver>(true);
  IsolatedWebAppInstallerViewController controller(
      profile(), fake_provider(), &model, std::move(pref_observer));
  testing::StrictMock<MockView> view;
  controller.SetViewForTesting(&view);

  EXPECT_CALL(view, ShowInstallScreen(metadata));

  controller.OnChildDialogAccepted();

  TestIsolatedWebAppInstallerModelObserver(&model).WaitForStepChange(
      Step::kInstall);
}

TEST_F(IsolatedWebAppInstallerViewControllerTest,
       SuccessfulInstallationMovesToSuccessScreen) {
  base::FilePath bundle_path = CreateBundlePath("test_bundle.swbn");
  IsolatedWebAppUrlInfo url_info = CreateAndWriteTestBundle(bundle_path, "1.0");
  SetTrustedWebBundleIdsForTesting({url_info.web_bundle_id()});
  MockIconAndPageState(url_info, "1.0");

  IsolatedWebAppInstallerModel model{IwaSourceBundleProdMode(bundle_path)};
  auto metadata = SignedWebBundleMetadata::CreateForTesting(
      url_info, IwaSourceBundleProdMode(bundle_path), u"app name",
      base::Version("1.0"), IconBitmaps());
  model.SetSignedWebBundleMetadata(metadata);
  model.SetStep(Step::kShowMetadata);
  model.SetDialog(IsolatedWebAppInstallerModel::ConfirmInstallationDialog{
      base::DoNothing()});

  auto pref_observer =
      std::make_unique<FakeIsolatedWebAppsEnabledPrefObserver>(true);
  IsolatedWebAppInstallerViewController controller(
      profile(), fake_provider(), &model, std::move(pref_observer));
  testing::StrictMock<MockView> view;
  controller.SetViewForTesting(&view);

  EXPECT_CALL(view, UpdateInstallProgress(_)).Times(AnyNumber());
  EXPECT_CALL(view, ShowInstallScreen(metadata));
  EXPECT_CALL(view, ShowInstallSuccessScreen(metadata));

  controller.OnChildDialogAccepted();

  TestIsolatedWebAppInstallerModelObserver(&model).WaitForStepChange(
      Step::kInstallSuccess);
  EXPECT_TRUE(
      fake_provider()->registrar_unsafe().IsInstalled(url_info.app_id()));
}

TEST_F(IsolatedWebAppInstallerViewControllerTest, CanLaunchAppAfterInstall) {
  base::FilePath bundle_path = CreateBundlePath("test_bundle.swbn");
  IsolatedWebAppUrlInfo url_info = CreateAndWriteTestBundle(bundle_path, "1.0");
  SetTrustedWebBundleIdsForTesting({url_info.web_bundle_id()});
  MockIconAndPageState(url_info, "1.0");

  IsolatedWebAppInstallerModel model{IwaSourceBundleProdMode(bundle_path)};
  auto metadata = SignedWebBundleMetadata::CreateForTesting(
      url_info, IwaSourceBundleProdMode(bundle_path), u"app name",
      base::Version("1.0"), IconBitmaps());
  model.SetSignedWebBundleMetadata(metadata);
  model.SetStep(Step::kShowMetadata);
  model.SetDialog(IsolatedWebAppInstallerModel::ConfirmInstallationDialog{
      base::DoNothing()});

  auto pref_observer =
      std::make_unique<FakeIsolatedWebAppsEnabledPrefObserver>(true);
  IsolatedWebAppInstallerViewController controller(
      profile(), fake_provider(), &model, std::move(pref_observer));
  testing::StrictMock<MockView> view;
  controller.SetViewForTesting(&view);

  EXPECT_CALL(view, ShowInstallScreen(metadata));
  EXPECT_CALL(view, UpdateInstallProgress(_)).Times(AnyNumber());
  EXPECT_CALL(view, ShowInstallSuccessScreen(metadata));

  controller.OnChildDialogAccepted();

  TestIsolatedWebAppInstallerModelObserver(&model).WaitForStepChange(
      Step::kInstallSuccess);

  base::test::TestFuture<apps::AppLaunchParams, LaunchWebAppWindowSetting>
      future;
  static_cast<FakeWebAppUiManager*>(&fake_provider()->ui_manager())
      ->SetOnLaunchWebAppCallback(future.GetRepeatingCallback());

  controller.OnAccept();

  EXPECT_EQ(future.Get<0>().app_id, metadata.app_id());
}

TEST_F(IsolatedWebAppInstallerViewControllerTest,
       InstallationErrorShowsErrorDialog) {
  base::FilePath bundle_path = CreateBundlePath("test_bundle.swbn");
  IsolatedWebAppUrlInfo url_info = CreateAndWriteTestBundle(bundle_path, "1.0");
  SetTrustedWebBundleIdsForTesting({url_info.web_bundle_id()});
  MockIconAndPageState(url_info, "1.0");

  IsolatedWebAppInstallerModel model{IwaSourceBundleProdMode(bundle_path)};
  auto metadata = SignedWebBundleMetadata::CreateForTesting(
      url_info, IwaSourceBundleProdMode(bundle_path), u"app name",
      base::Version("2.0"), IconBitmaps());
  model.SetSignedWebBundleMetadata(metadata);
  model.SetStep(Step::kShowMetadata);
  model.SetDialog(IsolatedWebAppInstallerModel::ConfirmInstallationDialog{
      base::DoNothing()});

  auto pref_observer =
      std::make_unique<FakeIsolatedWebAppsEnabledPrefObserver>(true);
  IsolatedWebAppInstallerViewController controller(
      profile(), fake_provider(), &model, std::move(pref_observer));
  testing::StrictMock<MockView> view;
  controller.SetViewForTesting(&view);

  EXPECT_CALL(view, UpdateInstallProgress(_)).Times(AnyNumber());
  EXPECT_CALL(view, ShowInstallScreen(metadata));
  EXPECT_CALL(
      view,
      ShowDialog(
          VariantWith<IsolatedWebAppInstallerModel::InstallationFailedDialog>(
              _)));

  controller.OnChildDialogAccepted();

  TestIsolatedWebAppInstallerModelObserver(&model).WaitForChildDialog();
  EXPECT_FALSE(
      fake_provider()->registrar_unsafe().IsInstalled(url_info.app_id()));
}

TEST_F(IsolatedWebAppInstallerViewControllerTest,
       InstallationErrorRetryRestartsFlow) {
  base::FilePath bundle_path = CreateBundlePath("test_bundle.swbn");
  IsolatedWebAppInstallerModel model{IwaSourceBundleProdMode(bundle_path)};
  SignedWebBundleMetadata metadata = CreateMetadata(u"Test App", "0.0.1");
  model.SetSignedWebBundleMetadata(metadata);
  model.SetStep(Step::kInstall);
  model.SetDialog(IsolatedWebAppInstallerModel::InstallationFailedDialog{});

  auto pref_observer =
      std::make_unique<FakeIsolatedWebAppsEnabledPrefObserver>(true);
  IsolatedWebAppInstallerViewController controller(
      profile(), fake_provider(), &model, std::move(pref_observer));

  testing::StrictMock<MockView> view;
  controller.SetViewForTesting(&view);
  controller.completion_callback_ = base::DoNothing();

  EXPECT_CALL(view, ShowGetMetadataScreen());

  controller.OnChildDialogAccepted();

  TestIsolatedWebAppInstallerModelObserver(&model).WaitForStepChange(
      Step::kGetMetadata);
}

#if BUILDFLAG(IS_CHROMEOS)

TEST_F(IsolatedWebAppInstallerViewControllerTest,
       ChangingPrefToFalseDisablesInstaller) {
  base::FilePath bundle_path = CreateBundlePath("test_bundle.swbn");
  IsolatedWebAppUrlInfo url_info = CreateAndWriteTestBundle(bundle_path, "1.0");
  SetTrustedWebBundleIdsForTesting({url_info.web_bundle_id()});
  MockIconAndPageState(url_info);

  IsolatedWebAppInstallerModel model{IwaSourceBundleProdMode(bundle_path)};
  auto pref_observer =
      std::make_unique<FakeIsolatedWebAppsEnabledPrefObserver>(true);
  FakeIsolatedWebAppsEnabledPrefObserver* raw_pref_observer =
      pref_observer.get();
  IsolatedWebAppInstallerViewController controller(
      profile(), fake_provider(), &model, std::move(pref_observer));
  testing::StrictMock<MockView> view;
  controller.SetViewForTesting(&view);

  EXPECT_CALL(view, UpdateGetMetadataProgress(_)).Times(AnyNumber());
  EXPECT_CALL(view, ShowGetMetadataScreen());
  EXPECT_CALL(
      view, ShowMetadataScreen(WithMetadata("hoealecpbefphiclhampllbdbdpfmfpi",
                                            u"test app name", "7.7.7")));

  controller.Start(base::DoNothing(), base::DoNothing());

  TestIsolatedWebAppInstallerModelObserver model_observer(&model);
  model_observer.WaitForStepChange(Step::kShowMetadata);

  EXPECT_CALL(view, ShowDisabledScreen());

  raw_pref_observer->UpdatePref(false);

  model_observer.WaitForStepChange(Step::kDisabled);
}

TEST_F(IsolatedWebAppInstallerViewControllerTest,
       ChangingPrefToTrueRestartsInstaller) {
  base::FilePath bundle_path = CreateBundlePath("test_bundle.swbn");
  IsolatedWebAppUrlInfo url_info = CreateAndWriteTestBundle(bundle_path, "1.0");
  SetTrustedWebBundleIdsForTesting({url_info.web_bundle_id()});
  MockIconAndPageState(url_info);

  IsolatedWebAppInstallerModel model{IwaSourceBundleProdMode(bundle_path)};
  auto pref_observer =
      std::make_unique<FakeIsolatedWebAppsEnabledPrefObserver>(false);
  FakeIsolatedWebAppsEnabledPrefObserver* raw_pref_observer =
      pref_observer.get();
  IsolatedWebAppInstallerViewController controller(
      profile(), fake_provider(), &model, std::move(pref_observer));
  testing::StrictMock<MockView> view;
  controller.SetViewForTesting(&view);

  EXPECT_CALL(view, ShowDisabledScreen());

  controller.Start(base::DoNothing(), base::DoNothing());

  TestIsolatedWebAppInstallerModelObserver model_observer(&model);
  model_observer.WaitForStepChange(Step::kDisabled);

  ASSERT_EQ(model.step(), Step::kDisabled);

  EXPECT_CALL(view, UpdateGetMetadataProgress(_)).Times(AnyNumber());
  EXPECT_CALL(view, ShowGetMetadataScreen());
  EXPECT_CALL(
      view, ShowMetadataScreen(WithMetadata("hoealecpbefphiclhampllbdbdpfmfpi",
                                            u"test app name", "7.7.7")));

  raw_pref_observer->UpdatePref(true);

  model_observer.WaitForStepChange(Step::kShowMetadata);
}
#endif  // BUILDFLAG(IS_CHROMEOS)
}  // namespace web_app
