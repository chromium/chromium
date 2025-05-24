// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_policy_manager.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/check_deref.h"
#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_writer.h"
#include "base/run_loop.h"
#include "base/strings/to_string.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/repeating_test_future.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/component_updater/iwa_key_distribution_component_installer.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/commands/cleanup_orphaned_isolated_web_apps_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/commands/install_isolated_web_app_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_install_source.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_storage_location.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_update_discovery_task.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_update_manager.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolation_data.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_test.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/iwa_test_server_configurator.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/key_distribution/test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/mock_isolated_web_app_install_command_wrapper.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/policy_generator.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/policy_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/test_iwa_installer_factory.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/test_signed_web_bundle_builder.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/fake_web_contents_manager.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_management_type.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/component_updater/component_updater_service.h"
#include "components/component_updater/mock_component_updater_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/user_manager/user.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/web_package/test_support/signed_web_bundles/ed25519_key_pair.h"
#include "components/webapps/common/web_app_id.h"
#include "components/webapps/isolated_web_apps/features.h"
#include "components/webapps/isolated_web_apps/iwa_key_distribution_info_provider.h"
#include "components/webapps/isolated_web_apps/proto/key_distribution.pb.h"
#include "content/public/common/content_features.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {

namespace {

using testing::_;
using ::testing::Eq;
using ::testing::IsNull;
using ::testing::NotNull;
using ::testing::UnorderedElementsAre;

class TestOrphanedCleanupWebAppCommandScheduler
    : public WebAppCommandScheduler {
 public:
  explicit TestOrphanedCleanupWebAppCommandScheduler(Profile& profile)
      : WebAppCommandScheduler(profile) {}

  void CleanupOrphanedIsolatedApps(
      CleanupOrphanedIsolatedWebAppsCallback callback,
      const base::Location& call_location) override {
    ++number_of_calls_;
    std::move(callback).Run(CleanupOrphanedIsolatedWebAppsCommandSuccess(0u));
    command_done_closure_.Run();
  }

  size_t GetNumberOfCalls() { return number_of_calls_; }

  void SetCommandDoneClosure(base::RepeatingClosure closure) {
    command_done_closure_ = std::move(closure);
  }

 private:
  base::RepeatingClosure command_done_closure_;
  size_t number_of_calls_ = 0;
};

}  // namespace

class IsolatedWebAppPolicyManagerTestBase : public IsolatedWebAppTest {
 public:
  explicit IsolatedWebAppPolicyManagerTestBase(
      bool is_mgs_session_install_enabled,
      bool is_user_session,
      base::test::TaskEnvironment::TimeSource time_source =
          base::test::TaskEnvironment::TimeSource::MOCK_TIME)
      : IsolatedWebAppTest(time_source, WithDevMode{}),
        is_mgs_session_install_enabled_(is_mgs_session_install_enabled),
        is_user_session_(is_user_session) {
#if BUILDFLAG(IS_CHROMEOS)
    if (is_mgs_session_install_enabled_) {
      scoped_feature_list_.InitAndEnableFeature(
          features::kIsolatedWebAppManagedGuestSessionInstall);
    }
#endif  // BUILDFLAG(IS_CHROMEOS)
  }

  void SetUpServedIwas() {
    std::unique_ptr<ScopedBundledIsolatedWebApp> app1 =
        IsolatedWebAppBuilder(ManifestBuilder().SetVersion("1.0.0"))
            .BuildBundle(test::GetDefaultEd25519KeyPair());
    app1->FakeInstallPageState(profile());

    std::unique_ptr<ScopedBundledIsolatedWebApp> app2 =
        IsolatedWebAppBuilder(ManifestBuilder().SetVersion("1.0.0"))
            .BuildBundle();
    app2->FakeInstallPageState(profile());

    lazy_app1_id_ = app1->web_bundle_id();
    lazy_app2_id_ = app2->web_bundle_id();

    test_update_server().AddBundle(std::move(app1));
    test_update_server().AddBundle(std::move(app2));
  }

  void SetUp() override {
    IsolatedWebAppTest::SetUp();
    SetCommandScheduler();

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

  virtual void SetCommandScheduler() = 0;
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

  bool IsManagedGuestSessionInstallEnabled() {
    return is_mgs_session_install_enabled_;
  }

  const web_package::SignedWebBundleId& web_bundle_id_1() {
    return *lazy_app1_id_;
  }
  const web_package::SignedWebBundleId& web_bundle_id_2() {
    return *lazy_app2_id_;
  }

 private:
  const bool is_mgs_session_install_enabled_;
  const bool is_user_session_;
  base::test::ScopedFeatureList scoped_feature_list_;
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
      : IsolatedWebAppPolicyManagerTestBase(
            /*is_mgs_session_install_enabled=*/false,
            /*is_user_session=*/true) {}

  // `IsolatedWebAppPolicyManagerTestBase`:
  void SetCommandScheduler() override {
    // For these tests we are fine with regular command scheduler.
  }
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
  const base::Version pinned_version = base::Version("1.0.0");
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
  const base::Version pinned_version = base::Version("1.9.0");
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

  ASSERT_NE(provider().registrar_unsafe().GetInstallState(url_info.app_id()),
            proto::InstallState::INSTALLED_WITH_OS_INTEGRATION);
}

TEST_F(IsolatedWebAppPolicyManagerTest,
       AppInstalledWhenPreviouslyUserInstalled) {
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
  }

  WebAppTestUninstallObserver uninstall_observer(profile());
  uninstall_observer.BeginListening({url_info.app_id()});
  WebAppTestInstallObserver install_observer(profile());
  install_observer.BeginListening({url_info.app_id()});

  test::AddForceInstalledIwaToPolicy(
      profile()->GetPrefs(),
      IwaTestServerConfigurator::CreateForceInstallPolicyEntry(
          url_info.web_bundle_id()));

  // Apps should be fully uninstalled before they can be force-installed.
  EXPECT_EQ(uninstall_observer.Wait(), url_info.app_id());
  EXPECT_EQ(install_observer.Wait(), url_info.app_id());
  task_environment().RunUntilIdle();

  const WebApp* web_app =
      provider().registrar_unsafe().GetAppById(url_info.app_id());
  ASSERT_THAT(web_app, NotNull());
  EXPECT_THAT(web_app->GetSources(),
              Eq(WebAppManagementTypes({WebAppManagement::Type::kIwaPolicy})));
}

TEST_F(IsolatedWebAppPolicyManagerTest,
       AppInstalledWhenPreviouslyDevInstalled) {
  const std::unique_ptr<ScopedBundledIsolatedWebApp> bundle =
      IsolatedWebAppBuilder(ManifestBuilder().SetVersion("1.0.0"))
          .BuildBundle(test::GetDefaultEd25519KeyPair());

  ASSERT_OK_AND_ASSIGN(const IsolatedWebAppUrlInfo url_info,
                       bundle->InstallWithSource(
                           profile(), &IsolatedWebAppInstallSource::FromDevUi));

  WebAppTestUninstallObserver uninstall_observer(profile());
  uninstall_observer.BeginListening({url_info.app_id()});
  WebAppTestInstallObserver install_observer(profile());
  install_observer.BeginListening({url_info.app_id()});

  test::AddForceInstalledIwaToPolicy(
      profile()->GetPrefs(),
      IwaTestServerConfigurator::CreateForceInstallPolicyEntry(
          url_info.web_bundle_id()));

  // Apps should be fully uninstalled before they can be force-installed.
  EXPECT_EQ(uninstall_observer.Wait(), url_info.app_id());
  EXPECT_EQ(install_observer.Wait(), url_info.app_id());
  task_environment().RunUntilIdle();

  const WebApp* web_app =
      provider().registrar_unsafe().GetAppById(url_info.app_id());
  ASSERT_THAT(web_app, NotNull());
  EXPECT_THAT(web_app->GetSources(),
              Eq(WebAppManagementTypes({WebAppManagement::Type::kIwaPolicy})));
}

#if BUILDFLAG(IS_CHROMEOS)
class ManagedGuestSessionInstallFlagTest
    : public IsolatedWebAppPolicyManagerTestBase,
      public testing::WithParamInterface<bool> {
 public:
  ManagedGuestSessionInstallFlagTest()
      : IsolatedWebAppPolicyManagerTestBase(
            /*is_mgs_session_install_enabled=*/GetParam(),
            /*is_user_session=*/false) {}

  // `IsolatedWebAppPolicyManagerTestBase`:
  void SetCommandScheduler() override {
    // For these tests we are fine with regular command scheduler.
  }
};

TEST_P(ManagedGuestSessionInstallFlagTest, AppInstalledIfFlagEnabled) {
  auto url_info =
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(web_bundle_id_1());

  test::AddForceInstalledIwaToPolicy(
      profile()->GetPrefs(),
      IwaTestServerConfigurator::CreateForceInstallPolicyEntry(
          url_info.web_bundle_id()));

  provider().command_manager().AwaitAllCommandsCompleteForTesting();
  task_environment().RunUntilIdle();

  const WebApp* web_app =
      provider().registrar_unsafe().GetAppById(url_info.app_id());
  if (IsManagedGuestSessionInstallEnabled()) {
    ASSERT_THAT(web_app, NotNull());
    EXPECT_THAT(
        web_app->GetSources(),
        Eq(WebAppManagementTypes({WebAppManagement::Type::kIwaPolicy})));
  } else {
    ASSERT_THAT(web_app, IsNull());
  }
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    ManagedGuestSessionInstallFlagTest,
    // Determines whether managed guest session install is enabled.
    testing::Bool());

#endif  // BUILDFLAG(IS_CHROMEOS)

class IsolatedWebAppManagedAllowlistTest
    : public IsolatedWebAppPolicyManagerTestBase {
 public:
  IsolatedWebAppManagedAllowlistTest()
      : IsolatedWebAppPolicyManagerTestBase(
            /*is_mgs_session_install_enabled=*/false,
            /*is_user_session=*/true) {}

  // `IsolatedWebAppPolicyManagerTestBase`:
  void SetCommandScheduler() override {
    // For these tests we are fine with regular command scheduler.
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kIsolatedWebAppManagedAllowlist};
};
using base::test::HasValue;

TEST_F(IsolatedWebAppManagedAllowlistTest, AllowedAppInstalled) {
  const auto url_info =
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(web_bundle_id_1());

  WebAppTestInstallObserver install_observer(profile());
  install_observer.BeginListening({url_info.app_id()});

  base::test::TestFuture<web_package::SignedWebBundleId, IwaInstallerResult>
      future;
  IsolatedWebAppPolicyManager::SetOnInstallTaskCompletedCallbackForTesting(
      future.GetRepeatingCallback());

  // Update allowlist
  EXPECT_THAT(test::UpdateKeyDistributionInfoWithAllowlist(
                  base::Version("1.0.1"),
                  /*managed_allowlist=*/{web_bundle_id_1().id()}),
              HasValue());

  EXPECT_TRUE(
      IwaKeyDistributionInfoProvider::GetInstance()->IsManagedInstallPermitted(
          web_bundle_id_1().id()));

  test::AddForceInstalledIwaToPolicy(
      profile()->GetPrefs(),
      IwaTestServerConfigurator::CreateForceInstallPolicyEntry(
          url_info.web_bundle_id()));

  auto [web_bundle_id, result] = future.Take();
  EXPECT_EQ(web_bundle_id, web_bundle_id_1());
  EXPECT_EQ(result.type(), IwaInstallerResultType::kSuccess);

  EXPECT_EQ(install_observer.Wait(), url_info.app_id());

  const WebApp* web_app =
      provider().registrar_unsafe().GetAppById(url_info.app_id());
  ASSERT_THAT(web_app, NotNull());
  EXPECT_THAT(web_app->GetSources(),
              Eq(WebAppManagementTypes({WebAppManagement::Type::kIwaPolicy})));
}

TEST_F(IsolatedWebAppManagedAllowlistTest, NotAllowedAppInstallationRefused) {
  auto url_info =
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(web_bundle_id_1());

  base::test::TestFuture<web_package::SignedWebBundleId, IwaInstallerResult>
      future;
  IsolatedWebAppPolicyManager::SetOnInstallTaskCompletedCallbackForTesting(
      future.GetRepeatingCallback());

  // Ensure allowlist is empty
  EXPECT_THAT(
      test::UpdateKeyDistributionInfoWithAllowlist(base::Version("1.0.1"),
                                                   /*managed_allowlist=*/{}),
      HasValue());

  EXPECT_FALSE(
      IwaKeyDistributionInfoProvider::GetInstance()->IsManagedInstallPermitted(
          web_bundle_id_1().id()));

  test::AddForceInstalledIwaToPolicy(
      profile()->GetPrefs(),
      IwaTestServerConfigurator::CreateForceInstallPolicyEntry(
          url_info.web_bundle_id()));

  auto [web_bundle_id, result] = future.Take();
  EXPECT_EQ(web_bundle_id, web_bundle_id_1());
  EXPECT_EQ(result.type(), IwaInstallerResultType::kErrorAppNotInAllowlist);

  const WebApp* web_app =
      provider().registrar_unsafe().GetAppById(url_info.app_id());
  EXPECT_THAT(web_app, testing::IsNull());
}

// This implementation of the command scheduler can't install an IWA. Instead
// it hangs and waits for the signal to signalize the
// invoker that the install failed.
class TestWebAppCommandScheduler : public WebAppCommandScheduler {
 public:
  using WebAppCommandScheduler::WebAppCommandScheduler;

  void InstallIsolatedWebApp(
      const IsolatedWebAppUrlInfo& url_info,
      const IsolatedWebAppInstallSource& install_source,
      const std::optional<base::Version>& expected_version,
      std::unique_ptr<ScopedKeepAlive> keep_alive,
      std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive,
      InstallIsolatedWebAppCallback callback,
      const base::Location& call_location) override {
    EXPECT_TRUE(stashed_callback_.is_null());
    EXPECT_EQ(install_source.install_surface(),
              webapps::WebappInstallSource::IWA_EXTERNAL_POLICY);
    id_ = url_info.web_bundle_id();
    stashed_callback_ = std::move(callback);
  }

  web_package::SignedWebBundleId FinishWithError() {
    std::move(stashed_callback_)
        .Run(base::unexpected<InstallIsolatedWebAppCommandError>(
            InstallIsolatedWebAppCommandError{
                .message = "Just test error. We even didn't try..."}));
    return id_.value();
  }

 private:
  InstallIsolatedWebAppCallback stashed_callback_;
  std::optional<web_package::SignedWebBundleId> id_;
};

template <typename T>
class IsolatedWebAppPolicyManagerCustomSchedulerTest
    : public IsolatedWebAppPolicyManagerTestBase {
 public:
  IsolatedWebAppPolicyManagerCustomSchedulerTest()
      : IsolatedWebAppPolicyManagerTestBase(
            /*is_mgs_session_install_enabled=*/false,
            /*is_user_session=*/true) {}

  T* get_command_scheduler() { return scheduler_; }
  // `IsolatedWebAppPolicyManagerTestBase`:
  void SetCommandScheduler() override {
    std::unique_ptr<T> scheduler = std::make_unique<T>(*profile());
    scheduler_ = scheduler.get();
    provider().SetScheduler(std::move(scheduler));
  }

  void TearDown() override {
    scheduler_ = nullptr;
    IsolatedWebAppPolicyManagerTestBase::TearDown();
  }

 private:
  raw_ptr<T> scheduler_;
};

using IsolatedWebAppPolicyManagerPolicyRaceTest =
    IsolatedWebAppPolicyManagerCustomSchedulerTest<TestWebAppCommandScheduler>;

// Verifies that the updating of policy during previous policy processing
// is handled correctly.
TEST_F(IsolatedWebAppPolicyManagerPolicyRaceTest,
       PolicyUpdateWhileInstallInProgress) {
  {
    test::AddForceInstalledIwaToPolicy(
        profile()->GetPrefs(),
        IwaTestServerConfigurator::CreateForceInstallPolicyEntry(
            web_bundle_id_1()));
  }

  task_environment().RunUntilIdle();

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
    policy_generator.AddForceInstalledIwa(
        id, GURL("https://update/manifest/does/not/exist"));
    profile()->GetPrefs()->Set(prefs::kIsolatedWebAppInstallForceList,
                               policy_generator.Generate());
  }

  task_environment().RunUntilIdle();

  // The third policy update. This one must be processed.
  {
    profile()->GetPrefs()->SetList(
        prefs::kIsolatedWebAppInstallForceList,
        base::Value::List()
            .Append(IwaTestServerConfigurator::CreateForceInstallPolicyEntry(
                web_bundle_id_1()))
            .Append(IwaTestServerConfigurator::CreateForceInstallPolicyEntry(
                web_bundle_id_2())));
  }

  // Finish the installation of the app1 from the first policy update.
  EXPECT_THAT(get_command_scheduler()->FinishWithError(),
              Eq(web_bundle_id_1()));
  task_environment().RunUntilIdle();

  // The second policy update is ignored as it was replaced by the third one.

  // Processing the third policy update.
  std::vector<web_package::SignedWebBundleId> ids;

  // Finish app1 from the third policy update.
  ids.push_back(get_command_scheduler()->FinishWithError());
  task_environment().RunUntilIdle();

  // Finish app2 from the third policy update.
  ids.push_back(get_command_scheduler()->FinishWithError());
  task_environment().RunUntilIdle();

  EXPECT_THAT(ids, UnorderedElementsAre(web_bundle_id_1(), web_bundle_id_2()));
}

// This scheduler is intercepting scheduling of the uninstall command,
// verifying if the App ID is expected for removal.
class UninstallWebAppCommandScheduler : public WebAppCommandScheduler {
 public:
  using WebAppCommandScheduler::WebAppCommandScheduler;

  void RemoveInstallManagementMaybeUninstall(
      const webapps::AppId& app_id,
      WebAppManagement::Type management_type,
      webapps::WebappUninstallSource uninstall_source,
      UninstallCallback callback,
      const base::Location& location) override {
    tried_to_uninstall_ = true;
    EXPECT_TRUE(base::Contains(expected_apps_to_remove_, app_id));
    EXPECT_EQ(management_type, expected_management_type_to_remove_.value_or(
                                   WebAppManagement::Type::kIwaPolicy));
    EXPECT_EQ(uninstall_source,
              webapps::WebappUninstallSource::kIwaEnterprisePolicy);
    auto app = expected_apps_to_remove_.find(app_id);
    expected_apps_to_remove_.erase(app);

    WebAppCommandScheduler::RemoveInstallManagementMaybeUninstall(
        app_id, management_type, uninstall_source, std::move(callback),
        location);
  }

  void AddExpectedToUninstallApp(const webapps::AppId& app_id) {
    expected_apps_to_remove_.insert(app_id);
  }

  size_t GetNumberOfAppsRemainingToUninstall() const {
    return expected_apps_to_remove_.size();
  }

  bool TriedToUninstall() { return tried_to_uninstall_; }

  void SetMaybeExpectedManagementTypeToUninstall(
      std::optional<WebAppManagement::Type> management_type) {
    expected_management_type_to_remove_ = management_type;
  }

 private:
  base::flat_set<webapps::AppId> expected_apps_to_remove_;
  std::optional<WebAppManagement::Type> expected_management_type_to_remove_;
  bool tried_to_uninstall_ = false;
};

using IsolatedWebAppPolicyManagerUninstallTest =
    IsolatedWebAppPolicyManagerCustomSchedulerTest<
        UninstallWebAppCommandScheduler>;

// Remove the app from policy and check if there will be attempt to uninstall
// that app.
TEST_F(IsolatedWebAppPolicyManagerUninstallTest, OneAppUninstalled) {
  // Force install 2 apps.
  {
    profile()->GetPrefs()->SetList(
        prefs::kIsolatedWebAppInstallForceList,
        base::Value::List()
            .Append(IwaTestServerConfigurator::CreateForceInstallPolicyEntry(
                web_bundle_id_1()))
            .Append(IwaTestServerConfigurator::CreateForceInstallPolicyEntry(
                web_bundle_id_2())));

    task_environment().RunUntilIdle();

    AssertAppInstalled(web_bundle_id_1());
    AssertAppInstalled(web_bundle_id_2());
  }

  // Now generate a policy with 1 app and expect an attempt to
  // remove the other app.
  {
    const webapps::AppId app2_id =
        IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(web_bundle_id_2())
            .app_id();
    get_command_scheduler()->AddExpectedToUninstallApp(app2_id);
    EXPECT_EQ(get_command_scheduler()->GetNumberOfAppsRemainingToUninstall(),
              1U);

    profile()->GetPrefs()->SetList(
        prefs::kIsolatedWebAppInstallForceList,
        base::Value::List().Append(
            IwaTestServerConfigurator::CreateForceInstallPolicyEntry(
                web_bundle_id_1())));

    task_environment().RunUntilIdle();

    EXPECT_EQ(get_command_scheduler()->GetNumberOfAppsRemainingToUninstall(),
              0U);
  }
}

TEST_F(IsolatedWebAppPolicyManagerUninstallTest, BothAppUninstalled) {
  // Force install 2 apps.
  {
    profile()->GetPrefs()->SetList(
        prefs::kIsolatedWebAppInstallForceList,
        base::Value::List()
            .Append(IwaTestServerConfigurator::CreateForceInstallPolicyEntry(
                web_bundle_id_1()))
            .Append(IwaTestServerConfigurator::CreateForceInstallPolicyEntry(
                web_bundle_id_2())));

    task_environment().RunUntilIdle();

    AssertAppInstalled(web_bundle_id_1());
    AssertAppInstalled(web_bundle_id_2());
  }

  // Set the policy without any app and expect an attempt to uninstall
  // both previously installed apps.
  {
    const webapps::AppId app1_id =
        IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(web_bundle_id_1())
            .app_id();
    const webapps::AppId app2_id =
        IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(web_bundle_id_2())
            .app_id();

    WebAppTestUninstallObserver uninstall_observer(profile());
    uninstall_observer.BeginListening({app1_id, app2_id});

    get_command_scheduler()->AddExpectedToUninstallApp(app1_id);
    get_command_scheduler()->AddExpectedToUninstallApp(app2_id);
    EXPECT_EQ(get_command_scheduler()->GetNumberOfAppsRemainingToUninstall(),
              2U);

    profile()->GetPrefs()->SetList(prefs::kIsolatedWebAppInstallForceList,
                                   base::Value::List());

    uninstall_observer.Wait();

    // WebAppTestUninstallObserver already triggers when the app is not fully
    // uninstalled. This causes issues with references to destroyed profiles
    // (see https://crbug.com/41484323#comment7). Wait until the app is actually
    // uninstalled here.
    task_environment().RunUntilIdle();

    EXPECT_EQ(get_command_scheduler()->GetNumberOfAppsRemainingToUninstall(),
              0U);
  }
}

TEST_F(IsolatedWebAppPolicyManagerUninstallTest,
       UserInstalledAppUninstalledAsWell) {
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
    get_command_scheduler()->AddExpectedToUninstallApp(url_info.app_id());
    get_command_scheduler()->SetMaybeExpectedManagementTypeToUninstall(
        WebAppManagement::Type::kIwaUserInstalled);

    WebAppTestUninstallObserver uninstall_observer(profile());
    uninstall_observer.BeginListening({url_info.app_id()});

    profile()->GetPrefs()->SetList(
        prefs::kIsolatedWebAppInstallForceList,
        base::Value::List().Append(
            IwaTestServerConfigurator::CreateForceInstallPolicyEntry(
                web_bundle_id_1())));

    uninstall_observer.Wait();

    // WebAppTestUninstallObserver already triggers when the app is not fully
    // uninstalled. This causes issues with references to destroyed profiles
    // (see https://crbug.com/41484323#comment7). Wait until the app is actually
    // uninstalled here.
    task_environment().RunUntilIdle();

    const WebApp* web_app =
        provider().registrar_unsafe().GetAppById(url_info.app_id());
    ASSERT_THAT(web_app, NotNull());
    EXPECT_THAT(
        web_app->GetSources(),
        Eq(WebAppManagementTypes({WebAppManagement::Type::kIwaPolicy})));

    get_command_scheduler()->SetMaybeExpectedManagementTypeToUninstall(
        std::nullopt);
  }

  // Set the policy without any app and expect an attempt to remove the policy
  // install source.
  {
    get_command_scheduler()->AddExpectedToUninstallApp(url_info.app_id());
    EXPECT_EQ(get_command_scheduler()->GetNumberOfAppsRemainingToUninstall(),
              1U);

    WebAppTestUninstallObserver uninstall_observer(profile());
    uninstall_observer.BeginListening({url_info.app_id()});

    profile()->GetPrefs()->SetList(prefs::kIsolatedWebAppInstallForceList,
                                   base::Value::List());

    uninstall_observer.Wait();

    // WebAppTestUninstallObserver already triggers when the app is not fully
    // uninstalled. This causes issues with references to destroyed profiles
    // (see https://crbug.com/41484323#comment7). Wait until the app is actually
    // uninstalled here.
    task_environment().RunUntilIdle();

    EXPECT_EQ(get_command_scheduler()->GetNumberOfAppsRemainingToUninstall(),
              0U);

    const WebApp* web_app =
        provider().registrar_unsafe().GetAppById(url_info.app_id());
    EXPECT_THAT(web_app, IsNull());
  }
}

// There should not be any attempt to uninstall an app if no apps have been
// removed from the apps.
TEST_F(IsolatedWebAppPolicyManagerUninstallTest, NoAppsUninstalled) {
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

  AssertAppInstalled(web_bundle_id_1());
  AssertAppInstalled(web_bundle_id_2());
  EXPECT_FALSE(get_command_scheduler()->TriedToUninstall());
}

class IsolatedWebAppRetryTest : public IsolatedWebAppPolicyManagerTestBase {
 public:
  IsolatedWebAppRetryTest()
      : IsolatedWebAppPolicyManagerTestBase(
            /*is_mgs_session_install_enabled=*/false,
            /*is_user_session=*/true,
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    features_.InitAndDisableFeature(kIwaPolicyManagerOnDemandComponentUpdate);
  }

 protected:
  TestIwaInstallerFactory iwa_installer_factory_;

 private:
  void SetUp() override {
    IsolatedWebAppPolicyManagerTestBase::SetUp();
    iwa_installer_factory_.SetUp(profile());
  }

  // `IsolatedWebAppPolicyManagerTestBase`:
  void SetCommandScheduler() override {
    // For these tests we are fine with the regular command scheduler.
  }

  base::test::ScopedFeatureList features_;
};

TEST_F(IsolatedWebAppRetryTest, FirstInstallFailsRetrySucceeds) {
  auto url_info =
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(web_bundle_id_1());
  iwa_installer_factory_.SetCommandBehavior(
      url_info.web_bundle_id().id(),
      /*execution_mode=*/
      MockIsolatedWebAppInstallCommandWrapper::ExecutionMode::kSimulateFailure,
      /*execute_immediately=*/true);

  test::AddForceInstalledIwaToPolicy(
      profile()->GetPrefs(),
      IwaTestServerConfigurator::CreateForceInstallPolicyEntry(
          web_bundle_id_1()));

  // Run the first attempt to install the isolated web app (which should fail).
  task_environment().FastForwardBy(base::TimeDelta(base::Seconds(1)));

  ASSERT_EQ(1u, iwa_installer_factory_.GetNumberOfCreatedInstallTasks());
  const WebApp* web_app_t0 =
      provider().registrar_unsafe().GetAppById(url_info.app_id());
  ASSERT_THAT(web_app_t0, IsNull());

  // Fast forward right before the retry should happen --> retry to process the
  // policy is still scheduled, but the isolated web app is not yet installed.
  iwa_installer_factory_.SetCommandBehavior(
      url_info.web_bundle_id().id(),
      /*execution_mode=*/
      MockIsolatedWebAppInstallCommandWrapper::ExecutionMode::kRunCommand,
      /*execute_immediately=*/true);
  task_environment().FastForwardBy(base::TimeDelta(base::Seconds(58)));

  const WebApp* web_app_t1 =
      provider().registrar_unsafe().GetAppById(url_info.app_id());
  ASSERT_THAT(web_app_t1, IsNull());

  WebAppTestInstallObserver install_observer(profile());
  install_observer.BeginListening({url_info.app_id()});

  // Fast forward another second and the app should be installed.
  task_environment().FastForwardBy(base::TimeDelta(base::Seconds(1)));

  ASSERT_EQ(2u, iwa_installer_factory_.GetNumberOfCreatedInstallTasks());

  // Make sure that even if there are further install tasks scheduled, they are
  // failing and therefore do not accidentally make this test pass.
  iwa_installer_factory_.SetCommandBehavior(
      url_info.web_bundle_id().id(),
      /*execution_mode=*/
      MockIsolatedWebAppInstallCommandWrapper::ExecutionMode::kSimulateFailure,
      /*execute_immediately=*/true);

  EXPECT_EQ(install_observer.Wait(), url_info.app_id());

  ASSERT_EQ(2u, iwa_installer_factory_.GetNumberOfCreatedInstallTasks());
  const WebApp* web_app_t2 =
      provider().registrar_unsafe().GetAppById(url_info.app_id());
  ASSERT_THAT(web_app_t2, NotNull());
  EXPECT_THAT(web_app_t2->GetSources(),
              Eq(WebAppManagementTypes({WebAppManagement::Type::kIwaPolicy})));
}

TEST_F(IsolatedWebAppRetryTest, RetryTimeStepsCorrect) {
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
    iwa_installer_factory_.SetCommandBehavior(
        url_info.web_bundle_id().id(),
        /*execution_mode=*/
        MockIsolatedWebAppInstallCommandWrapper::ExecutionMode::
            kSimulateFailure,
        /*execute_immediately=*/true);

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

      ASSERT_EQ(expected_number_install_tasks,
                iwa_installer_factory_.GetNumberOfCreatedInstallTasks());
      const WebApp* web_app_t0 =
          provider().registrar_unsafe().GetAppById(url_info.app_id());
      ASSERT_THAT(web_app_t0, IsNull());

      // Fast forward right before the retry should happen --> retry to process
      // the policy is still scheduled, but the install task is not yet created.
      task_environment().FastForwardBy(base::TimeDelta(
          base::Seconds(next_time_step - current_time_step - 2)));

      const WebApp* web_app_t1 =
          provider().registrar_unsafe().GetAppById(url_info.app_id());
      ASSERT_THAT(web_app_t1, IsNull());

      // Fast forward another second and the next retry should happen.
      task_environment().FastForwardBy(base::TimeDelta(base::Seconds(1)));
      ASSERT_EQ(++expected_number_install_tasks,
                iwa_installer_factory_.GetNumberOfCreatedInstallTasks());
    }

    WebAppTestInstallObserver install_observer(profile());
    install_observer.BeginListening({url_info.app_id()});

    // Finally make the installation work. This should reset the delay for the
    // next install.
    iwa_installer_factory_.SetCommandBehavior(
        url_info.web_bundle_id().id(),
        /*execution_mode=*/
        MockIsolatedWebAppInstallCommandWrapper::ExecutionMode::kRunCommand,
        /*execute_immediately=*/true);
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

  profile()->GetPrefs()->SetList(
      prefs::kIsolatedWebAppInstallForceList,
      base::Value::List()
          .Append(IwaTestServerConfigurator::CreateForceInstallPolicyEntry(
              web_bundle_id_1()))
          .Append(IwaTestServerConfigurator::CreateForceInstallPolicyEntry(
              web_bundle_id_2())));

  // The first app installation finishes immediately, but fails. The second app
  // installation does not finish immediately and completion has to be triggered
  // later by the test (this simulates a completion delay), but will succeed.
  iwa_installer_factory_.SetCommandBehavior(
      url_info_1.web_bundle_id().id(),
      /*execution_mode=*/
      MockIsolatedWebAppInstallCommandWrapper::ExecutionMode::kSimulateFailure,
      /*execute_immediately=*/true);
  iwa_installer_factory_.SetCommandBehavior(
      url_info_2.web_bundle_id().id(),
      /*execution_mode=*/
      MockIsolatedWebAppInstallCommandWrapper::ExecutionMode::kRunCommand,
      /*execute_immediately=*/false);

  // Run the first attempt to install the isolated apps (the first one fails
  // immediately, the second one is still busy).
  task_environment().FastForwardBy(base::TimeDelta(base::Seconds(1)));

  ASSERT_EQ(2u, iwa_installer_factory_.GetNumberOfCreatedInstallTasks());
  const WebApp* web_app1_t0 =
      provider().registrar_unsafe().GetAppById(url_info_1.app_id());
  ASSERT_THAT(web_app1_t0, IsNull());
  const WebApp* web_app2_t0 =
      provider().registrar_unsafe().GetAppById(url_info_2.app_id());
  ASSERT_THAT(web_app2_t0, IsNull());

  // Forward by 60 seconds. Because the second app was not completed yet, still
  // no retry should be scheduled.
  task_environment().FastForwardBy(base::TimeDelta(base::Seconds(60)));
  ASSERT_EQ(2u, iwa_installer_factory_.GetNumberOfCreatedInstallTasks());

  ASSERT_TRUE(iwa_installer_factory_.GetLatestCommandWrapper(
      url_info_2.web_bundle_id().id()));
  ASSERT_FALSE(iwa_installer_factory_
                   .GetLatestCommandWrapper(url_info_2.web_bundle_id().id())
                   ->CommandWasScheduled());

  // Complete install task for the second app (which succeeds).
  WebAppTestInstallObserver app2_install_observer(profile());
  app2_install_observer.BeginListening({url_info_2.app_id()});

  task_environment().GetMainThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &MockIsolatedWebAppInstallCommandWrapper::ScheduleCommand,
          base::Unretained(iwa_installer_factory_.GetLatestCommandWrapper(
              url_info_2.web_bundle_id().id()))));
  task_environment().FastForwardBy(base::TimeDelta(base::Seconds(1)));

  EXPECT_EQ(app2_install_observer.Wait(), url_info_2.app_id());

  // The retry command for the first app should be successful. The second app
  // doesn't need a retry.
  iwa_installer_factory_.SetCommandBehavior(
      url_info_1.web_bundle_id().id(),
      /*execution_mode=*/
      MockIsolatedWebAppInstallCommandWrapper::ExecutionMode::kRunCommand,
      /*execute_immediately=*/true);
  task_environment().FastForwardBy(base::TimeDelta(base::Seconds(1)));
  // The retry is scheduled, but the install task for the remaining app is not
  // yet created.
  ASSERT_EQ(2u, iwa_installer_factory_.GetNumberOfCreatedInstallTasks());

  // Forward to right before an additional install task for the first app is
  // scheduled.
  task_environment().FastForwardBy(base::TimeDelta(base::Seconds(57)));
  ASSERT_EQ(2u, iwa_installer_factory_.GetNumberOfCreatedInstallTasks());

  WebAppTestInstallObserver app1_install_observer(profile());
  app1_install_observer.BeginListening({url_info_1.app_id()});

  // Moving the clock forward will finally install the second app.
  task_environment().FastForwardBy(base::TimeDelta(base::Seconds(1)));
  ASSERT_EQ(3u, iwa_installer_factory_.GetNumberOfCreatedInstallTasks());

  EXPECT_EQ(app1_install_observer.Wait(), url_info_1.app_id());

  const WebApp* web_app1_t2 =
      provider().registrar_unsafe().GetAppById(url_info_1.app_id());
  ASSERT_THAT(web_app1_t2, NotNull());
  EXPECT_THAT(web_app1_t2->GetSources(),
              Eq(WebAppManagementTypes({WebAppManagement::Type::kIwaPolicy})));
  const WebApp* web_app2_t2 =
      provider().registrar_unsafe().GetAppById(url_info_2.app_id());
  ASSERT_THAT(web_app2_t2, NotNull());
  EXPECT_THAT(web_app2_t2->GetSources(),
              Eq(WebAppManagementTypes({WebAppManagement::Type::kIwaPolicy})));
}

class CleanupOrphanedBundlesTest : public IsolatedWebAppPolicyManagerTestBase {
 public:
  CleanupOrphanedBundlesTest()
      : IsolatedWebAppPolicyManagerTestBase(
            /*is_mgs_session_install_enabled=*/false,
            /*is_user_session=*/true) {}

  void SetUp() override {
    IsolatedWebAppPolicyManagerTestBase::SetUp();
    iwa_installer_factory_.SetUp(profile());
  }

  void TearDown() override {
    command_scheduler_ = nullptr;
    IsolatedWebAppPolicyManagerTestBase::TearDown();
  }

 protected:
  TestIwaInstallerFactory iwa_installer_factory_;
  raw_ptr<TestOrphanedCleanupWebAppCommandScheduler> command_scheduler_ =
      nullptr;
  base::test::TestFuture<void> command_done_future_;

 private:
  // `IsolatedWebAppPolicyManagerTestBase`:
  void SetCommandScheduler() override {
    auto command_scheduler =
        std::make_unique<TestOrphanedCleanupWebAppCommandScheduler>(*profile());
    command_scheduler_ = command_scheduler.get();
    command_scheduler_->SetCommandDoneClosure(
        command_done_future_.GetRepeatingCallback());
    provider().SetScheduler(std::move(command_scheduler));
  }
};

TEST_F(CleanupOrphanedBundlesTest, CleanUpCalledOnSessionStart) {
  // Nothing to do here. The session start is automatically performed and the
  // expectations are therefore set early in
  // `CleanupOrphanedBundlesTest::SetCommandScheduler`.
  command_scheduler_->SetCommandDoneClosure(
      command_done_future_.GetRepeatingCallback());
  ASSERT_TRUE(command_done_future_.Wait());
  EXPECT_EQ(1u, command_scheduler_->GetNumberOfCalls());
}

// Install two isolated web apps. One of them succeeds, the other one fails and
// therefore the cleanup command should be scheduled.
TEST_F(CleanupOrphanedBundlesTest, CleanUpCalledOnTaskFailure) {
  ASSERT_TRUE(command_done_future_.Wait());
  EXPECT_EQ(1u, command_scheduler_->GetNumberOfCalls());
  command_done_future_.Clear();

  auto url_info_1 =
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(web_bundle_id_1());
  auto url_info_2 =
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(web_bundle_id_2());
  iwa_installer_factory_.SetCommandBehavior(
      url_info_1.web_bundle_id().id(),
      /*execution_mode=*/
      MockIsolatedWebAppInstallCommandWrapper::ExecutionMode::kRunCommand,
      /*execute_immediately=*/true);
  iwa_installer_factory_.SetCommandBehavior(
      url_info_2.web_bundle_id().id(),
      /*execution_mode=*/
      MockIsolatedWebAppInstallCommandWrapper::ExecutionMode::kSimulateFailure,
      /*execute_immediately=*/true);

  WebAppTestInstallObserver install_observer(profile());
  install_observer.BeginListening({url_info_1.app_id()});

  profile()->GetPrefs()->SetList(
      prefs::kIsolatedWebAppInstallForceList,
      base::Value::List()
          .Append(IwaTestServerConfigurator::CreateForceInstallPolicyEntry(
              web_bundle_id_1()))
          .Append(IwaTestServerConfigurator::CreateForceInstallPolicyEntry(
              web_bundle_id_2())));

  EXPECT_EQ(install_observer.Wait(), url_info_1.app_id());

  ASSERT_TRUE(command_done_future_.Wait());
  EXPECT_EQ(2u, command_scheduler_->GetNumberOfCalls());
}

TEST_F(CleanupOrphanedBundlesTest, CleanUpNotCalledOnAllTasksSuccess) {
  ASSERT_TRUE(command_done_future_.Wait());
  EXPECT_EQ(1u, command_scheduler_->GetNumberOfCalls());
  command_done_future_.Clear();

  auto url_info_1 =
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(web_bundle_id_1());
  auto url_info_2 =
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(web_bundle_id_2());
  iwa_installer_factory_.SetCommandBehavior(
      url_info_1.web_bundle_id().id(),
      /*execution_mode=*/
      MockIsolatedWebAppInstallCommandWrapper::ExecutionMode::kRunCommand,
      /*execute_immediately=*/true);
  iwa_installer_factory_.SetCommandBehavior(
      url_info_2.web_bundle_id().id(),
      /*execution_mode=*/
      MockIsolatedWebAppInstallCommandWrapper::ExecutionMode::kRunCommand,
      /*execute_immediately=*/true);

  // Wait until the initial commands were executed (among of which one is a
  // cleanup command).
  provider().command_manager().AwaitAllCommandsCompleteForTesting();

  // We do not expect the cleanup command to be called.
  command_scheduler_->SetCommandDoneClosure(base::NullCallback());

  WebAppTestInstallObserver install_observer(profile());
  install_observer.BeginListening({url_info_1.app_id(), url_info_2.app_id()});

  profile()->GetPrefs()->SetList(
      prefs::kIsolatedWebAppInstallForceList,
      base::Value::List()
          .Append(IwaTestServerConfigurator::CreateForceInstallPolicyEntry(
              web_bundle_id_1()))
          .Append(IwaTestServerConfigurator::CreateForceInstallPolicyEntry(
              web_bundle_id_2())));

  const webapps::AppId last_installed_app_id = install_observer.Wait();
  task_environment().RunUntilIdle();

  EXPECT_TRUE(last_installed_app_id == url_info_1.app_id() ||
              last_installed_app_id == url_info_2.app_id());
}

class IsolatedWebAppInstallEmergencyMechanismTest
    : public IsolatedWebAppPolicyManagerTestBase,
      public testing::WithParamInterface<int> {
 public:
  IsolatedWebAppInstallEmergencyMechanismTest()
      : IsolatedWebAppPolicyManagerTestBase(
            /*is_mgs_session_install_enabled=*/false,
            /*is_user_session=*/true,
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    feature_list_.InitAndDisableFeature(
        kIwaPolicyManagerOnDemandComponentUpdate);
  }

 protected:
  int GetSimulatedPendingInstallCount() { return GetParam(); }

  webapps::AppId app_id_;

 private:
  // `IsolatedWebAppPolicyManagerTestBase`:
  void SetCommandScheduler() override {
    // For these tests we are fine with the regular command scheduler.
    app_id_ = IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
                  test::GetDefaultEd25519WebBundleId())
                  .app_id();

    test::AddForceInstalledIwaToPolicy(
        profile()->GetPrefs(),
        IwaTestServerConfigurator::CreateForceInstallPolicyEntry(
            test::GetDefaultEd25519WebBundleId()));

    // Set the number of previous crashes on profile creation to simulate a
    // previously crashing device.
    profile()->GetPrefs()->SetInteger(
        prefs::kIsolatedWebAppPendingInitializationCount,
        GetSimulatedPendingInstallCount());
  }

  base::test::ScopedFeatureList feature_list_;
};

TEST_P(IsolatedWebAppInstallEmergencyMechanismTest,
       EmergencyMechanismOnStartup) {
  // If the emergency mechanism is triggered, the install count is increased by
  // one. If not, the startup is successful and the pending install count is
  // reset to 0.
  if (GetSimulatedPendingInstallCount() > 2) {
    EXPECT_EQ(GetSimulatedPendingInstallCount() + 1,
              profile()->GetPrefs()->GetInteger(
                  prefs::kIsolatedWebAppPendingInitializationCount));
  } else {
    EXPECT_EQ(0, profile()->GetPrefs()->GetInteger(
                     prefs::kIsolatedWebAppPendingInitializationCount));
  }

  // Process all the pending immediate tasks (not the delayed emergency task).
  task_environment().FastForwardBy(base::Seconds(1));

  // If we already tried twice, we delay the execution to allow for updates.
  if (GetSimulatedPendingInstallCount() > 2) {
    EXPECT_EQ(GetSimulatedPendingInstallCount() + 1,
              profile()->GetPrefs()->GetInteger(
                  prefs::kIsolatedWebAppPendingInitializationCount));
    EXPECT_EQ(0u, provider().registrar_unsafe().GetAppIds().size());

    // Forward until one second before the retry. The pending installation count
    // is still not reset.
    task_environment().FastForwardBy(base::Hours(4) + base::Minutes(59) +
                                     base::Seconds(58));
    EXPECT_EQ(GetSimulatedPendingInstallCount() + 1,
              profile()->GetPrefs()->GetInteger(
                  prefs::kIsolatedWebAppPendingInitializationCount));
    EXPECT_EQ(0u, provider().registrar_unsafe().GetAppIds().size());

    // Forward by another second, which triggers the retry.
    task_environment().FastForwardBy(base::Seconds(1));
  }

  provider().command_manager().AwaitAllCommandsCompleteForTesting();
  EXPECT_EQ(1u, provider().registrar_unsafe().GetAppIds().size());
  EXPECT_EQ(0, profile()->GetPrefs()->GetInteger(
                   prefs::kIsolatedWebAppPendingInitializationCount));
}

INSTANTIATE_TEST_SUITE_P(
    /***/,
    IsolatedWebAppInstallEmergencyMechanismTest,
    // Simulates the number of failed attempts before the current session start.
    testing::ValuesIn({0, 1, 2, 3}));

class IsolatedWebAppPolicyManagerOnDemandUpdateDownloadedTest
    : public IsolatedWebAppTest {
 public:
  using Component =
      component_updater::IwaKeyDistributionComponentInstallerPolicy;
  using Priority = component_updater::OnDemandUpdater::Priority;
  using ComponentRegistration = component_updater::ComponentRegistration;

  static constexpr std::string_view kIwaKeyDistributionComponentId =
      "iebhnlpddlcpcfpfalldikcoeakpeoah";

  IsolatedWebAppPolicyManagerOnDemandUpdateDownloadedTest()
      : IsolatedWebAppTest(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
  }

 protected:
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
};

using testing::Field;
using testing::WithoutArgs;

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
