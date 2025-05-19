// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/functional/overloaded.h"
#include "base/json/json_writer.h"
#include "base/task/current_thread.h"
#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/app_mode/test/kiosk_mixin.h"
#include "chrome/browser/ash/app_mode/test/kiosk_test_utils.h"
#include "chrome/browser/ash/login/existing_user_controller.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/ash/policy/test_support/embedded_policy_test_server_mixin.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/developer_tools_policy_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_update_server_mixin.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_cache_client.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/policy_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/test_signed_web_bundle_builder.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/ash/components/policy/device_local_account/device_local_account_type.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/web_package/test_support/signed_web_bundles/ed25519_key_pair.h"
#include "content/public/test/browser_test.h"

namespace web_app {

namespace {

namespace em = enterprise_management;

using base::test::TestFuture;
using web_package::SignedWebBundleId;
using UpdateDiscoveryTaskFuture =
    TestFuture<IsolatedWebAppUpdateDiscoveryTask::CompletionStatus>;
using UpdateApplyTaskFuture =
    TestFuture<IsolatedWebAppUpdateApplyTask::CompletionStatus>;
using ash::KioskMixin;
using ash::kiosk::test::LaunchAppManually;
using ash::kiosk::test::TheKioskApp;
using ash::kiosk::test::WaitKioskLaunched;
using base::test::ValueIs;
using testing::Eq;
using testing::HasSubstr;

constexpr char kBaseVersion[] = "1.0.0";
const char kUpdateVersion[] = "2.0.2";
constexpr char kEmail[] = "iwa@example.com";
constexpr char kMgsDisplayName[] = "MGS";
constexpr char kIwaName[] = "IsolatedWebApp";

const SignedWebBundleId kWebBundleId = test::GetDefaultEd25519WebBundleId();
const web_package::test::Ed25519KeyPair kPublicKeyPair =
    test::GetDefaultEd25519KeyPair();

const SignedWebBundleId kWebBundleId2 = test::GetDefaultEcdsaP256WebBundleId();
const web_package::test::EcdsaP256KeyPair kPublicKeyPair2 =
    test::GetDefaultEcdsaP256KeyPair();

KioskMixin::Config GetKioskIwaManualLaunchConfig(
    const SignedWebBundleId& bundle_id,
    const GURL& update_manifest_url) {
  KioskMixin::IsolatedWebAppOption iwa_option(kEmail, bundle_id,
                                              update_manifest_url);
  return {kIwaName,
          /*auto_launch_account_id=*/{},
          {iwa_option}};
}

void WaitUntilPathExists(const base::FilePath& path) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  ASSERT_TRUE(base::test::RunUntil([&]() { return base::PathExists(path); }));
}

void WaitUntilPathDoesNotExist(const base::FilePath& path) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  ASSERT_TRUE(base::test::RunUntil([&]() { return !base::PathExists(path); }));
}

void CheckPathDoesNotExist(const base::FilePath& path) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  ASSERT_FALSE(base::PathExists(path));
}

}  // namespace

enum class SessionType {
  kManagedGuestSession = 0,
  kKiosk = 1,
};

// This mixin helps browser tests to test Managed Guest Session(MGS) mode.
// TODO(crbug.com/307518336): extract this class and reuse `MgsMixin` in other
// browser tests.
class MgsMixin {
 public:
  explicit MgsMixin(InProcessBrowserTestMixinHost* host)
      : policy_test_server_mixin_(host),
        device_state_(
            host,
            ash::DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED) {}

  void ConfigureMgsWithIwa(const std::string& forced_installed_iwa) {
    AddManagedGuestSessionToDevicePolicy();
    AddDeviceLocalAccountIwaPolicy(forced_installed_iwa);
    UploadAndInstallDeviceLocalAccountPolicy();
  }

  bool LaunchMgs() {
    // Start login into the device-local account.
    auto& host = CHECK_DEREF(ash::LoginDisplayHost::default_host());
    host.StartSignInScreen();

    auto& controller =
        CHECK_DEREF(ash::ExistingUserController::current_controller());
    ash::UserContext user_context(user_manager::UserType::kPublicAccount,
                                  kMgsAccountId);
    controller.Login(user_context, ash::SigninSpecifics());

    // Wait for MGS start.
    if (session_manager::SessionManager::Get()->IsSessionStarted()) {
      return true;
    }
    ash::test::WaitForPrimaryUserSessionStart();
    return true;
  }

 private:
  void AddManagedGuestSessionToDevicePolicy() {
    policy::DeviceLocalAccountTestHelper::SetupDeviceLocalAccount(
        &device_local_account_policy_, kEmail, kMgsDisplayName);

    em::ChromeDeviceSettingsProto& proto(
        policy_helper_.device_policy()->payload());
    policy::DeviceLocalAccountTestHelper::AddPublicSession(&proto, kEmail);
    policy_helper_.RefreshDevicePolicy();
    policy_test_server_mixin_.UpdateDevicePolicy(proto);
  }

  // This policy is active at the moment of MGS login.
  void AddDeviceLocalAccountIwaPolicy(const std::string& forced_installed_iwa) {
    em::StringPolicyProto* const isolated_web_apps_proto =
        device_local_account_policy_.payload()
            .mutable_isolatedwebappinstallforcelist();
    isolated_web_apps_proto->set_value(forced_installed_iwa);
  }

  void UploadAndInstallDeviceLocalAccountPolicy() {
    // Build device local account policy.
    device_local_account_policy_.SetDefaultSigningKey();
    device_local_account_policy_.Build();

    policy_test_server_mixin_.UpdatePolicy(
        policy::dm_protocol::kChromePublicAccountPolicyType, kEmail,
        device_local_account_policy_.payload().SerializeAsString());

    ash::FakeSessionManagerClient::Get()->set_device_local_account_policy(
        kEmail, device_local_account_policy_.GetBlob());

    // Wait for the display name becoming available as that indicates
    // device-local account policy is fully loaded, which is a prerequisite for
    // successful login.
    policy::DictionaryLocalStateValueWaiter("UserDisplayName", kMgsDisplayName,
                                            kMgsAccountId.GetUserEmail())
        .Wait();
  }

  const AccountId kMgsAccountId =
      AccountId::FromUserEmail(GenerateDeviceLocalAccountUserId(
          kEmail,
          policy::DeviceLocalAccountType::kPublicSession));

  ash::EmbeddedPolicyTestServerMixin policy_test_server_mixin_;
  // Used to enroll the device and simulate pre-cached policy state.
  ash::DeviceStateMixin device_state_;
  policy::DevicePolicyCrosTestHelper policy_helper_;
  policy::UserPolicyBuilder device_local_account_policy_;
};

class IwaCacheBaseTest : public ash::LoginManagerTest {
 public:
  explicit IwaCacheBaseTest(SessionType session_type,
                            bool should_configure_session = true)
      : session_type_(session_type),
        should_configure_session_(should_configure_session),
        session_mixin_(CreateSessionMixin(session_type_)) {
    scoped_feature_list_.InitWithFeatures(
        {features::kIsolatedWebAppBundleCache,
         features::kIsolatedWebAppManagedGuestSessionInstall},
        /*disabled_features=*/{});
  }

  void SetUpOnMainThread() override {
    ash::LoginManagerTest::SetUpOnMainThread();
    iwa_mixin_.AddBundle(
        IsolatedWebAppBuilder(ManifestBuilder().SetVersion(kBaseVersion))
            .BuildBundle(kPublicKeyPair));
    OverrideCacheDir();

    if (should_configure_session_) {
      ConfigureSession(kWebBundleId);
    }
  }

  void ConfigureSession(const SignedWebBundleId& installed_iwa) {
    // Configure MGS or kiosk.
    std::visit(base::Overloaded{
                   [&](MgsMixin& mgs_mixin) {
                     mgs_mixin.ConfigureMgsWithIwa(
                         WriteJson(base::Value::List().Append(
                                       iwa_mixin_.CreateForceInstallPolicyEntry(
                                           installed_iwa)))
                             .value());
                   },
                   [&](KioskMixin& kiosk_mixin) {
                     ash::ScopedDevicePolicyUpdate scoped_update(
                         policy_helper_.device_policy(),
                         base::BindLambdaForTesting([&]() {
                           policy_helper_
                               .RefreshPolicyAndWaitUntilDeviceSettingsUpdated(
                                   {ash::kAccountsPrefDeviceLocalAccounts});
                         }));
                     kiosk_mixin.Configure(
                         scoped_update,
                         GetKioskIwaManualLaunchConfig(
                             installed_iwa,
                             iwa_mixin_.GetUpdateManifestUrl(installed_iwa)));
                   },
               },
               session_mixin_);
  }

  void LaunchSession() {
    std::visit(
        base::Overloaded([](MgsMixin& mgs_mixin) { mgs_mixin.LaunchMgs(); },
                         [](KioskMixin& kiosk_mixin) {
                           ASSERT_TRUE(LaunchAppManually(TheKioskApp()));
                           ASSERT_TRUE(WaitKioskLaunched());
                         }),
        session_mixin_);
  }

  void AssertAppInstalledAtVersion(const std::string_view version,
                                   bool wait_for_initial_installation = true) {
    if (IsManagedGuestSession() && wait_for_initial_installation) {
      // Wait for the IWA to be installed in MGS. In Kiosk app is already
      // installed when the kiosk is launched.
      WebAppTestInstallObserver observer(profile());
      observer.BeginListeningAndWait({GetAppId()});
    }

    const WebApp* app = GetIsolatedWebApp();
    ASSERT_TRUE(app);
    ASSERT_EQ(app->isolation_data()->version().GetString(), version);
  }

  base::FilePath GetCachedBundlePath(
      std::string_view version,
      const SignedWebBundleId& bundle_id = kWebBundleId) {
    base::FilePath bundle_directory_path = cache_root_dir_;
    switch (session_type()) {
      case SessionType::kManagedGuestSession:
        bundle_directory_path =
            bundle_directory_path.AppendASCII(IwaCacheClient::kMgsDirName);
        break;
      case SessionType::kKiosk:
        bundle_directory_path =
            bundle_directory_path.AppendASCII(IwaCacheClient::kKioskDirName);
        break;
    }
    return bundle_directory_path.AppendASCII(bundle_id.id())
        .AppendASCII(version)
        .AppendASCII(kMainSwbnFileName);
  }

  // Ensures that the follow-up installation is done via cache, since it's not
  // possible to install IWA from the Internet after this function is executed.
  void RemoveBundleFromUpdateServer() {
    auto versions = GetVersionsFromUpdateManifest();
    for (auto version : GetVersionsFromUpdateManifest()) {
      iwa_mixin_.RemoveBundle(kWebBundleId, version);
    }
  }

  std::vector<base::Version> GetVersionsFromUpdateManifest() {
    std::vector<base::Version> versions;

    base::Value::Dict manifest_dict =
        iwa_mixin_.GetUpdateManifest(kWebBundleId);
    for (auto& version_value :
         CHECK_DEREF(manifest_dict.FindList("versions"))) {
      auto& version_dict = CHECK_DEREF(version_value.GetIfDict());
      versions.emplace_back(CHECK_DEREF(version_dict.FindString("version")));
    }
    return versions;
  }

  void AddNewVersionToUpdateServer(std::string_view version) {
    AddNewIwaToServer(kPublicKeyPair, version);
  }

  void AddNewIwaToServer(const web_package::test::KeyPair& key_pair,
                         std::string_view version) {
    iwa_mixin_.AddBundle(
        IsolatedWebAppBuilder(
            ManifestBuilder().SetName(kIwaName).SetVersion(version))
            .BuildBundle(key_pair));
  }

  void OpenIwa() { OpenIsolatedWebApp(profile(), GetAppId()); }

  IsolatedWebAppUpdateDiscoveryTask::CompletionStatus
  DiscoverUpdateAndWaitForResult() {
    UpdateDiscoveryTaskFuture discovery_update_future;
    UpdateDiscoveryTaskResultWaiter discovery_update_waiter(
        provider(), GetAppId(), discovery_update_future.GetCallback());

    DiscoverUpdatesNow();
    return discovery_update_future.Get();
  }

  void DiscoverUpdatesNow() {
    EXPECT_THAT(provider().iwa_update_manager().DiscoverUpdatesNow(), Eq(1ul));
  }

  void DestroyCacheDir() { cache_root_dir_override_.reset(); }

  const WebApp* GetIsolatedWebApp() {
    ASSIGN_OR_RETURN(
        const WebApp& iwa,
        GetIsolatedWebAppById(provider().registrar_unsafe(), GetAppId()),
        [](const std::string&) { return nullptr; });
    return &iwa;
  }

  size_t GetNumOpenedWindowsForIwa() {
    return provider().ui_manager().GetNumWindowsForApp(GetAppId());
  }

  WebAppProvider& provider() {
    auto* provider = WebAppProvider::GetForTest(profile());
    CHECK(provider);
    return *provider;
  }

  webapps::AppId GetAppId() const {
    return IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(kWebBundleId)
        .app_id();
  }

  bool IsManagedGuestSession() const {
    return session_type() == SessionType::kManagedGuestSession;
  }

  SessionType session_type() const { return session_type_; }

 private:
  std::variant<MgsMixin, KioskMixin> CreateSessionMixin(
      SessionType session_type) {
    switch (session_type) {
      case SessionType::kManagedGuestSession:
        return std::variant<MgsMixin, KioskMixin>{std::in_place_type<MgsMixin>,
                                                  &mixin_host_};
      case SessionType::kKiosk:
        return std::variant<MgsMixin, KioskMixin>{
            std::in_place_type<KioskMixin>, &mixin_host_};
    }
  }

  void OverrideCacheDir() {
    ProfileManager* profile_manager = g_browser_process->profile_manager();
    EXPECT_TRUE(profile_manager);
    cache_root_dir_ = profile_manager->user_data_dir();
    cache_root_dir_override_ = std::make_unique<base::ScopedPathOverride>(
        ash::DIR_DEVICE_LOCAL_ACCOUNT_IWA_CACHE, cache_root_dir_);
  }

  Profile* profile() {
    // Any profile can be used here since this test does not test multi profile.
    return ProfileManager::GetActiveUserProfile();
  }

  const SessionType session_type_;
  bool should_configure_session_;
  IsolatedWebAppUpdateServerMixin iwa_mixin_{&mixin_host_};
  base::test::ScopedFeatureList scoped_feature_list_;
  policy::DevicePolicyCrosTestHelper policy_helper_;
  base::FilePath cache_root_dir_;
  std::unique_ptr<base::ScopedPathOverride> cache_root_dir_override_;
  std::variant<MgsMixin, KioskMixin> session_mixin_;
};

class IwaCacheTest : public IwaCacheBaseTest,
                     public testing::WithParamInterface<SessionType> {
 public:
  IwaCacheTest() : IwaCacheBaseTest(GetParam()) {}
};

IN_PROC_BROWSER_TEST_P(IwaCacheTest, PRE_InstallIsolatedWebAppOnLogin) {
  LaunchSession();
  AssertAppInstalledAtVersion(kBaseVersion);

  // Checks that bundle is copied to cache after the successful installation.
  WaitUntilPathExists(GetCachedBundlePath(kBaseVersion));
}

IN_PROC_BROWSER_TEST_P(IwaCacheTest, InstallIsolatedWebAppOnLogin) {
  // Checks that the bundle is still in cache from the PRE test.
  WaitUntilPathExists(GetCachedBundlePath(kBaseVersion));

  RemoveBundleFromUpdateServer();
  LaunchSession();
  AssertAppInstalledAtVersion(kBaseVersion);
}

IN_PROC_BROWSER_TEST_P(IwaCacheTest, PRE_UpdateApplyTaskFinishedOnSessionExit) {
  LaunchSession();
  AssertAppInstalledAtVersion(kBaseVersion);
  WaitUntilPathExists(GetCachedBundlePath(kBaseVersion));
  if (IsManagedGuestSession()) {
    // Only open app in MGS, in kiosk app is always opened after the session
    // started.
    OpenIwa();
  }
  // When app is opened, the update cannot be applied, so it will be applied on
  // session exit.
  EXPECT_THAT(GetNumOpenedWindowsForIwa(), Eq(1ul));

  AddNewVersionToUpdateServer(kUpdateVersion);

  EXPECT_THAT(DiscoverUpdateAndWaitForResult(),
              ValueIs(IsolatedWebAppUpdateDiscoveryTask::Success::
                          kUpdateFoundAndSavedInDatabase));
  CheckPathDoesNotExist(GetCachedBundlePath(kUpdateVersion));
}

// Checks that on session exit in PRE_ test, pending update apply task is
// successfully finished and it updated the cache.
IN_PROC_BROWSER_TEST_P(IwaCacheTest, UpdateApplyTaskFinishedOnSessionExit) {
  // TODO(crbug.com/392069400): update to check old version is deleted from
  // cache.
  WaitUntilPathExists(GetCachedBundlePath(kBaseVersion));
  WaitUntilPathExists(GetCachedBundlePath(kUpdateVersion));

  RemoveBundleFromUpdateServer();
  LaunchSession();

  AssertAppInstalledAtVersion(kUpdateVersion);
}

IN_PROC_BROWSER_TEST_P(IwaCacheTest, PRE_UpdateNotFound) {
  LaunchSession();
  AssertAppInstalledAtVersion(kBaseVersion);
  WaitUntilPathExists(GetCachedBundlePath(kBaseVersion));
  if (IsManagedGuestSession()) {
    // Only open app in MGS, in kiosk app is always opened after the session
    // started.
    OpenIwa();
  }
  // When app is opened, the update cannot be applied, so it will be applied on
  // session exit.
  EXPECT_THAT(GetNumOpenedWindowsForIwa(), Eq(1ul));

  EXPECT_THAT(
      DiscoverUpdateAndWaitForResult(),
      ValueIs(IsolatedWebAppUpdateDiscoveryTask::Success::kNoUpdateFound));
}

// In PRE_ test, update discovery task did not find the update, check that the
// cache was not updated on the session exit.
IN_PROC_BROWSER_TEST_P(IwaCacheTest, UpdateNotFound) {
  WaitUntilPathExists(GetCachedBundlePath(kBaseVersion));
  CheckPathDoesNotExist(GetCachedBundlePath(kUpdateVersion));

  RemoveBundleFromUpdateServer();
  LaunchSession();

  AssertAppInstalledAtVersion(kBaseVersion);
}

// Install base version from the Internet.
IN_PROC_BROWSER_TEST_P(IwaCacheTest,
                       PRE_PRE_UpdateTaskIsTriggeredAutomatically) {
  LaunchSession();
  AssertAppInstalledAtVersion(kBaseVersion);
  WaitUntilPathExists(GetCachedBundlePath(kBaseVersion));
  CheckPathDoesNotExist(GetCachedBundlePath(kUpdateVersion));
}

// Add new version to the manifest, but the installation will be done from cache
// with the base version first. Then the IWA cache manager will automatically
// trigger the update check. On the session exit the new version will be copied
// to cache. On the 3rd session start new IWA version will be installed.
IN_PROC_BROWSER_TEST_P(IwaCacheTest, PRE_UpdateTaskIsTriggeredAutomatically) {
  AddNewVersionToUpdateServer(kUpdateVersion);
  LaunchSession();

  AssertAppInstalledAtVersion(kBaseVersion);
  if (IsManagedGuestSession()) {
    // Only open app in MGS, in kiosk app is always opened after the session
    // started.
    OpenIwa();
  }

  // If the apply update task is not completed, wait at least until the discover
  // update task has completed.
  const WebApp* app = GetIsolatedWebApp();
  ASSERT_TRUE(app);
  if (app->isolation_data()->version().GetString() == kBaseVersion) {
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return app->isolation_data()->pending_update_info().has_value();
    }));
  }
}

IN_PROC_BROWSER_TEST_P(IwaCacheTest, UpdateTaskIsTriggeredAutomatically) {
  WaitUntilPathExists(GetCachedBundlePath(kUpdateVersion));

  RemoveBundleFromUpdateServer();
  LaunchSession();

  AssertAppInstalledAtVersion(kUpdateVersion);
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    IwaCacheTest,
    testing::Values(SessionType::kManagedGuestSession, SessionType::kKiosk));

// This test class is made for cases when session configuration need to be
// different from the one in `IwaCacheBaseTest`. Call `ConfigureSession` in
// tests with specified parameters.
class IwaCacheNonConfiguredSessionTest
    : public IwaCacheBaseTest,
      public testing::WithParamInterface<SessionType> {
 public:
  IwaCacheNonConfiguredSessionTest()
      : IwaCacheBaseTest(GetParam(),
                         /*should_configure_session=*/false) {}
};

IN_PROC_BROWSER_TEST_P(IwaCacheNonConfiguredSessionTest,
                       PRE_RemoveCachedBundleForUninstalledIwa) {
  ConfigureSession(kWebBundleId);
  LaunchSession();
  AssertAppInstalledAtVersion(kBaseVersion);
  WaitUntilPathExists(GetCachedBundlePath(kBaseVersion, kWebBundleId));
}

// When IWA is no longer in the policy list, `IwaCacheManager` will remove
// it's cache on session start.
IN_PROC_BROWSER_TEST_P(IwaCacheNonConfiguredSessionTest,
                       RemoveCachedBundleForUninstalledIwa) {
  AddNewIwaToServer(kPublicKeyPair2, kBaseVersion);
  ConfigureSession(kWebBundleId2);
  LaunchSession();

  auto bundle_path = GetCachedBundlePath(kBaseVersion, kWebBundleId);
  WaitUntilPathDoesNotExist(bundle_path);
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    IwaCacheNonConfiguredSessionTest,
    testing::Values(SessionType::kManagedGuestSession, SessionType::kKiosk));

// Covers Managed Guest Session (MGS) specific tests which cannot be tested in
// kiosk. For example, kiosk always launch the IWA app, but in MGS it is
// possible to open and close the app inside the sessions.
class IwaMgsCacheTest : public IwaCacheBaseTest {
 public:
  IwaMgsCacheTest() : IwaCacheBaseTest(SessionType::kManagedGuestSession) {}

  IsolatedWebAppUpdateApplyTask::CompletionStatus
  WaitForUpdateApplyTaskResult() {
    UpdateApplyTaskFuture apply_update_future;
    UpdateApplyTaskResultWaiter apply_update_waiter(
        provider(), GetAppId(), apply_update_future.GetCallback());
    return apply_update_future.Get();
  }

  void CloseApp() {
    TestFuture<void> app_closed_future;
    provider().ui_manager().NotifyOnAllAppWindowsClosed(
        GetAppId(), app_closed_future.GetCallback());
    provider().ui_manager().CloseAppWindows(GetAppId());
    EXPECT_TRUE(app_closed_future.Wait());
    EXPECT_THAT(GetNumOpenedWindowsForIwa(), Eq(0ul));
  }
};

IN_PROC_BROWSER_TEST_F(IwaMgsCacheTest, UpdateAppWhenAppNotOpened) {
  LaunchSession();
  AssertAppInstalledAtVersion(kBaseVersion);
  WaitUntilPathExists(GetCachedBundlePath(kBaseVersion));

  AddNewVersionToUpdateServer(kUpdateVersion);
  DiscoverUpdatesNow();

  EXPECT_TRUE(WaitForUpdateApplyTaskResult().has_value());
  AssertAppInstalledAtVersion(kUpdateVersion,
                              /*wait_for_initial_installation=*/false);
  WaitUntilPathExists(GetCachedBundlePath(kUpdateVersion));
}

IN_PROC_BROWSER_TEST_F(IwaMgsCacheTest, UpdateApplyTaskWhenAppClosed) {
  LaunchSession();
  AssertAppInstalledAtVersion(kBaseVersion);
  WaitUntilPathExists(GetCachedBundlePath(kBaseVersion));

  OpenIwa();
  EXPECT_THAT(GetNumOpenedWindowsForIwa(), Eq(1ul));
  AddNewVersionToUpdateServer(kUpdateVersion);

  // Updates will be applied once the app's window is closed.
  CloseApp();
  DiscoverUpdatesNow();

  EXPECT_TRUE(WaitForUpdateApplyTaskResult().has_value());
  AssertAppInstalledAtVersion(kUpdateVersion,
                              /*wait_for_initial_installation=*/false);
  WaitUntilPathExists(GetCachedBundlePath(kUpdateVersion));
}

IN_PROC_BROWSER_TEST_F(IwaMgsCacheTest, CopyToCacheFailed) {
  LaunchSession();
  AssertAppInstalledAtVersion(kBaseVersion);
  WaitUntilPathExists(GetCachedBundlePath(kBaseVersion));

  AddNewVersionToUpdateServer(kUpdateVersion);
  DestroyCacheDir();
  EXPECT_THAT(DiscoverUpdateAndWaitForResult(),
              ValueIs(IsolatedWebAppUpdateDiscoveryTask::Success::
                          kUpdateFoundAndSavedInDatabase));

  IsolatedWebAppUpdateApplyTask::CompletionStatus apply_task_result =
      WaitForUpdateApplyTaskResult();

  // The update is applied, but it was not saved to cache because of the error
  // during copying to cache.
  ASSERT_FALSE(apply_task_result.has_value());
  EXPECT_THAT(
      apply_task_result.error().message,
      HasSubstr(IsolatedWebAppUpdateApplyTask::kCopyToCacheFailedMessage));
  AssertAppInstalledAtVersion(kUpdateVersion,
                              /*wait_for_initial_installation=*/false);
  CheckPathDoesNotExist(GetCachedBundlePath(kUpdateVersion));
}

// TODO(crbug.com/392069400): add more browser tests when cleaning old IWA
// versions from cache is implemented.

}  // namespace web_app
