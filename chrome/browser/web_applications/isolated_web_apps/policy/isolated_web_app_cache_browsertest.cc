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
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_update_server_mixin.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_cache_client.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/test_signed_web_bundle_builder.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/common/chrome_features.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "components/policy/core/common/device_local_account_type.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/web_package/test_support/signed_web_bundles/ed25519_key_pair.h"
#include "content/public/test/browser_test.h"

namespace web_app {

namespace {

namespace em = enterprise_management;

using ash::KioskMixin;

constexpr char kVersion[] = "1.0.0";
constexpr char kEmail[] = "iwa@example.com";
constexpr char kMgsDisplayName[] = "MGS";
constexpr char kIwaName[] = "IsolatedWebApp";

const web_package::SignedWebBundleId kWebBundleId =
    test::GetDefaultEd25519WebBundleId();
const web_package::test::Ed25519KeyPair kPublicKeyPair =
    test::GetDefaultEd25519KeyPair();

KioskMixin::Config GetKioskIwaManualLaunchConfig(
    const GURL& update_manifest_url) {
  KioskMixin::IsolatedWebAppOption iwa_option(kEmail, kWebBundleId,
                                              update_manifest_url);
  return {kIwaName,
          /*auto_launch_account_id=*/{},
          {iwa_option}};
}

void WaitUntilPathExists(const base::FilePath& path) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  ASSERT_TRUE(base::test::RunUntil([&]() { return base::PathExists(path); }));
}

}  // namespace

enum class SessionType {
  kMgs = 0,
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

class IwaCacheTest : public ash::LoginManagerTest,
                     public testing::WithParamInterface<SessionType> {
 public:
  IwaCacheTest()
      : session_type_(GetParam()),
        session_mixin_(CreateSessionMixin(session_type_)) {
    scoped_feature_list_.InitWithFeatures(
        {features::kIsolatedWebAppBundleCache,
         features::kIsolatedWebAppManagedGuestSessionInstall},
        /*disabled_features=*/{});
  }

  void SetUpOnMainThread() override {
    ash::LoginManagerTest::SetUpOnMainThread();
    iwa_mixin_.AddBundle(
        IsolatedWebAppBuilder(ManifestBuilder().SetVersion(kVersion))
            .BuildBundle(kPublicKeyPair));
    OverrideCacheDir();

    // Configure MGS or kiosk.
    std::visit(base::Overloaded{
                   [&](MgsMixin& mgs_mixin) {
                     mgs_mixin.ConfigureMgsWithIwa(
                         WriteJson(base::Value::List().Append(
                                       iwa_mixin_.CreateForceInstallPolicyEntry(
                                           kWebBundleId)))
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
                             iwa_mixin_.GetUpdateManifestUrl(kWebBundleId)));
                   },
               },
               session_mixin_);
  }

  void LaunchSession() {
    std::visit(
        base::Overloaded([](MgsMixin& mgs_mixin) { mgs_mixin.LaunchMgs(); },
                         [](KioskMixin& kiosk_mixin) {
                           ASSERT_TRUE(kiosk_mixin.LaunchManually(
                               ash::kiosk::test::TheKioskApp()));
                           ASSERT_TRUE(kiosk_mixin.WaitSessionLaunched());
                         }, ),
        session_mixin_);
  }

  void AssertAppInstalledAtVersion(const std::string_view version) {
    if (session_type_ == SessionType::kMgs) {
      // Wait for the IWA to be installed in MGS. In Kiosk app is already
      // installed when the kiosk is launched.
      WebAppTestInstallObserver observer(profile());
      observer.BeginListeningAndWait(
          {IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(kWebBundleId)
               .app_id()});
    }

    const WebApp* app =
        WebAppProvider::GetForTest(profile())->registrar_unsafe().GetAppById(
            IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(kWebBundleId)
                .app_id());
    ASSERT_TRUE(app);
    ASSERT_TRUE(app->isolation_data());
    ASSERT_EQ(app->isolation_data()->version().GetString(), version);
  }

  base::FilePath GetFullBundlePath() {
    base::FilePath bundle_directory_path = cache_root_dir_;
    switch (session_type_) {
      case SessionType::kMgs:
        bundle_directory_path =
            bundle_directory_path.AppendASCII(IwaCacheClient::kMgsDirName);
        break;
      case SessionType::kKiosk:
        bundle_directory_path =
            bundle_directory_path.AppendASCII(IwaCacheClient::kKioskDirName);
        break;
    }
    return bundle_directory_path.AppendASCII(kWebBundleId.id())
        .AppendASCII(kVersion)
        .AppendASCII(kMainSwbnFileName);
  }

  void RemoveBundle() {
    iwa_mixin_.RemoveBundle(kWebBundleId, base::Version(kVersion));
  }

 private:
  std::variant<MgsMixin, KioskMixin> CreateSessionMixin(
      SessionType session_type) {
    switch (session_type) {
      case SessionType::kMgs:
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
  IsolatedWebAppUpdateServerMixin iwa_mixin_{&mixin_host_};
  base::test::ScopedFeatureList scoped_feature_list_;
  policy::DevicePolicyCrosTestHelper policy_helper_;
  base::FilePath cache_root_dir_;
  std::unique_ptr<base::ScopedPathOverride> cache_root_dir_override_;

  std::variant<MgsMixin, KioskMixin> session_mixin_;
};

IN_PROC_BROWSER_TEST_P(IwaCacheTest, PRE_InstallIsolatedWebAppOnLogin) {
  LaunchSession();
  AssertAppInstalledAtVersion(kVersion);

  // Checks that bundle is copied to cache after the successful installation.
  WaitUntilPathExists(GetFullBundlePath());
}

IN_PROC_BROWSER_TEST_P(IwaCacheTest, InstallIsolatedWebAppOnLogin) {
  // Checks that the bundle is still in cache from the PRE test.
  WaitUntilPathExists(GetFullBundlePath());

  // Make sure the IWA installation is done via cache since it's not possible
  // to install IWA from the Internet after `RemoveBundle()`.
  RemoveBundle();
  LaunchSession();
  AssertAppInstalledAtVersion(kVersion);
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    IwaCacheTest,
    testing::Values(SessionType::kMgs, SessionType::kKiosk));

// TODO(crbug.com/388729035): add more browser tests when IWA updates are
// handled.

}  // namespace web_app
