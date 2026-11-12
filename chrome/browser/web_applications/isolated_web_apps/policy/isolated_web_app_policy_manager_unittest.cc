// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_policy_manager.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/check_deref.h"
#include "base/containers/flat_set.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_writer.h"
#include "base/run_loop.h"
#include "base/strings/to_string.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/repeating_test_future.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/test/values_test_util.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/component_updater/iwa_key_distribution_component_installer.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/chrome_iwa_client.h"
#include "chrome/browser/web_applications/isolated_web_apps/install/isolated_web_app_install_source.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_test.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/iwa_test_server_configurator.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/key_distribution/test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/policy_generator.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/policy_test_utils.h"
#include "chrome/browser/web_applications/model/isolation_data.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/fake_web_contents_manager.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_filter.h"
#include "chrome/browser/web_applications/web_app_management_type.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/component_updater/component_updater_service.h"
#include "components/component_updater/installer_policies/iwa_key_distribution_component_installer_policy.h"
#include "components/component_updater/mock_component_updater_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/user_manager/user.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/web_package/test_support/signed_web_bundles/ed25519_key_pair.h"
#include "components/webapps/browser/web_contents/web_app_url_loader.h"
#include "components/webapps/common/web_app_id.h"
#include "components/webapps/isolated_web_apps/key_distribution/iwa_key_distribution_histograms.h"
#include "components/webapps/isolated_web_apps/key_distribution/iwa_key_distribution_info_provider.h"
#include "components/webapps/isolated_web_apps/key_distribution/proto/key_distribution.pb.h"
#include "components/webapps/isolated_web_apps/test_support/signing_keys.h"
#include "components/webapps/isolated_web_apps/types/iwa_version.h"
#include "components/webapps/isolated_web_apps/types/storage_location.h"
#include "content/public/common/content_features.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

namespace web_app {

namespace {

constexpr std::string_view kIwaKeyDistributionComponentId =
    "iebhnlpddlcpcfpfalldikcoeakpeoah";

using Component = component_updater::IwaKeyDistributionComponentInstallerPolicy;
using ComponentRegistration = component_updater::ComponentRegistration;

using ::testing::_;
using ::testing::DoAll;
using ::testing::Eq;
using ::testing::Field;
using ::testing::IsNull;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::UnorderedElementsAre;
using ::testing::WithoutArgs;

class MockOnDemandUpdater : public component_updater::OnDemandUpdater {
 public:
  MOCK_METHOD(void,
              OnDemandUpdate,
              (const std::string&,
               component_updater::OnDemandUpdater::Priority,
               component_updater::Callback),
              (override));
};

std::unique_ptr<base::ScopedTempDir> CreateIwaComponentDir(
    const base::Version& version,
    const IwaKeyDistribution& component_data,
    bool is_preloaded) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  auto dir = std::make_unique<base::ScopedTempDir>();
  CHECK(dir->CreateUniqueTempDir());

  auto manifest = base::DictValue()
                      .Set("manifest_version", 1)
                      .Set("name", Component::kManifestName)
                      .Set("version", version.GetString());
  if (is_preloaded) {
    manifest.Set("is_preloaded", true);
  }

  CHECK(
      base::WriteFile(dir->GetPath().Append(FILE_PATH_LITERAL("manifest.json")),
                      *base::WriteJson(manifest)));
  CHECK(base::WriteFile(dir->GetPath().Append(Component::kDataFileName),
                        component_data.SerializeAsString()));

  return dir;
}

class IwaComponentWrapper {
 public:
  IwaComponentWrapper() {
    auto cus = std::make_unique<
        testing::NiceMock<component_updater::MockComponentUpdateService>>();
    cus_ = cus.get();
    TestingBrowserProcess::GetGlobal()->SetComponentUpdater(std::move(cus));

    ON_CALL(*cus_, GetOnDemandUpdater)
        .WillByDefault(ReturnRef(on_demand_updater()));
    ON_CALL(*cus_, RegisterComponent(Field(&ComponentRegistration::app_id,
                                           Eq(kIwaKeyDistributionComponentId))))
        .WillByDefault(DoAll(
            [&](const ComponentRegistration& component) {
              CHECK(!on_component_registered_.is_signaled())
                  << " Component registration is supposed to only happen once.";
              installer_ = component.installer;
              on_component_registered_.Signal();
            },
            Return(true)));
    component_updater::RegisterIwaKeyDistributionComponent(cus_);
  }

  void InstallComponentAsync(const base::Version& version,
                             const IwaKeyDistribution& component_data,
                             bool is_preloaded) {
    auto component_dir_path =
        WriteIwaComponentData(version, component_data, is_preloaded);
    on_component_registered_.Post(
        FROM_HERE, base::BindLambdaForTesting([&, component_dir_path] {
          base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
              FROM_HERE,
              base::BindOnce(&update_client::CrxInstaller::Install, installer_,
                             component_dir_path,
                             /*public_key=*/"", /*install_params=*/nullptr,
                             base::DoNothing(), base::DoNothing()));
        }));
  }

  MockOnDemandUpdater& on_demand_updater() { return on_demand_updater_; }

 private:
  base::FilePath WriteIwaComponentData(const base::Version& version,
                                       const IwaKeyDistribution& component_data,
                                       bool is_preloaded) {
    CHECK(!component_dirs_.contains(version))
        << " There's already an installed component with version " << version;
    std::unique_ptr<base::ScopedTempDir> dir =
        CreateIwaComponentDir(version, component_data, is_preloaded);
    auto path = dir->GetPath();
    component_dirs_[version] = std::move(dir);
    return path;
  }

  // Owned by `g_browser_process`.
  raw_ptr<component_updater::MockComponentUpdateService> cus_ = nullptr;

  // `on_demand_updater_` is defined as StrictMock to prevent situations where
  // `OnDemandUpdate()` is dispatched to an empty implementation; this is only a
  // likely case if the initial component data is marked as preloaded by the
  // inheriting test suite.
  testing::StrictMock<MockOnDemandUpdater> on_demand_updater_;

  base::OneShotEvent on_component_registered_;
  scoped_refptr<update_client::CrxInstaller> installer_;

  base::flat_map<base::Version, std::unique_ptr<base::ScopedTempDir>>
      component_dirs_;
};

}  // namespace

class IsolatedWebAppPolicyManagerTestBase : public IsolatedWebAppTest {
 public:
  explicit IsolatedWebAppPolicyManagerTestBase(
      bool is_user_session,
      base::test::TaskEnvironment::TimeSource time_source =
          base::test::TaskEnvironment::TimeSource::MOCK_TIME)
      : IsolatedWebAppTest(time_source, WithDevMode{}),
        is_user_session_(is_user_session) {}

  void SetUpServedIwas() {
    std::unique_ptr<ScopedBundledIsolatedWebApp> app1 =
        IsolatedWebAppBuilder(ManifestBuilder().SetVersion("1.0.0"))
            .BuildBundle(test::GetDefaultEd25519KeyPair());
    app1->FakeInstallPageState(profile());

    std::unique_ptr<ScopedBundledIsolatedWebApp> app2 =
        IsolatedWebAppBuilder(ManifestBuilder().SetVersion("1.0.0"))
            .BuildBundle(test::GetDefaultEcdsaP256KeyPair());
    app2->FakeInstallPageState(profile());

    lazy_app1_id_ = app1->web_bundle_id();
    lazy_app2_id_ = app2->web_bundle_id();

    test_update_server().AddBundle(std::move(app1));
    test_update_server().AddBundle(std::move(app2));
  }

  void SetUp() override {
    IwaKeyDistributionInfoProvider::GetInstanceForTesting()
        .SkipManagedAllowlistChecksForTesting(true);

    IsolatedWebAppTest::SetUp();

    if (ShouldStartWebAppProvider()) {
      test::AwaitStartWebAppProviderAndSubsystems(profile());
      SetUpServedIwas();
    }

#if BUILDFLAG(IS_CHROMEOS)
    if (!is_user_session_) {
      test_managed_guest_session_ =
          std::make_unique<profiles::testing::ScopedTestManagedGuestSession>();
    }
#else
    // Suppress -Wunused-private-field warning.
    (void)is_user_session_;
#endif  // BUILDFLAG(IS_CHROMEOS)
  }

  void TearDown() override {
    IwaKeyDistributionInfoProvider::GetInstanceForTesting()
        .SkipManagedAllowlistChecksForTesting(false);
    IsolatedWebAppTest::TearDown();
  }

  virtual bool ShouldStartWebAppProvider() const { return true; }

  sync_preferences::TestingPrefServiceSyncable* pref_service() {
    return profile()->GetTestingPrefService();
  }

  FakeWebContentsManager& fake_web_contents_manager() {
    return static_cast<FakeWebContentsManager&>(
        provider().web_contents_manager());
  }

  void AssertAppInstalled(const web_package::SignedWebBundleId& swbn_id) {
    const WebApp* web_app = provider().registrar_unsafe().GetAppById(
        IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(swbn_id).app_id());
    ASSERT_THAT(web_app, testing::NotNull()) << "The app in not installed :(";
  }

  const web_package::SignedWebBundleId& web_bundle_id_1() {
    return *lazy_app1_id_;
  }
  const web_package::SignedWebBundleId& web_bundle_id_2() {
    return *lazy_app2_id_;
  }

 private:
  const bool is_user_session_;
#if BUILDFLAG(IS_CHROMEOS)
  std::unique_ptr<profiles::testing::ScopedTestManagedGuestSession>
      test_managed_guest_session_;
#endif  // BUILDFLAG(IS_CHROMEOS)

  std::optional<web_package::SignedWebBundleId> lazy_app1_id_;
  std::optional<web_package::SignedWebBundleId> lazy_app2_id_;
};

class IsolatedWebAppPolicyManagerTest
    : public IsolatedWebAppPolicyManagerTestBase {
 public:
  IsolatedWebAppPolicyManagerTest()
      : IsolatedWebAppPolicyManagerTestBase(/*is_user_session=*/true) {}
};

TEST_F(IsolatedWebAppPolicyManagerTest, AppInstalled) {
  auto url_info =
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(web_bundle_id_1());

  WebAppTestInstallObserver install_observer(profile());
  install_observer.BeginListening({url_info.app_id()});

  test::AddForceInstalledIwaToPolicy(
      profile()->GetPrefs(),
      IwaTestServerConfigurator::CreateForceInstallPolicyEntry(
          url_info.web_bundle_id()));

  EXPECT_EQ(install_observer.Wait(), url_info.app_id());
  task_environment().RunUntilIdle();

  const WebApp* web_app =
      provider().registrar_unsafe().GetAppById(url_info.app_id());
  ASSERT_THAT(web_app, NotNull());
  EXPECT_THAT(web_app->GetSources(),
              Eq(WebAppManagementTypes({WebAppManagement::Type::kIwaPolicy})));
}

TEST_F(IsolatedWebAppPolicyManagerTest, AppInstalledAtPinnedVersion) {
  const IwaVersion pinned_version = *IwaVersion::Create("1.0.0");
  auto url_info =
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(web_bundle_id_1());

  WebAppTestInstallObserver install_observer(profile());
  install_observer.BeginListening({url_info.app_id()});

  test::AddForceInstalledIwaToPolicy(
      profile()->GetPrefs(),
      IwaTestServerConfigurator::CreateForceInstallPolicyEntry(
          web_bundle_id_1(), /*update_channel=*/std::nullopt,
          /*pinned_version=*/pinned_version));

  EXPECT_EQ(install_observer.Wait(), url_info.app_id());
  task_environment().RunUntilIdle();

  const WebApp* web_app =
      provider().registrar_unsafe().GetAppById(url_info.app_id());
  ASSERT_THAT(web_app, NotNull());
  ASSERT_EQ(web_app->isolation_data()->version(), pinned_version);
  EXPECT_THAT(web_app->GetSources(),
              Eq(WebAppManagementTypes({WebAppManagement::Type::kIwaPolicy})));
}

TEST_F(IsolatedWebAppPolicyManagerTest, AppNotInstalledIncorrectPinnedVersion) {
  const IwaVersion pinned_version = *IwaVersion::Create("1.9.0");
  auto url_info =
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(web_bundle_id_1());

  WebAppTestInstallObserver install_observer(profile());
  install_observer.BeginListening({url_info.app_id()});

  test::AddForceInstalledIwaToPolicy(
      profile()->GetPrefs(),
      IwaTestServerConfigurator::CreateForceInstallPolicyEntry(
          web_bundle_id_1(), /*update_channel=*/std::nullopt,
          /*pinned_version=*/pinned_version));

  task_environment().RunUntilIdle();
  EXPECT_FALSE(provider().registrar_unsafe().AppMatches(
      url_info.app_id(), WebAppFilter::PolicyInstalledIsolatedWebApp()));
}

TEST_F(IsolatedWebAppPolicyManagerTest,
       AddForceInstalledSourceToUserInstalledIwa) {
  const std::unique_ptr<ScopedBundledIsolatedWebApp> bundle =
      IsolatedWebAppBuilder(ManifestBuilder().SetVersion("1.0.0"))
          .BuildBundle(test::GetDefaultEd25519KeyPair());

  bundle->FakeInstallPageState(profile());
  const IsolatedWebAppUrlInfo url_info = bundle->InstallChecked(profile());
  {
    const WebApp* web_app =
        provider().registrar_unsafe().GetAppById(url_info.app_id());
    ASSERT_THAT(web_app, NotNull());
    EXPECT_THAT(
        web_app->GetSources(),
        Eq(WebAppManagementTypes({WebAppManagement::Type::kIwaUserInstalled})));
    EXPECT_FALSE(provider().registrar_unsafe().AppMatches(
        url_info.app_id(), WebAppFilter::PolicyInstalledIsolatedWebApp()));
  }

  WebAppTestInstallObserver install_observer(profile());
  install_observer.BeginListening({url_info.app_id()});

  test::AddForceInstalledIwaToPolicy(
      profile()->GetPrefs(),
      IwaTestServerConfigurator::CreateForceInstallPolicyEntry(
          url_info.web_bundle_id()));

  EXPECT_EQ(install_observer.Wait(), url_info.app_id());
  task_environment().RunUntilIdle();

  const WebApp* web_app =
      provider().registrar_unsafe().GetAppById(url_info.app_id());
  ASSERT_THAT(web_app, NotNull());
  EXPECT_THAT(
      web_app->GetSources(),
      Eq(WebAppManagementTypes({WebAppManagement::Type::kIwaPolicy,
                                WebAppManagement::Type::kIwaUserInstalled})));
  EXPECT_TRUE(provider().registrar_unsafe().AppMatches(
      url_info.app_id(), WebAppFilter::PolicyInstalledIsolatedWebApp()));
}

TEST_F(IsolatedWebAppPolicyManagerTest, AppForceInstallSourceRemoved) {
  const std::unique_ptr<ScopedBundledIsolatedWebApp> bundle =
      IsolatedWebAppBuilder(ManifestBuilder().SetVersion("1.0.0"))
          .BuildBundle(test::GetDefaultEd25519KeyPair());

  bundle->FakeInstallPageState(profile());
  const IsolatedWebAppUrlInfo url_info = bundle->InstallChecked(profile());
  {
    const WebApp* web_app =
        provider().registrar_unsafe().GetAppById(url_info.app_id());
    ASSERT_THAT(web_app, NotNull());
    EXPECT_THAT(
        web_app->GetSources(),
        Eq(WebAppManagementTypes({WebAppManagement::Type::kIwaUserInstalled})));
    EXPECT_FALSE(provider().registrar_unsafe().AppMatches(
        url_info.app_id(), WebAppFilter::PolicyInstalledIsolatedWebApp()));
  }

  WebAppTestInstallObserver install_observer(profile());
  install_observer.BeginListening({url_info.app_id()});

  test::AddForceInstalledIwaToPolicy(
      profile()->GetPrefs(),
      IwaTestServerConfigurator::CreateForceInstallPolicyEntry(
          url_info.web_bundle_id()));

  EXPECT_EQ(install_observer.Wait(), url_info.app_id());
  task_environment().RunUntilIdle();

  {
    const WebApp* web_app =
        provider().registrar_unsafe().GetAppById(url_info.app_id());
    EXPECT_THAT(
        web_app->GetSources(),
        Eq(WebAppManagementTypes({WebAppManagement::Type::kIwaPolicy,
                                  WebAppManagement::Type::kIwaUserInstalled})));
  }

  {
    test::RemoveForceInstalledIwaFromPolicy(profile()->GetPrefs(),
                                            url_info.web_bundle_id());
    task_environment().RunUntilIdle();

    const WebApp* web_app =
        provider().registrar_unsafe().GetAppById(url_info.app_id());
    EXPECT_THAT(
        web_app->GetSources(),
        Eq(WebAppManagementTypes({WebAppManagement::Type::kIwaUserInstalled})));
  }
}

TEST_F(IsolatedWebAppPolicyManagerTest, CantUserInstallAppWhenForceInstalled) {
  const std::unique_ptr<ScopedBundledIsolatedWebApp> bundle =
      IsolatedWebAppBuilder(ManifestBuilder().SetVersion("1.0.0"))
          .BuildBundle(test::GetDefaultEd25519KeyPair());
  auto url_info = IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
      bundle->web_bundle_id());

  WebAppTestInstallObserver install_observer(profile());
  install_observer.BeginListening({url_info.app_id()});

  test::AddForceInstalledIwaToPolicy(
      profile()->GetPrefs(),
      IwaTestServerConfigurator::CreateForceInstallPolicyEntry(
          url_info.web_bundle_id()));

  EXPECT_EQ(install_observer.Wait(), url_info.app_id());
  task_environment().RunUntilIdle();

  const WebApp* web_app =
      provider().registrar_unsafe().GetAppById(url_info.app_id());
  ASSERT_THAT(web_app, NotNull());
  EXPECT_THAT(web_app->GetSources(),
              Eq(WebAppManagementTypes({WebAppManagement::Type::kIwaPolicy})));

  EXPECT_EQ(bundle
                ->InstallWithSource(profile(),
                                    &IsolatedWebAppInstallSource::FromDevUi)
                .error(),
            "App is already installed");
}

TEST_F(IsolatedWebAppPolicyManagerTest, DevModeAppRemovedWhenForceInstalled) {
  const std::unique_ptr<ScopedBundledIsolatedWebApp> bundle =
      IsolatedWebAppBuilder(ManifestBuilder().SetVersion("1.0.0"))
          .BuildBundle(test::GetDefaultEd25519KeyPair());

  ASSERT_OK_AND_ASSIGN(const IsolatedWebAppUrlInfo url_info,
                       bundle->InstallWithSource(
                           profile(), &IsolatedWebAppInstallSource::FromDevUi));

  WebAppTestInstallObserver install_observer(profile());
  install_observer.BeginListening({url_info.app_id()});

  test::AddForceInstalledIwaToPolicy(
      profile()->GetPrefs(),
      IwaTestServerConfigurator::CreateForceInstallPolicyEntry(
          url_info.web_bundle_id()));

  EXPECT_EQ(install_observer.Wait(), url_info.app_id());
  task_environment().RunUntilIdle();

  const WebApp* web_app =
      provider().registrar_unsafe().GetAppById(url_info.app_id());
  ASSERT_THAT(web_app, NotNull());
  EXPECT_THAT(web_app->GetSources(),
              Eq(WebAppManagementTypes({WebAppManagement::Type::kIwaPolicy})));
}

#if BUILDFLAG(IS_CHROMEOS)
class ManagedGuestSessionInstallTest
    : public IsolatedWebAppPolicyManagerTestBase {
 public:
  ManagedGuestSessionInstallTest()
      : IsolatedWebAppPolicyManagerTestBase(/*is_user_session=*/false) {}
};

TEST_F(ManagedGuestSessionInstallTest, AppInstalled) {
  auto url_info =
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(web_bundle_id_1());

  WebAppTestInstallObserver install_observer(profile());
  install_observer.BeginListening({url_info.app_id()});

  test::AddForceInstalledIwaToPolicy(
      profile()->GetPrefs(),
      IwaTestServerConfigurator::CreateForceInstallPolicyEntry(
          url_info.web_bundle_id()));

  EXPECT_EQ(install_observer.Wait(), url_info.app_id());
  provider().command_manager().AwaitAllCommandsCompleteForTesting();

  const WebApp* web_app =
      provider().registrar_unsafe().GetAppById(url_info.app_id());
  ASSERT_THAT(web_app, NotNull());
  EXPECT_THAT(web_app->GetSources(),
              Eq(WebAppManagementTypes({WebAppManagement::Type::kIwaPolicy})));
}

#endif  // BUILDFLAG(IS_CHROMEOS)

class IsolatedWebAppManagedAllowlistTest
    : public IsolatedWebAppPolicyManagerTestBase {
 public:
  IsolatedWebAppManagedAllowlistTest()
      : IsolatedWebAppPolicyManagerTestBase(/*is_user_session=*/true) {}

  void SetUp() override {
    IsolatedWebAppPolicyManagerTestBase::SetUp();
    IwaKeyDistributionInfoProvider::GetInstanceForTesting()
        .SkipManagedAllowlistChecksForTesting(false);
  }
};
using base::test::HasValue;

TEST_F(IsolatedWebAppManagedAllowlistTest, AllowedAppInstalled) {
  base::HistogramTester ht;
  const auto url_info =
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(web_bundle_id_1());

  WebAppTestInstallObserver install_observer(profile());
  install_observer.BeginListening({url_info.app_id()});

  base::test::TestFuture<web_package::SignedWebBundleId, IwaInstallerResult>
      future;
  IsolatedWebAppPolicyManager::SetOnInstallTaskCompletedCallbackForTesting(
      future.GetRepeatingCallback());

  // Update allowlist
  EXPECT_OK(test::KeyDistributionComponentBuilder(base::Version("1.0.1"))
                .AddToManagedAllowlist(web_bundle_id_1())
                .Build()
                .UploadFromComponentFolder());

  test::AddForceInstalledIwaToPolicy(
      profile()->GetPrefs(),
      IwaTestServerConfigurator::CreateForceInstallPolicyEntry(
          url_info.web_bundle_id()));

  auto [web_bundle_id, result] = future.Take();
  EXPECT_EQ(web_bundle_id, web_bundle_id_1());
  EXPECT_EQ(result.type(), IwaInstallerResultType::kSuccess);

  EXPECT_EQ(install_observer.Wait(), url_info.app_id());

  EXPECT_THAT(
      ht.GetAllSamples(kIwaKeyDistributionManagedInstallAllowedHistogramName),
      base::BucketsAre(base::Bucket(true, 2)));

  const WebApp* web_app =
      provider().registrar_unsafe().GetAppById(url_info.app_id());
  ASSERT_THAT(web_app, NotNull());
  EXPECT_THAT(web_app->GetSources(),
              Eq(WebAppManagementTypes({WebAppManagement::Type::kIwaPolicy})));
}

TEST_F(IsolatedWebAppManagedAllowlistTest, NotAllowedAppInstallationRefused) {
  base::HistogramTester ht;
  auto url_info =
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(web_bundle_id_1());

  base::test::TestFuture<web_package::SignedWebBundleId, IwaInstallerResult>
      future;
  IsolatedWebAppPolicyManager::SetOnInstallTaskCompletedCallbackForTesting(
      future.GetRepeatingCallback());

  // Ensure allowlist is empty
  EXPECT_OK(test::KeyDistributionComponentBuilder(base::Version("1.0.1"))
                .WithManagedAllowlist({})  // For clarity only
                .Build()
                .UploadFromComponentFolder());

  EXPECT_FALSE(IwaKeyDistributionInfoProvider::GetInstanceForTesting()
                   .IsManagedInstallPermitted(web_bundle_id_1().id()));

  test::AddForceInstalledIwaToPolicy(
      profile()->GetPrefs(),
      IwaTestServerConfigurator::CreateForceInstallPolicyEntry(
          url_info.web_bundle_id()));

  auto [web_bundle_id, result] = future.Take();
  EXPECT_EQ(web_bundle_id, web_bundle_id_1());
  EXPECT_EQ(result.type(), IwaInstallerResultType::kErrorAppNotInAllowlist);

  EXPECT_THAT(
      ht.GetAllSamples(kIwaKeyDistributionManagedInstallAllowedHistogramName),
      base::BucketsAre(base::Bucket(false, 2)));

  const WebApp* web_app =
      provider().registrar_unsafe().GetAppById(url_info.app_id());
  EXPECT_THAT(web_app, testing::IsNull());
}

using IsolatedWebAppPolicyManagerPolicyRaceTest =
    IsolatedWebAppPolicyManagerTest;

// Verifies that the updating of policy during previous policy processing
// is handled correctly.
TEST_F(IsolatedWebAppPolicyManagerPolicyRaceTest,
       PolicyUpdateWhileInstallInProgress) {
  const auto app1_url_info =
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(web_bundle_id_1());
  const auto app2_url_info =
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(web_bundle_id_2());
  const GURL app1_bundle_url = test_update_server().GetBundleUrlForIwa(
      web_bundle_id_1(), *IwaVersion::Create("1.0.0"));
  const GURL kNonExistentUpdateManifestUrl(
      "https://update/manifest/does/not/exist");

  // Hold app1's bundle download pending so policy #1 stays in progress while
  // policies #2 and #3 arrive.
  url_loader_factory().EraseResponse(app1_bundle_url);
  base::flat_set<GURL> requested_urls;
  url_loader_factory().SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        requested_urls.insert(request.url);
      }));

  test::AddForceInstalledIwaToPolicy(
      profile()->GetPrefs(),
      IwaTestServerConfigurator::CreateForceInstallPolicyEntry(
          web_bundle_id_1()));
  url_loader_factory().WaitForRequest(app1_bundle_url);

  // Update the policy at the moment when first policy update is being
  // processed. We set the policy to force install not existing app.
  // This policy variant will not be processed because it will be replaced
  // by the third policy update.
  {
    PolicyGenerator policy_generator;
    const web_package::SignedWebBundleId id =
        web_package::SignedWebBundleId::Create(
            "xyzxyzxyzxyzxyzxyzxyzxyzxyzxyzxyzxyzxyzxyzxyzxyzxyzaaaic")
            .value();
    policy_generator.AddForceInstalledIwa(id, kNonExistentUpdateManifestUrl);
    profile()->GetPrefs()->Set(prefs::kIsolatedWebAppInstallForceList,
                               policy_generator.Generate());
  }
  task_environment().RunUntilIdle();

  // The third policy update. This one must be processed.
  profile()->GetPrefs()->SetList(
      prefs::kIsolatedWebAppInstallForceList,
      base::ListValue()
          .Append(IwaTestServerConfigurator::CreateForceInstallPolicyEntry(
              web_bundle_id_1()))
          .Append(IwaTestServerConfigurator::CreateForceInstallPolicyEntry(
              web_bundle_id_2())));
  task_environment().RunUntilIdle();

  WebAppTestInstallObserver install_observer(profile());
  install_observer.BeginListening(
      {app1_url_info.app_id(), app2_url_info.app_id()});

  // Unblock app1's pending download so policy #1 finishes and policy #3
  // reprocesses to install the final set.
  std::unique_ptr<ScopedBundledIsolatedWebApp> app1 =
      IsolatedWebAppBuilder(ManifestBuilder().SetVersion("1.0.0"))
          .BuildBundle(test::GetDefaultEd25519KeyPair());
  app1->FakeInstallPageState(profile());
  test_update_server().AddBundle(std::move(app1));

  install_observer.Wait();
  provider().command_manager().AwaitAllCommandsCompleteForTesting();
  url_loader_factory().SetInterceptor(base::NullCallback());

  EXPECT_FALSE(requested_urls.contains(kNonExistentUpdateManifestUrl));
  EXPECT_THAT(
      provider().registrar_unsafe().GetAppIds(),
      UnorderedElementsAre(app1_url_info.app_id(), app2_url_info.app_id()));
  EXPECT_THAT(provider()
                  .registrar_unsafe()
                  .GetAppById(app1_url_info.app_id())
                  ->GetSources(),
              Eq(WebAppManagementTypes({WebAppManagement::Type::kIwaPolicy})));
  EXPECT_THAT(provider()
                  .registrar_unsafe()
                  .GetAppById(app2_url_info.app_id())
                  ->GetSources(),
              Eq(WebAppManagementTypes({WebAppManagement::Type::kIwaPolicy})));
}

class IsolatedWebAppPolicyManagerUninstallTest
    : public IsolatedWebAppPolicyManagerTestBase {
 public:
  IsolatedWebAppPolicyManagerUninstallTest()
      : IsolatedWebAppPolicyManagerTestBase(/*is_user_session=*/true) {}
};

// Remove an app from policy and verify that it is uninstalled while the
// remaining policy-installed app stays installed.
TEST_F(IsolatedWebAppPolicyManagerUninstallTest, OneAppUninstalled) {
  // Force install 2 apps.
  {
    profile()->GetPrefs()->SetList(
        prefs::kIsolatedWebAppInstallForceList,
        base::ListValue()
            .Append(IwaTestServerConfigurator::CreateForceInstallPolicyEntry(
                web_bundle_id_1()))
            .Append(IwaTestServerConfigurator::CreateForceInstallPolicyEntry(
                web_bundle_id_2())));

    task_environment().RunUntilIdle();

    AssertAppInstalled(web_bundle_id_1());
    AssertAppInstalled(web_bundle_id_2());
  }

  // Now generate a policy with 1 app and expect the other app to be
  // uninstalled.
  {
    const webapps::AppId app2_id =
        IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(web_bundle_id_2())
            .app_id();
    WebAppTestUninstallObserver uninstall_observer(profile());
    uninstall_observer.BeginListening({app2_id});

    profile()->GetPrefs()->SetList(
        prefs::kIsolatedWebAppInstallForceList,
        base::ListValue().Append(
            IwaTestServerConfigurator::CreateForceInstallPolicyEntry(
                web_bundle_id_1())));

    EXPECT_EQ(uninstall_observer.Wait(), app2_id);
    provider().command_manager().AwaitAllCommandsCompleteForTesting();

    AssertAppInstalled(web_bundle_id_1());
    EXPECT_THAT(provider().registrar_unsafe().GetAppById(app2_id), IsNull());
  }
}

TEST_F(IsolatedWebAppPolicyManagerUninstallTest, BothAppUninstalled) {
  // Force install 2 apps.
  {
    profile()->GetPrefs()->SetList(
        prefs::kIsolatedWebAppInstallForceList,
        base::ListValue()
            .Append(IwaTestServerConfigurator::CreateForceInstallPolicyEntry(
                web_bundle_id_1()))
            .Append(IwaTestServerConfigurator::CreateForceInstallPolicyEntry(
                web_bundle_id_2())));

    task_environment().RunUntilIdle();

    AssertAppInstalled(web_bundle_id_1());
    AssertAppInstalled(web_bundle_id_2());
  }

  // Set the policy without any app and expect both previously installed apps
  // to be uninstalled.
  {
    const webapps::AppId app1_id =
        IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(web_bundle_id_1())
            .app_id();
    const webapps::AppId app2_id =
        IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(web_bundle_id_2())
            .app_id();

    WebAppTestUninstallObserver uninstall_observer(profile());
    uninstall_observer.BeginListening({app1_id, app2_id});

    profile()->GetPrefs()->SetList(prefs::kIsolatedWebAppInstallForceList,
                                   base::ListValue());

    uninstall_observer.Wait();

    // WebAppTestUninstallObserver already triggers when the app is not fully
    // uninstalled. This causes issues with references to destroyed profiles
    // (see https://crbug.com/41484323#comment7). Wait until the app is actually
    // uninstalled here.
    task_environment().RunUntilIdle();
    provider().command_manager().AwaitAllCommandsCompleteForTesting();

    EXPECT_THAT(provider().registrar_unsafe().GetAppById(app1_id), IsNull());
    EXPECT_THAT(provider().registrar_unsafe().GetAppById(app2_id), IsNull());
  }
}

TEST_F(IsolatedWebAppPolicyManagerUninstallTest,
       UserInstalledAppStaysInstalled) {
  const std::unique_ptr<ScopedBundledIsolatedWebApp> bundle =
      IsolatedWebAppBuilder(ManifestBuilder().SetVersion("1.0.0"))
          .BuildBundle(test::GetDefaultEd25519KeyPair());

  const IsolatedWebAppUrlInfo url_info = bundle->InstallChecked(profile());
  // User-install the app.
  {
    const WebApp* web_app =
        provider().registrar_unsafe().GetAppById(url_info.app_id());
    ASSERT_THAT(web_app, NotNull());
    EXPECT_THAT(
        web_app->GetSources(),
        Eq(WebAppManagementTypes({WebAppManagement::Type::kIwaUserInstalled})));
  }

  // Force install the app via policy.
  {
    WebAppTestInstallObserver install_observer(profile());
    install_observer.BeginListening({url_info.app_id()});

    profile()->GetPrefs()->SetList(
        prefs::kIsolatedWebAppInstallForceList,
        base::ListValue().Append(
            IwaTestServerConfigurator::CreateForceInstallPolicyEntry(
                web_bundle_id_1())));

    install_observer.Wait();

    const WebApp* web_app =
        provider().registrar_unsafe().GetAppById(url_info.app_id());
    ASSERT_THAT(web_app, NotNull());
    EXPECT_THAT(
        web_app->GetSources(),
        Eq(WebAppManagementTypes({WebAppManagement::Type::kIwaPolicy,
                                  WebAppManagement::Type::kIwaUserInstalled})));
  }

  // Removing the app from policy should drop only the policy source and keep
  // the user-installed app installed.
  {
    profile()->GetPrefs()->SetList(prefs::kIsolatedWebAppInstallForceList,
                                   base::ListValue());
    ASSERT_TRUE(base::test::RunUntil([&]() {
      const WebApp* app =
          provider().registrar_unsafe().GetAppById(url_info.app_id());
      return app && !app->GetSources().Has(WebAppManagement::Type::kIwaPolicy);
    }));
    provider().command_manager().AwaitAllCommandsCompleteForTesting();

    const WebApp* web_app =
        provider().registrar_unsafe().GetAppById(url_info.app_id());
    ASSERT_THAT(web_app, NotNull());
    EXPECT_TRUE(web_app->WasInstalledByUser());
    EXPECT_THAT(
        web_app->GetSources(),
        Eq(WebAppManagementTypes({WebAppManagement::Type::kIwaUserInstalled})));
  }
}

// No apps should be uninstalled if no apps have been removed from policy.
TEST_F(IsolatedWebAppPolicyManagerUninstallTest, NoAppsUninstalled) {
  const webapps::AppId app1_id =
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(web_bundle_id_1())
          .app_id();
  const webapps::AppId app2_id =
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(web_bundle_id_2())
          .app_id();

  test::AddForceInstalledIwaToPolicy(
      profile()->GetPrefs(),
      IwaTestServerConfigurator::CreateForceInstallPolicyEntry(
          web_bundle_id_1()));

  task_environment().RunUntilIdle();

  AssertAppInstalled(web_bundle_id_1());

  test::AddForceInstalledIwaToPolicy(
      profile()->GetPrefs(),
      IwaTestServerConfigurator::CreateForceInstallPolicyEntry(
          web_bundle_id_2()));

  task_environment().RunUntilIdle();
  provider().command_manager().AwaitAllCommandsCompleteForTesting();

  AssertAppInstalled(web_bundle_id_1());
  AssertAppInstalled(web_bundle_id_2());
  EXPECT_THAT(provider().registrar_unsafe().GetAppIds(),
              UnorderedElementsAre(app1_id, app2_id));
}

class IsolatedWebAppRetryTest : public IsolatedWebAppPolicyManagerTestBase {
 public:
  IsolatedWebAppRetryTest()
      : IsolatedWebAppPolicyManagerTestBase(
            /*is_user_session=*/true,
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    features_.InitAndDisableFeature(kIwaPolicyManagerOnDemandComponentUpdate);
  }

 protected:
  size_t completed_install_tasks() const { return completed_install_tasks_; }

  void SetInstallPageLoadResult(const IsolatedWebAppUrlInfo& url_info,
                                webapps::WebAppUrlLoaderResult result) {
    fake_web_contents_manager()
        .GetOrCreatePageState(
            url_info.origin().GetURL().Resolve(kInstallPagePath))
        .url_load_result = result;
  }

  std::unique_ptr<ScopedBundledIsolatedWebApp> CreateServedBundleForId(
      const web_package::SignedWebBundleId& bundle_id) {
    auto key_pair =
        bundle_id == web_bundle_id_1()
            ? web_package::test::KeyPair(test::GetDefaultEd25519KeyPair())
            : web_package::test::KeyPair(test::GetDefaultEcdsaP256KeyPair());
    auto bundle = IsolatedWebAppBuilder(ManifestBuilder().SetVersion("1.0.0"))
                      .BuildBundle(key_pair);
    bundle->FakeInstallPageState(profile());
    return bundle;
  }

 private:
  void SetUp() override {
    IsolatedWebAppPolicyManager::SetOnInstallTaskCompletedCallbackForTesting(
        base::BindLambdaForTesting(
            [this](web_package::SignedWebBundleId, IwaInstallerResult) {
              ++completed_install_tasks_;
            }));
    IsolatedWebAppPolicyManagerTestBase::SetUp();
  }

  void TearDown() override {
    IsolatedWebAppPolicyManager::SetOnInstallTaskCompletedCallbackForTesting(
        base::NullCallback());
    IsolatedWebAppPolicyManagerTestBase::TearDown();
  }

  size_t completed_install_tasks_ = 0;
  base::test::ScopedFeatureList features_;
};

TEST_F(IsolatedWebAppRetryTest, FirstInstallFailsRetrySucceeds) {
  auto url_info =
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(web_bundle_id_1());
  SetInstallPageLoadResult(
      url_info, webapps::WebAppUrlLoaderResult::kFailedErrorPageLoaded);

  test::AddForceInstalledIwaToPolicy(
      profile()->GetPrefs(),
      IwaTestServerConfigurator::CreateForceInstallPolicyEntry(
          web_bundle_id_1()));

  // Run the first attempt to install the isolated web app (which should fail).
  task_environment().FastForwardBy(base::TimeDelta(base::Seconds(1)));

  ASSERT_EQ(1u, completed_install_tasks());
  const WebApp* web_app_t0 =
      provider().registrar_unsafe().GetAppById(url_info.app_id());
  ASSERT_THAT(web_app_t0, IsNull());

  // Fast forward right before the retry should happen --> retry to process the
  // policy is still scheduled, but the isolated web app is not yet installed.
  SetInstallPageLoadResult(url_info,
                           webapps::WebAppUrlLoaderResult::kUrlLoaded);
  task_environment().FastForwardBy(base::TimeDelta(base::Seconds(58)));

  const WebApp* web_app_t1 =
      provider().registrar_unsafe().GetAppById(url_info.app_id());
  ASSERT_THAT(web_app_t1, IsNull());

  WebAppTestInstallObserver install_observer(profile());
  install_observer.BeginListening({url_info.app_id()});

  // Fast forward another second and the app should be installed.
  task_environment().FastForwardBy(base::TimeDelta(base::Seconds(1)));

  ASSERT_EQ(2u, completed_install_tasks());

  // Make sure that even if there are further install tasks scheduled, they are
  // failing and therefore do not accidentally make this test pass.
  SetInstallPageLoadResult(
      url_info, webapps::WebAppUrlLoaderResult::kFailedErrorPageLoaded);

  EXPECT_EQ(install_observer.Wait(), url_info.app_id());

  ASSERT_EQ(2u, completed_install_tasks());
  const WebApp* web_app_t2 =
      provider().registrar_unsafe().GetAppById(url_info.app_id());
  ASSERT_THAT(web_app_t2, NotNull());
  EXPECT_THAT(web_app_t2->GetSources(),
              Eq(WebAppManagementTypes({WebAppManagement::Type::kIwaPolicy})));
}

TEST_F(IsolatedWebAppRetryTest, RetryTimeStepsCorrect) {
#if BUILDFLAG(IS_MAC)
  // TODO(crbug.com/434660312): Re-enable on macOS 26 once issues with
  // unexpected test timeout failures are resolved.
  if (base::mac::MacOSMajorVersion() == 26) {
    GTEST_SKIP() << "Disabled on macOS Tahoe.";
  }
#endif
  const std::array<web_package::SignedWebBundleId, 2> kApps = {
      web_bundle_id_1(), web_bundle_id_2()};

  const std::vector<int> desired_retry_time_steps_in_seconds = {
      // Continuously increasing delay by i * 60.
      0,
      60,
      180,
      420,
      900,
      1860,
      3780,
      7620,
      15300,
      30660,
      // From here on the delay saturates at 5 hours.
      48660,
      66660,
      84660,
  };

  // Try multiple apps to make sure that the delay gets reset after a successful
  // installation.
  unsigned int expected_number_install_tasks = 1u;
  for (const auto& web_bundle_id : kApps) {
    auto url_info =
        IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(web_bundle_id);
    SetInstallPageLoadResult(
        url_info, webapps::WebAppUrlLoaderResult::kFailedErrorPageLoaded);

    // Make sure all the scheduled policy process calls related to startup have
    // already been executed
    task_environment().RunUntilIdle();

    test::AddForceInstalledIwaToPolicy(
        profile()->GetPrefs(),
        IwaTestServerConfigurator::CreateForceInstallPolicyEntry(
            web_bundle_id));

    for (size_t i = 0; i < desired_retry_time_steps_in_seconds.size() - 1;
         ++i) {
      const int& current_time_step = desired_retry_time_steps_in_seconds[i];
      const int& next_time_step = desired_retry_time_steps_in_seconds[i + 1];

      // Another (failed) attempt to install the isolated web app
      task_environment().FastForwardBy(base::TimeDelta(base::Seconds(1)));

      ASSERT_EQ(expected_number_install_tasks, completed_install_tasks());
      const WebApp* web_app_t0 =
          provider().registrar_unsafe().GetAppById(url_info.app_id());
      ASSERT_THAT(web_app_t0, IsNull());

      // Fast forward right before the retry should happen --> retry to process
      // the policy is still scheduled, but the install task has not yet run.
      task_environment().FastForwardBy(base::TimeDelta(
          base::Seconds(next_time_step - current_time_step - 2)));

      const WebApp* web_app_t1 =
          provider().registrar_unsafe().GetAppById(url_info.app_id());
      ASSERT_THAT(web_app_t1, IsNull());

      // Fast forward another second and the next retry should happen.
      task_environment().FastForwardBy(base::TimeDelta(base::Seconds(1)));
      ASSERT_EQ(++expected_number_install_tasks, completed_install_tasks());
    }

    WebAppTestInstallObserver install_observer(profile());
    install_observer.BeginListening({url_info.app_id()});

    // Finally make the installation work. This should reset the delay for the
    // next install.
    SetInstallPageLoadResult(url_info,
                             webapps::WebAppUrlLoaderResult::kUrlLoaded);
    task_environment().FastForwardBy(base::TimeDelta(base::Seconds(18000)));

    EXPECT_EQ(install_observer.Wait(), url_info.app_id());

    const WebApp* web_app =
        provider().registrar_unsafe().GetAppById(url_info.app_id());
    ASSERT_THAT(web_app, NotNull());
    EXPECT_THAT(
        web_app->GetSources(),
        Eq(WebAppManagementTypes({WebAppManagement::Type::kIwaPolicy})));
    expected_number_install_tasks += 2;
  }
}

// This test checks that retries are only scheduled once all install tasks are
// done. It does so by installing two isolated web apps. The first app install
// finishes immediately (but fails), while the second app does not finish for 60
// seconds. In these 60 seconds, no retry should be scheduled. The test then
// manually triggers the completion of the second install task (which succeeds).
// From that point in time, a retry should be scheduled with a delay of another
// 60 seconds.
TEST_F(IsolatedWebAppRetryTest, RetryTriggeredWhenAllTasksDone) {
  auto url_info_1 =
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(web_bundle_id_1());
  auto url_info_2 =
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(web_bundle_id_2());

  // Determine the alphabetical queue sorting order
  bool is_1_first = url_info_1.web_bundle_id() < url_info_2.web_bundle_id();
  const auto& first_url_info = is_1_first ? url_info_1 : url_info_2;
  const auto& second_url_info = is_1_first ? url_info_2 : url_info_1;

  // The first app installation finishes immediately, but fails. The second app
  // bundle download is held pending so its install task stays in progress until
  // we serve its bundle response.
  SetInstallPageLoadResult(
      first_url_info, webapps::WebAppUrlLoaderResult::kFailedErrorPageLoaded);
  const GURL second_bundle_url = test_update_server().GetBundleUrlForIwa(
      second_url_info.web_bundle_id(), *IwaVersion::Create("1.0.0"));
  url_loader_factory().EraseResponse(second_bundle_url);

  profile()->GetPrefs()->SetList(
      prefs::kIsolatedWebAppInstallForceList,
      base::ListValue()
          .Append(IwaTestServerConfigurator::CreateForceInstallPolicyEntry(
              web_bundle_id_1()))
          .Append(IwaTestServerConfigurator::CreateForceInstallPolicyEntry(
              web_bundle_id_2())));

  // Run the first attempt to install the isolated apps (the first one fails
  // immediately, the second one is still busy downloading its bundle).
  task_environment().FastForwardBy(base::TimeDelta(base::Seconds(1)));

  ASSERT_EQ(1u, completed_install_tasks());
  ASSERT_TRUE(url_loader_factory().IsPending(second_bundle_url.spec()));
  const WebApp* web_app1_t0 =
      provider().registrar_unsafe().GetAppById(first_url_info.app_id());
  ASSERT_THAT(web_app1_t0, IsNull());
  const WebApp* web_app2_t0 =
      provider().registrar_unsafe().GetAppById(second_url_info.app_id());
  ASSERT_THAT(web_app2_t0, IsNull());

  // Forward by 60 seconds. Because the second app was not completed yet, still
  // no retry should be scheduled.
  task_environment().FastForwardBy(base::TimeDelta(base::Seconds(60)));
  ASSERT_EQ(1u, completed_install_tasks());
  ASSERT_TRUE(url_loader_factory().IsPending(second_bundle_url.spec()));

  // Complete install task for the second app (which succeeds).
  WebAppTestInstallObserver app2_install_observer(profile());
  app2_install_observer.BeginListening({second_url_info.app_id()});

  test_update_server().AddBundle(
      CreateServedBundleForId(second_url_info.web_bundle_id()));
  task_environment().FastForwardBy(base::TimeDelta(base::Seconds(1)));

  EXPECT_EQ(app2_install_observer.Wait(), second_url_info.app_id());
  ASSERT_EQ(2u, completed_install_tasks());

  // The retry command for the first app should be successful. The second app
  // doesn't need a retry.
  SetInstallPageLoadResult(first_url_info,
                           webapps::WebAppUrlLoaderResult::kUrlLoaded);
  task_environment().FastForwardBy(base::TimeDelta(base::Seconds(1)));
  // The retry is scheduled, but the install task for the remaining app has not
  // yet run.
  ASSERT_EQ(2u, completed_install_tasks());

  // Forward to right before an additional install task for the first app is
  // scheduled.
  task_environment().FastForwardBy(base::TimeDelta(base::Seconds(57)));
  ASSERT_EQ(2u, completed_install_tasks());

  WebAppTestInstallObserver app1_install_observer(profile());
  app1_install_observer.BeginListening({first_url_info.app_id()});

  // Moving the clock forward will finally install the first app.
  task_environment().FastForwardBy(base::TimeDelta(base::Seconds(1)));
  ASSERT_EQ(3u, completed_install_tasks());

  EXPECT_EQ(app1_install_observer.Wait(), first_url_info.app_id());

  const WebApp* web_app1_t2 =
      provider().registrar_unsafe().GetAppById(first_url_info.app_id());
  ASSERT_THAT(web_app1_t2, NotNull());
  EXPECT_THAT(web_app1_t2->GetSources(),
              Eq(WebAppManagementTypes({WebAppManagement::Type::kIwaPolicy})));
  const WebApp* web_app2_t2 =
      provider().registrar_unsafe().GetAppById(second_url_info.app_id());
  ASSERT_THAT(web_app2_t2, NotNull());
  EXPECT_THAT(web_app2_t2->GetSources(),
              Eq(WebAppManagementTypes({WebAppManagement::Type::kIwaPolicy})));
}

class CleanupOrphanedBundlesTest : public IsolatedWebAppPolicyManagerTestBase {
 public:
  CleanupOrphanedBundlesTest()
      : IsolatedWebAppPolicyManagerTestBase(/*is_user_session=*/true) {}

  bool ShouldStartWebAppProvider() const override { return false; }
};

TEST_F(CleanupOrphanedBundlesTest, CleanUpCalledOnSessionStart) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  const base::FilePath orphan_dir =
      profile()->GetPath().Append(kIwaDirName).AppendASCII("orphan_bundle_dir");
  ASSERT_TRUE(base::CreateDirectory(orphan_dir));
  ASSERT_TRUE(base::PathExists(orphan_dir));

  test::AwaitStartWebAppProviderAndSubsystems(profile());
  task_environment().FastForwardBy(base::Minutes(10));
  provider().command_manager().AwaitAllCommandsCompleteForTesting();

  EXPECT_FALSE(base::PathExists(orphan_dir));
}

class IsolatedWebAppPolicyManagerOnDemandUpdateDownloadedTest
    : public IsolatedWebAppTest {
 public:
  IsolatedWebAppPolicyManagerOnDemandUpdateDownloadedTest()
      : IsolatedWebAppTest(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
  }

  MockOnDemandUpdater& on_demand_updater() {
    return component_wrapper_->on_demand_updater();
  }

  void InstallComponentAsync(const base::Version& version,
                             const IwaKeyDistribution& component_data) {
    component_wrapper_->InstallComponentAsync(version, component_data,
                                              /*is_preloaded=*/false);
  }

 protected:
  void SetUp() override {
    IsolatedWebAppTest::SetUp();
    IwaKeyDistributionInfoProvider::GetInstanceForTesting()
        .SkipManagedAllowlistChecksForTesting(true);
    component_wrapper_ = std::make_unique<IwaComponentWrapper>();
    component_wrapper_->InstallComponentAsync(base::Version("1.0.0"),
                                              /*component_data=*/{},
                                              IsIwaComponentPreloaded());
  }

  void TearDown() override {
    IsolatedWebAppTest::TearDown();
    IwaKeyDistributionInfoProvider::DestroyInstanceForTesting();
  }

  IsolatedWebAppUrlInfo url_info() const {
    return IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
        test::GetDefaultEd25519WebBundleId());
  }

  void SetUpForceInstallPolicyForOneApp() {
    test::AddForceInstalledIwaToPolicy(
        profile()->GetPrefs(),
        IwaTestServerConfigurator::CreateForceInstallPolicyEntry(
            url_info().web_bundle_id()));
  }

 private:
  virtual bool IsIwaComponentPreloaded() const { return false; }

  std::unique_ptr<IwaComponentWrapper> component_wrapper_;

  base::ScopedPathOverride preinstalled_dir_override_{
      component_updater::DIR_COMPONENT_PREINSTALLED};
};

class IsolatedWebAppPolicyManagerOnDemandUpdatePreloadedTest
    : public IsolatedWebAppPolicyManagerOnDemandUpdateDownloadedTest {
 protected:
  // `IsolatedWebAppPolicyManagerOnDemandUpdateDownloadedTest`:
  bool IsIwaComponentPreloaded() const override { return true; }
};

// The on-demand is dispatched, but without success. In this case the policy
// processing will take place in 15 seconds.
TEST_F(IsolatedWebAppPolicyManagerOnDemandUpdatePreloadedTest,
       ComponentUpdateQueuedButNoUpdate) {
  EXPECT_CALL(on_demand_updater(),
              OnDemandUpdate(Eq(kIwaKeyDistributionComponentId), _, _))
      .Times(1);

  SetUpForceInstallPolicyForOneApp();
  test::AwaitStartWebAppProviderAndSubsystems(profile());
  {
    auto bundle = IsolatedWebAppBuilder(ManifestBuilder())
                      .BuildBundle(test::GetDefaultEd25519KeyPair());
    bundle->FakeInstallPageState(profile());
    test_update_server().AddBundle(std::move(bundle));
  }

  provider().command_manager().AwaitAllCommandsCompleteForTesting();
  EXPECT_EQ(0u, provider().registrar_unsafe().GetAppIds().size());

  task_environment().FastForwardBy(base::Seconds(5));
  provider().command_manager().AwaitAllCommandsCompleteForTesting();
  EXPECT_EQ(0u, provider().registrar_unsafe().GetAppIds().size());

  task_environment().FastForwardBy(base::Seconds(10));
  provider().command_manager().AwaitAllCommandsCompleteForTesting();
  EXPECT_EQ(1u, provider().registrar_unsafe().GetAppIds().size());
}

// The on-demand is dispatched successfully -- the policy reprocessing is
// triggered upon successful update.
TEST_F(IsolatedWebAppPolicyManagerOnDemandUpdatePreloadedTest,
       ComponentUpdateQueuedSuccessfully) {
  EXPECT_CALL(on_demand_updater(),
              OnDemandUpdate(Eq(kIwaKeyDistributionComponentId), _, _))
      .Times(1)
      .WillOnce(WithoutArgs([&] {
        InstallComponentAsync(base::Version("1.0.1"), IwaKeyDistribution());
      }));

  SetUpForceInstallPolicyForOneApp();
  test::AwaitStartWebAppProviderAndSubsystems(profile());
  {
    auto bundle = IsolatedWebAppBuilder(ManifestBuilder())
                      .BuildBundle(test::GetDefaultEd25519KeyPair());
    bundle->FakeInstallPageState(profile());
    test_update_server().AddBundle(std::move(bundle));
  }

  provider().command_manager().AwaitAllCommandsCompleteForTesting();
  EXPECT_EQ(0u, provider().registrar_unsafe().GetAppIds().size());

  // Wait for the policy reprocessing triggered by the component installation.
  WebAppTestInstallObserver(profile()).BeginListeningAndWait(
      {url_info().app_id()});

  provider().command_manager().AwaitAllCommandsCompleteForTesting();
  EXPECT_EQ(1u, provider().registrar_unsafe().GetAppIds().size());
}

// The on-demand must not be triggered if a non-preloaded version is present.
TEST_F(IsolatedWebAppPolicyManagerOnDemandUpdateDownloadedTest,
       ComponentUpdateNotQueuedWhenComponentIsAlreadyLoaded) {
  EXPECT_CALL(on_demand_updater(),
              OnDemandUpdate(Eq(kIwaKeyDistributionComponentId), _, _))
      .Times(0);

  SetUpForceInstallPolicyForOneApp();
  test::AwaitStartWebAppProviderAndSubsystems(profile());
  {
    auto bundle = IsolatedWebAppBuilder(ManifestBuilder())
                      .BuildBundle(test::GetDefaultEd25519KeyPair());
    bundle->FakeInstallPageState(profile());
    test_update_server().AddBundle(std::move(bundle));
  }

  WebAppTestInstallObserver(profile()).BeginListeningAndWait(
      {url_info().app_id()});

  provider().command_manager().AwaitAllCommandsCompleteForTesting();
  EXPECT_EQ(1u, provider().registrar_unsafe().GetAppIds().size());
}

// The on-demand must not be triggered if the policy is empty.
TEST_F(IsolatedWebAppPolicyManagerOnDemandUpdatePreloadedTest,
       ComponentUpdateNotQueuedWhenPolicyEmpty) {
  EXPECT_CALL(on_demand_updater(),
              OnDemandUpdate(Eq(kIwaKeyDistributionComponentId), _, _))
      .Times(0);

  test::AwaitStartWebAppProviderAndSubsystems(profile());

  provider().command_manager().AwaitAllCommandsCompleteForTesting();
  EXPECT_EQ(0u, provider().registrar_unsafe().GetAppIds().size());
}

TEST_F(IsolatedWebAppPolicyManagerOnDemandUpdatePreloadedTest,
       ComponentUpdateTriggeredWhenEmptyPolicyChangesToNonEmpty) {
  // The on-demand update must not be triggered during the initial policy
  // processing.
  EXPECT_CALL(on_demand_updater(), OnDemandUpdate).Times(0);

  test::AwaitStartWebAppProviderAndSubsystems(profile());
  provider().command_manager().AwaitAllCommandsCompleteForTesting();

  testing::Mock::VerifyAndClearExpectations(&on_demand_updater());

  // The on-demand update will now be triggered once the policy changes.
  EXPECT_CALL(on_demand_updater(),
              OnDemandUpdate(Eq(kIwaKeyDistributionComponentId), _, _))
      .Times(1)
      .WillOnce(WithoutArgs([&] {
        InstallComponentAsync(base::Version("1.0.1"), IwaKeyDistribution());
      }));

  {
    auto bundle = IsolatedWebAppBuilder(ManifestBuilder())
                      .BuildBundle(test::GetDefaultEd25519KeyPair());
    bundle->FakeInstallPageState(profile());
    test_update_server().AddBundle(std::move(bundle));
  }
  SetUpForceInstallPolicyForOneApp();

  // Wait for the policy reprocessing triggered by the component installation.
  WebAppTestInstallObserver(profile()).BeginListeningAndWait(
      {url_info().app_id()});

  provider().command_manager().AwaitAllCommandsCompleteForTesting();
  EXPECT_EQ(1u, provider().registrar_unsafe().GetAppIds().size());
}

}  // namespace web_app
