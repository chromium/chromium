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
#include "base/json/json_writer.h"
#include "base/notreached.h"
#include "base/task/current_thread.h"
#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/ash/app_mode/kiosk_app.h"
#include "chrome/browser/ash/app_mode/kiosk_controller.h"
#include "chrome/browser/ash/app_mode/test/kiosk_mixin.h"
#include "chrome/browser/ash/app_mode/test/kiosk_test_utils.h"
#include "chrome/browser/ash/app_mode/test/network_state_mixin.h"
#include "chrome/browser/ash/login/existing_user_controller.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/browser/ash/login/wizard_controller.h"
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
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_update_apply_task.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_update_server_mixin.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_cache_client.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_cache_manager.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/key_distribution/test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/policy_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/test_signed_web_bundle_builder.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/profile_waiter.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/ash/components/policy/device_local_account/device_local_account_type.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/web_package/test_support/signed_web_bundles/ed25519_key_pair.h"
#include "components/webapps/isolated_web_apps/features.h"
#include "components/webapps/isolated_web_apps/types/update_channel.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace web_app {

namespace {

namespace em = enterprise_management;

using base::test::TestFuture;
using web_package::SignedWebBundleId;
using DiscoveryTask = IsolatedWebAppUpdateDiscoveryTask;
using ApplyTask = IsolatedWebAppUpdateApplyTask;
using UpdateDiscoveryTaskFuture = TestFuture<DiscoveryTask::CompletionStatus>;
using UpdateApplyTaskFuture = TestFuture<ApplyTask::CompletionStatus>;

using ash::KioskMixin;
using ash::LoginManagerMixin;
using ash::kiosk::test::LaunchAppManually;
using ash::kiosk::test::TheKioskApp;
using ash::kiosk::test::WaitKioskLaunched;
using ash::kiosk::test::WaitNetworkScreen;
using base::test::ErrorIs;
using base::test::HasValue;
using base::test::ValueIs;
using testing::Eq;
using testing::Field;
using testing::HasSubstr;
using testing::Ne;

constexpr char kEmail[] = "iwa@example.com";
constexpr char kMgsDisplayName[] = "MGS";
constexpr char kIwaName[] = "IsolatedWebApp";

// TODO(crbug.com/428148477): rename to `kWebBundleId1` and `kPublicKeyPair1`.
const SignedWebBundleId kWebBundleId = test::GetDefaultEd25519WebBundleId();
const web_package::test::Ed25519KeyPair kPublicKeyPair =
    test::GetDefaultEd25519KeyPair();

const SignedWebBundleId kWebBundleId2 = test::GetDefaultEcdsaP256WebBundleId();
const web_package::test::EcdsaP256KeyPair kPublicKeyPair2 =
    test::GetDefaultEcdsaP256KeyPair();

const base::Version kBaseVersion = base::Version("1.0.0");
const base::Version kUpdateVersion = base::Version("2.0.2");

const UpdateChannel kBetaChannel = UpdateChannel::Create("beta").value();

KioskMixin::Config GetKioskIwaManualLaunchConfig(
    const SignedWebBundleId& bundle_id,
    const GURL& update_manifest_url,
    const std::optional<UpdateChannel>& update_channel,
    const std::optional<base::Version>& pinned_version) {
  // Use `bundle_id` as `account_id` to make it possible to find the app by the
  // AccountId.
  KioskMixin::IsolatedWebAppOption iwa_option(
      bundle_id.id(), bundle_id, update_manifest_url,
      update_channel ? update_channel->ToString() : "",
      pinned_version ? pinned_version->GetString() : "");
  return {bundle_id.id(),
          /*auto_launch_account_id=*/{},
          {iwa_option}};
}

void WaitUntilPathExists(const base::FilePath& path) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  ASSERT_TRUE(base::test::RunUntil([&]() { return base::PathExists(path); }));
}

void CheckPathExists(const base::FilePath& path) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  ASSERT_TRUE(base::PathExists(path));
}

void WaitUntilPathDoesNotExist(const base::FilePath& path) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  ASSERT_TRUE(base::test::RunUntil([&]() { return !base::PathExists(path); }));
}

void CheckPathDoesNotExist(const base::FilePath& path) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  ASSERT_FALSE(base::PathExists(path));
}

void WaitForProfile() {
  ProfileWaiter waiter;
  waiter.WaitForProfileAdded();
}

void WaitForUserSessionLaunch() {
  if (session_manager::SessionManager::Get()->IsSessionStarted()) {
    return;
  }
  if (ash::WizardController::default_controller()) {
    ash::WizardController::default_controller()
        ->SkipPostLoginScreensForTesting();
  }
  ash::test::WaitForPrimaryUserSessionStart();
}

constexpr char kCopyBundleToCacheAfterUpdateSuccessMetric[] =
    "WebApp.Isolated.CopyBundleToCacheAfterUpdateSuccess";
constexpr char kCopyBundleToCacheAfterUpdateErrorMetric[] =
    "WebApp.Isolated.CopyBundleToCacheAfterUpdateError";

}  // namespace

enum class SessionType {
  kManagedGuestSession = 0,
  kKiosk = 1,
  kUserSession = 2,
};

// IWA config which is used to add IWAs to policy.
class IwaPolicyConfig {
 public:
  explicit IwaPolicyConfig(
      const SignedWebBundleId& bundle_id,
      const std::optional<UpdateChannel>& update_channel = std::nullopt,
      const std::optional<base::Version>& pinned_version = std::nullopt)
      : bundle_id_(bundle_id),
        update_channel_(update_channel),
        pinned_version_(pinned_version) {}

  IwaPolicyConfig(const IwaPolicyConfig& other)
      : bundle_id_(other.bundle_id()),
        update_channel_(other.update_channel()),
        pinned_version_(other.pinned_version()) {}

  ~IwaPolicyConfig() = default;

  const SignedWebBundleId& bundle_id() const { return bundle_id_; }
  const std::optional<UpdateChannel>& update_channel() const {
    return update_channel_;
  }
  const std::optional<base::Version>& pinned_version() const {
    return pinned_version_;
  }

 private:
  const SignedWebBundleId bundle_id_;
  const std::optional<UpdateChannel> update_channel_;
  const std::optional<base::Version> pinned_version_;
};

// This class is used to add an IWA to the update server.
class IwaServerConfig {
 public:
  IwaServerConfig(const SignedWebBundleId& bundle_id,
                  const base::Version& version,
                  const web_package::test::KeyPair& public_key_pair)
      : bundle_id_(bundle_id),
        version_(version),
        public_key_pair_(public_key_pair) {}

  IwaServerConfig(const IwaServerConfig& other)
      : bundle_id_(other.bundle_id()),
        version_(other.version()),
        public_key_pair_(other.public_key_pair()) {}

  ~IwaServerConfig() = default;

  const SignedWebBundleId& bundle_id() const { return bundle_id_; }
  const base::Version& version() const { return version_; }
  const web_package::test::KeyPair& public_key_pair() const {
    return public_key_pair_;
  }

 private:
  const SignedWebBundleId bundle_id_;
  const base::Version version_;
  const web_package::test::KeyPair public_key_pair_;
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
    return true;
  }

  void WaitForMgsLaunch() { ash::test::WaitForPrimaryUserSessionStart(); }

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
  explicit IwaCacheBaseTest(
      const SessionType session_type,
      const std::vector<IwaPolicyConfig>& iwa_policy_configs,
      const std::vector<IwaServerConfig>& add_to_server_iwas)
      : session_type_(session_type),
        iwa_policy_configs_(iwa_policy_configs),
        add_to_server_iwas_(add_to_server_iwas),
        session_mixin_(CreateSessionMixin(session_type_)) {
    scoped_feature_list_.InitWithFeatures(
        {features::kIsolatedWebAppBundleCache,
         features::kIsolatedWebAppManagedGuestSessionInstall,
         features::kIsolatedWebAppManagedAllowlist},
        /*disabled_features=*/{});
  }

  void SetUpOnMainThread() override {
    ash::LoginManagerTest::SetUpOnMainThread();

    for (auto& iwa : add_to_server_iwas_) {
      AddNewIwaToServer(iwa);
    }

    OverrideCacheDir();
    ConfigureSession(iwa_policy_configs_);
    SkipIwaAllowlist(/*skip=*/true);
  }

  void TearDownOnMainThread() override {
    // Each session start, IWA cache manager checks for the updates. Wait for
    // this result to avoid crashes in tests.
    WaitForInitialUpdateDiscoveryTasksToFinish();
    ash::LoginManagerTest::TearDownOnMainThread();
  }

  void WaitForInitialUpdateDiscoveryTasksToFinish() {
    for (auto& update_future : initial_discovery_update_futures_) {
      EXPECT_TRUE(update_future.Wait());
    }
    initial_discovery_update_futures_.clear();
    initial_discovery_update_waiters_.clear();
  }

  virtual void ConfigureSession(
      const IwaPolicyConfig& app_to_configure_in_session) {
    ConfigureSession(std::vector{app_to_configure_in_session});
  }

  void ConfigureSession(
      const std::vector<IwaPolicyConfig>& apps_to_configure_in_session) {
    if (apps_to_configure_in_session.empty()) {
      return;
    }
    std::visit(absl::Overload{
                   [&](MgsMixin& mgs_mixin) {
                     base::Value::List config;
                     for (auto& iwa : apps_to_configure_in_session) {
                       config.Append(iwa_mixin_.CreateForceInstallPolicyEntry(
                           iwa.bundle_id(), iwa.update_channel(),
                           iwa.pinned_version()));
                     }
                     mgs_mixin.ConfigureMgsWithIwa(WriteJson(config).value());
                   },
                   [&](KioskMixin& kiosk_mixin) {
                     ash::ScopedDevicePolicyUpdate scoped_update(
                         policy_helper_.device_policy(),
                         base::BindLambdaForTesting([&]() {
                           policy_helper_
                               .RefreshPolicyAndWaitUntilDeviceSettingsUpdated(
                                   {ash::kAccountsPrefDeviceLocalAccounts});
                         }));

                     for (auto& iwa : apps_to_configure_in_session) {
                       kiosk_mixin.Configure(
                           scoped_update,
                           GetKioskIwaManualLaunchConfig(
                               iwa.bundle_id(),
                               iwa_mixin_.GetUpdateManifestUrl(iwa.bundle_id()),
                               iwa.update_channel(), iwa.pinned_version()));
                     }
                   },
                   [&](LoginManagerMixin& login_manager_mixin) {
                     login_manager_mixin.AppendRegularUsers(1);
                   },
               },
               session_mixin_);
  }

  void LaunchSession(const SignedWebBundleId& expected_iwa,
                     bool should_wait_for_initial_update = true) {
    LaunchSession(std::vector{expected_iwa}, should_wait_for_initial_update);
  }

  // `ConfigureSession` should be called before this function and contain
  // `expected_iwas`. `ConfigureSession` is usually called during the set up.
  void LaunchSession(const std::vector<SignedWebBundleId>& expected_iwas,
                     bool should_wait_for_initial_updates = true) {
    std::visit(
        absl::Overload([](MgsMixin& mgs_mixin) { mgs_mixin.LaunchMgs(); },
                       [&](KioskMixin& kiosk_mixin) {
                         ASSERT_TRUE(expected_iwas.size() == 1)
                             << "Only one app can be launched in kiosk "
                                "session";

                         std::optional<ash::KioskApp> kiosk_app =
                             ash::kiosk::test::GetAppByAccountId(
                                 expected_iwas[0].id());
                         EXPECT_THAT(kiosk_app, Ne(std::nullopt));
                         ASSERT_TRUE(LaunchAppManually(kiosk_app.value()));
                       },
                       [&](LoginManagerMixin& login_manager_mixin) {
                         LoginUser(login_manager_mixin.users()[0].account_id);
                       }),
        session_mixin_);

    if (session_type() != SessionType::kUserSession &&
        should_wait_for_initial_updates) {
      WaitForProfile();

      // The initial update is checked on the session start inside Managed Guest
      // Session and kiosk, initialize the waiter here to avoid race conditions.
      for (auto& iwa : expected_iwas) {
        initial_discovery_update_futures_.emplace_back();
        initial_discovery_update_waiters_.push_back(
            std::make_unique<UpdateDiscoveryTaskResultWaiter>(
                provider(), GetAppId(iwa),
                initial_discovery_update_futures_.back().GetCallback()));
      }
    }

    WaitForSessionLaunch();
  }

  void AssertAppInstalledAtVersion(
      const SignedWebBundleId& bundle_id,
      const base::Version& version,
      const bool wait_for_initial_installation = true) {
    if (IsManagedGuestSession() && wait_for_initial_installation &&
        !GetIsolatedWebApp(bundle_id)) {
      // Wait for the IWA to be installed in MGS. In Kiosk app is already
      // installed when the kiosk is launched.
      WebAppTestInstallObserver observer(profile());
      observer.BeginListeningAndWait({GetAppId(bundle_id)});
    }

    const WebApp* app = GetIsolatedWebApp(bundle_id);
    ASSERT_TRUE(app);
    ASSERT_EQ(app->isolation_data()->version(), version);
  }

  base::FilePath GetCachedBundlePath(const SignedWebBundleId& bundle_id,
                                     const base::Version& version) {
    return GetCachedBundlePath(bundle_id, version, session_type());
  }

  base::FilePath GetCachedBundlePath(const SignedWebBundleId& bundle_id,
                                     const base::Version& version,
                                     const SessionType session_type) {
    return GetCachedBundleDir(bundle_id, version, session_type)
        .AppendASCII(kMainSwbnFileName);
  }

  base::FilePath GetCachedBundleDir(const SignedWebBundleId& bundle_id,
                                    const base::Version& version,
                                    const SessionType session_type) {
    base::FilePath bundle_directory_path = cache_root_dir();
    switch (session_type) {
      case SessionType::kManagedGuestSession:
        bundle_directory_path =
            bundle_directory_path.AppendASCII(IwaCacheClient::kMgsDirName);
        break;
      case SessionType::kKiosk:
        bundle_directory_path =
            bundle_directory_path.AppendASCII(IwaCacheClient::kKioskDirName);
        break;
      case SessionType::kUserSession:
        NOTREACHED()
            << "No cache path since IWAs are not cached in user session.";
    }
    return bundle_directory_path.AppendASCII(bundle_id.id())
        .AppendASCII(version.GetString());
  }

  base::FilePath CreateBundlePath(const SignedWebBundleId& bundle_id,
                                  const base::Version& version,
                                  const SessionType session_type) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath bundle_directory_path =
        GetCachedBundleDir(bundle_id, version, session_type);
    EXPECT_TRUE(base::CreateDirectory(bundle_directory_path));

    base::FilePath temp_file;
    EXPECT_TRUE(base::CreateTemporaryFileInDir(cache_root_dir(), &temp_file));
    base::FilePath bundle_path =
        GetCachedBundlePath(bundle_id, version, session_type);
    EXPECT_TRUE(base::CopyFile(temp_file, bundle_path));
    return bundle_path;
  }

  // Ensures that the follow-up installation is done via cache, since it's not
  // possible to install IWA from the Internet after this function is executed.
  void RemoveAllBundlesFromUpdateServer() {
    for (auto& iwa : add_to_server_iwas_) {
      // Other versions could have been added to the update server after
      // configuring IWAs. We need to remove all of them.
      auto versions = GetVersionsFromUpdateManifest(iwa.bundle_id());
      for (auto version : versions) {
        iwa_mixin_.RemoveBundle(iwa.bundle_id(), version);
      }
    }
  }

  void AddNewIwaToServer(const IwaServerConfig& iwa_server_config,
                         std::optional<std::vector<UpdateChannel>>
                             update_channels = std::nullopt) {
    iwa_mixin_.AddBundle(
        IsolatedWebAppBuilder(ManifestBuilder().SetName(kIwaName).SetVersion(
                                  iwa_server_config.version().GetString()))
            .BuildBundle(iwa_server_config.public_key_pair()),
        std::move(update_channels));
  }

  void OpenIwa(const SignedWebBundleId& bundle_id) {
    OpenIsolatedWebApp(profile(), GetAppId(bundle_id));
  }

  DiscoveryTask::CompletionStatus DiscoverUpdateAndWaitForResult(
      const SignedWebBundleId& bundle_id) {
    UpdateDiscoveryTaskFuture discovery_update_future;
    UpdateDiscoveryTaskResultWaiter discovery_update_waiter(
        provider(), GetAppId(bundle_id), discovery_update_future.GetCallback());

    DiscoverUpdatesNow();
    return discovery_update_future.Get();
  }

  void DiscoverUpdatesNow() {
    EXPECT_THAT(provider().iwa_update_manager().DiscoverUpdatesNow(), Eq(1ul));
  }

  void DestroyCacheDir() { cache_root_dir_override_.reset(); }

  size_t GetNumOpenedWindows(const SignedWebBundleId& bundle_id) {
    return provider().ui_manager().GetNumWindowsForApp(GetAppId(bundle_id));
  }

  void SkipIwaAllowlist(bool skip) {
    IwaKeyDistributionInfoProvider::GetInstance()
        .SkipManagedAllowlistChecksForTesting(skip);
  }

  // To set the allowlist multiple times within one test,
  // `key_distribution_version` should be increased.
  void SetIwasAllowlist(
      const std::vector<SignedWebBundleId>& bundle_ids,
      base::Version key_distribution_version = base::Version("1.0.1")) {
    base::ScopedAllowBlockingForTesting allow_blocking;

    EXPECT_THAT(test::UpdateKeyDistributionInfoWithAllowlist(
                    key_distribution_version,
                    /*managed_allowlist=*/bundle_ids),
                base::test::HasValue());
  }

  void CheckCacheManagerDebugOperationResult(const std::string& operation_name,
                                             const std::string& result) {
    base::Value debug_value = provider().iwa_cache_manager().GetDebugValue();
    base::Value::List* operations_results =
        debug_value.GetDict().FindList(kOperationsResults);
    ASSERT_TRUE(operations_results);
    EXPECT_TRUE(operations_results->contains(
        base::Value::Dict().Set(operation_name, result)));
  }

  void ExpectEmptyCopyBundleAfterUpdateMetric() {
    histogram_tester_.ExpectTotalCount(
        kCopyBundleToCacheAfterUpdateSuccessMetric, 0);
    histogram_tester_.ExpectTotalCount(kCopyBundleToCacheAfterUpdateErrorMetric,
                                       0);
  }

  void ExpectSuccessCopyBundleAfterUpdateMetric() {
    EXPECT_THAT(histogram_tester_.GetAllSamples(
                    kCopyBundleToCacheAfterUpdateSuccessMetric),
                BucketsAre(base::Bucket(true, 1)));
    histogram_tester_.ExpectTotalCount(kCopyBundleToCacheAfterUpdateErrorMetric,
                                       0);
  }

  void ExpectErrorCopyBundleAfterUpdateMetric(
      const CopyBundleToCacheError& error) {
    EXPECT_THAT(histogram_tester_.GetAllSamples(
                    kCopyBundleToCacheAfterUpdateSuccessMetric),
                BucketsAre(base::Bucket(false, 1)));
    EXPECT_THAT(histogram_tester_.GetAllSamples(
                    kCopyBundleToCacheAfterUpdateErrorMetric),
                BucketsAre(base::Bucket(error, 1)));
  }

  WebAppProvider& provider() {
    auto* provider = WebAppProvider::GetForTest(profile());
    CHECK(provider);
    return *provider;
  }

  webapps::AppId GetAppId(const SignedWebBundleId& bundle_id) const {
    return IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(bundle_id)
        .app_id();
  }

  bool IsManagedGuestSession() const {
    return session_type() == SessionType::kManagedGuestSession;
  }

  SessionType session_type() const { return session_type_; }

  const base::FilePath& cache_root_dir() const { return cache_root_dir_; }

 protected:
  void WaitForSessionLaunch() {
    std::visit(
        absl::Overload(
            [](MgsMixin& mgs_mixin) { mgs_mixin.WaitForMgsLaunch(); },
            [](KioskMixin& kiosk_mixin) { ASSERT_TRUE(WaitKioskLaunched()); },
            [](LoginManagerMixin& login_manager_mixin) {
              WaitForUserSessionLaunch();
            }),
        session_mixin_);
  }

  std::vector<base::Version> GetVersionsFromUpdateManifest(
      const SignedWebBundleId& bundle_id) {
    std::vector<base::Version> versions;

    base::Value::Dict manifest_dict = iwa_mixin_.GetUpdateManifest(bundle_id);
    for (auto& version_value :
         CHECK_DEREF(manifest_dict.FindList("versions"))) {
      auto& version_dict = CHECK_DEREF(version_value.GetIfDict());
      versions.emplace_back(CHECK_DEREF(version_dict.FindString("version")));
    }
    return versions;
  }

  const WebApp* GetIsolatedWebApp(const SignedWebBundleId& bundle_id) {
    ASSIGN_OR_RETURN(const WebApp& iwa,
                     GetIsolatedWebAppById(provider().registrar_unsafe(),
                                           GetAppId(bundle_id)),
                     [](const std::string&) { return nullptr; });
    return &iwa;
  }

  std::variant<MgsMixin, KioskMixin, LoginManagerMixin> CreateSessionMixin(
      SessionType session_type) {
    switch (session_type) {
      case SessionType::kManagedGuestSession:
        return std::variant<MgsMixin, KioskMixin, LoginManagerMixin>{
            std::in_place_type<MgsMixin>, &mixin_host_};
      case SessionType::kKiosk:
        return std::variant<MgsMixin, KioskMixin, LoginManagerMixin>{
            std::in_place_type<KioskMixin>, &mixin_host_};
      case SessionType::kUserSession:
        return std::variant<MgsMixin, KioskMixin, LoginManagerMixin>{
            std::in_place_type<LoginManagerMixin>, &mixin_host_};
        ;
    }
  }

  void OverrideCacheDir() {
    ProfileManager* profile_manager = g_browser_process->profile_manager();
    ASSERT_TRUE(profile_manager);
    cache_root_dir_ = profile_manager->user_data_dir();
    cache_root_dir_override_ = std::make_unique<base::ScopedPathOverride>(
        ash::DIR_DEVICE_LOCAL_ACCOUNT_IWA_CACHE, cache_root_dir_);
  }

  Profile* profile() {
    // Any profile can be used here since this test does not test multi profile.
    return ProfileManager::GetActiveUserProfile();
  }

  base::HistogramTester histogram_tester_;
  const SessionType session_type_;
  // `bundle_id`s should be unique in `iwa_policy_configs_`.
  const std::vector<IwaPolicyConfig> iwa_policy_configs_;
  const std::vector<IwaServerConfig> add_to_server_iwas_;
  IsolatedWebAppUpdateServerMixin iwa_mixin_{&mixin_host_};
  base::test::ScopedFeatureList scoped_feature_list_;
  policy::DevicePolicyCrosTestHelper policy_helper_;
  base::FilePath cache_root_dir_;
  std::unique_ptr<base::ScopedPathOverride> cache_root_dir_override_;
  std::variant<MgsMixin, KioskMixin, LoginManagerMixin> session_mixin_;
  std::vector<UpdateDiscoveryTaskFuture> initial_discovery_update_futures_;
  std::vector<std::unique_ptr<UpdateDiscoveryTaskResultWaiter>>
      initial_discovery_update_waiters_;
};

class IwaCacheOneAppTest : public IwaCacheBaseTest,
                           public testing::WithParamInterface<SessionType> {
 public:
  IwaCacheOneAppTest()
      : IwaCacheBaseTest(
            GetParam(),
            {IwaPolicyConfig{kWebBundleId}},
            /*add_to_server_iwas=*/
            {IwaServerConfig{kWebBundleId, kBaseVersion, kPublicKeyPair}}) {}
};

IN_PROC_BROWSER_TEST_P(IwaCacheOneAppTest, PRE_InstallIsolatedWebAppFromCache) {
  LaunchSession(kWebBundleId);
  AssertAppInstalledAtVersion(kWebBundleId, kBaseVersion);

  // Checks that bundle is copied to cache after the successful installation.
  WaitUntilPathExists(GetCachedBundlePath(kWebBundleId, kBaseVersion));
}

IN_PROC_BROWSER_TEST_P(IwaCacheOneAppTest, InstallIsolatedWebAppFromCache) {
  // Checks that the bundle is still in cache from the PRE test.
  CheckPathExists(GetCachedBundlePath(kWebBundleId, kBaseVersion));

  // Make sure the IWA is installed from the cache.
  RemoveAllBundlesFromUpdateServer();
  LaunchSession(kWebBundleId);
  AssertAppInstalledAtVersion(kWebBundleId, kBaseVersion);
}

IN_PROC_BROWSER_TEST_P(IwaCacheOneAppTest,
                       PRE_UpdateApplyTaskFinishedOnSessionExit) {
  LaunchSession(kWebBundleId);
  AssertAppInstalledAtVersion(kWebBundleId, kBaseVersion);
  WaitUntilPathExists(GetCachedBundlePath(kWebBundleId, kBaseVersion));
  if (IsManagedGuestSession()) {
    // Only open app in MGS, in kiosk app is always opened after the session
    // started.
    OpenIwa(kWebBundleId);
  }
  // When app is opened, the update cannot be applied, so it will be applied on
  // session exit.
  EXPECT_THAT(GetNumOpenedWindows(kWebBundleId), Eq(1ul));

  // Before triggering new update, wait for the initial update check.
  WaitForInitialUpdateDiscoveryTasksToFinish();
  AddNewIwaToServer(
      IwaServerConfig{kWebBundleId, kUpdateVersion, kPublicKeyPair});

  EXPECT_THAT(DiscoverUpdateAndWaitForResult(kWebBundleId),
              ValueIs(DiscoveryTask::Success::kUpdateFoundAndSavedInDatabase));
  CheckPathDoesNotExist(GetCachedBundlePath(kWebBundleId, kUpdateVersion));
}

// Checks that on session exit in PRE_ test, pending update apply task is
// successfully finished and it updated the cache.
IN_PROC_BROWSER_TEST_P(IwaCacheOneAppTest,
                       UpdateApplyTaskFinishedOnSessionExit) {
  CheckPathExists(GetCachedBundlePath(kWebBundleId, kBaseVersion));
  CheckPathExists(GetCachedBundlePath(kWebBundleId, kUpdateVersion));

  RemoveAllBundlesFromUpdateServer();
  LaunchSession(kWebBundleId);

  AssertAppInstalledAtVersion(kWebBundleId, kUpdateVersion);
  // After session start the previously cached bundle version should be deleted.
  WaitUntilPathDoesNotExist(GetCachedBundlePath(kWebBundleId, kBaseVersion));
  CheckPathExists(GetCachedBundlePath(kWebBundleId, kUpdateVersion));
  CheckCacheManagerDebugOperationResult(
      kRemoveObsoleteIwaVersionCache,
      "Successfully finished versions cleanup, number of removed obsolete "
      "versions: 1");
}

IN_PROC_BROWSER_TEST_P(IwaCacheOneAppTest, PRE_UpdateNotFound) {
  LaunchSession(kWebBundleId);
  AssertAppInstalledAtVersion(kWebBundleId, kBaseVersion);
  WaitUntilPathExists(GetCachedBundlePath(kWebBundleId, kBaseVersion));
  if (IsManagedGuestSession()) {
    // Only open app in MGS, in kiosk app is always opened after the session
    // started.
    OpenIwa(kWebBundleId);
  }
  // When app is opened, the update cannot be applied, so it will be applied on
  // session exit.
  EXPECT_THAT(GetNumOpenedWindows(kWebBundleId), Eq(1ul));

  EXPECT_THAT(DiscoverUpdateAndWaitForResult(kWebBundleId),
              ValueIs(DiscoveryTask::Success::kNoUpdateFound));
}

// In PRE_ test, update discovery task did not find the update, check that the
// cache was not updated on the session exit.
IN_PROC_BROWSER_TEST_P(IwaCacheOneAppTest, UpdateNotFound) {
  CheckPathExists(GetCachedBundlePath(kWebBundleId, kBaseVersion));
  CheckPathDoesNotExist(GetCachedBundlePath(kWebBundleId, kUpdateVersion));

  RemoveAllBundlesFromUpdateServer();
  LaunchSession(kWebBundleId);

  AssertAppInstalledAtVersion(kWebBundleId, kBaseVersion);
}

// Install base version from the Internet.
IN_PROC_BROWSER_TEST_P(IwaCacheOneAppTest,
                       PRE_PRE_UpdateTaskIsTriggeredAutomatically) {
  LaunchSession(kWebBundleId);
  AssertAppInstalledAtVersion(kWebBundleId, kBaseVersion);
  WaitUntilPathExists(GetCachedBundlePath(kWebBundleId, kBaseVersion));
  CheckPathDoesNotExist(GetCachedBundlePath(kWebBundleId, kUpdateVersion));
}

// Add new version to the manifest, but the installation will be done from cache
// with the base version first. Then the IWA cache manager will
// automatically trigger the update check. On the session exit the new
// version will be copied
// to cache. On the 3rd session start new IWA version will be installed.
IN_PROC_BROWSER_TEST_P(IwaCacheOneAppTest,
                       PRE_UpdateTaskIsTriggeredAutomatically) {
  AddNewIwaToServer(
      IwaServerConfig{kWebBundleId, kUpdateVersion, kPublicKeyPair});
  LaunchSession(kWebBundleId);

  AssertAppInstalledAtVersion(kWebBundleId, kBaseVersion);
  if (IsManagedGuestSession()) {
    // Only open app in MGS, in kiosk app is always opened after the session
    // started.
    OpenIwa(kWebBundleId);
  }
}

IN_PROC_BROWSER_TEST_P(IwaCacheOneAppTest, UpdateTaskIsTriggeredAutomatically) {
  CheckPathExists(GetCachedBundlePath(kWebBundleId, kUpdateVersion));

  RemoveAllBundlesFromUpdateServer();
  LaunchSession(kWebBundleId);

  AssertAppInstalledAtVersion(kWebBundleId, kUpdateVersion);
  // After session start the previously cached bundle version should be deleted.
  WaitUntilPathDoesNotExist(GetCachedBundlePath(kWebBundleId, kBaseVersion));
  CheckPathExists(GetCachedBundlePath(kWebBundleId, kUpdateVersion));
}

IN_PROC_BROWSER_TEST_P(IwaCacheOneAppTest, GetDebugValue) {
  LaunchSession(kWebBundleId);
  AssertAppInstalledAtVersion(kWebBundleId, kBaseVersion);
  WaitUntilPathExists(GetCachedBundlePath(kWebBundleId, kBaseVersion));

  base::Value debug_value = provider().iwa_cache_manager().GetDebugValue();
  EXPECT_EQ(debug_value.GetDict().FindBool(kBundleCacheIsEnabled), true);
  EXPECT_NE(debug_value.GetDict().Find(kOperationsResults), nullptr);
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    IwaCacheOneAppTest,
    testing::Values(SessionType::kManagedGuestSession, SessionType::kKiosk));

// This test class is made for cases when session configuration need to be
// different from the one in `IwaCacheBaseTest`. Call `ConfigureSession` in
// tests with specified parameters.
class IwaCacheNonConfiguredMgsSessionTest : public IwaCacheBaseTest {
 public:
  IwaCacheNonConfiguredMgsSessionTest()
      : IwaCacheBaseTest(SessionType::kManagedGuestSession,
                         /*iwa_policy_configs=*/{},
                         /*add_to_server_iwas=*/{}) {}
};

IN_PROC_BROWSER_TEST_F(IwaCacheNonConfiguredMgsSessionTest,
                       PRE_RemoveCachedBundleForUninstalledIwa) {
  ConfigureSession(IwaPolicyConfig{kWebBundleId});
  AddNewIwaToServer(
      IwaServerConfig{kWebBundleId, kBaseVersion, kPublicKeyPair});
  LaunchSession(kWebBundleId);

  AssertAppInstalledAtVersion(kWebBundleId, kBaseVersion);
  WaitUntilPathExists(GetCachedBundlePath(kWebBundleId, kBaseVersion));
}

// When IWA is no longer in the policy list, `IwaCacheManager` will remove
// it's cache on session start.
IN_PROC_BROWSER_TEST_F(IwaCacheNonConfiguredMgsSessionTest,
                       RemoveCachedBundleForUninstalledIwa) {
  AddNewIwaToServer(
      IwaServerConfig{kWebBundleId2, kBaseVersion, kPublicKeyPair2});
  ConfigureSession(IwaPolicyConfig{kWebBundleId2});
  LaunchSession(kWebBundleId2);

  AssertAppInstalledAtVersion(kWebBundleId2, kBaseVersion);

  // Cache for `kWebBundleId` should be removed.
  WaitUntilPathDoesNotExist(GetCachedBundlePath(kWebBundleId, kBaseVersion));
  CheckCacheManagerDebugOperationResult(
      kCleanupManagedGuestSessionOrphanedIwas,
      "Successfully finished cleanup, number of cleaned up directories: 1");
}

IN_PROC_BROWSER_TEST_F(IwaCacheNonConfiguredMgsSessionTest,
                       PRE_RemoveTwoCachedBundles) {
  SkipIwaAllowlist(/*skip=*/false);
  SetIwasAllowlist({kWebBundleId, kWebBundleId2});

  ConfigureSession(
      {IwaPolicyConfig{kWebBundleId}, IwaPolicyConfig{kWebBundleId2}});
  AddNewIwaToServer(
      IwaServerConfig{kWebBundleId, kBaseVersion, kPublicKeyPair});
  AddNewIwaToServer(
      IwaServerConfig{kWebBundleId2, kBaseVersion, kPublicKeyPair2});

  LaunchSession({kWebBundleId, kWebBundleId2});

  AssertAppInstalledAtVersion(kWebBundleId, kBaseVersion);
  AssertAppInstalledAtVersion(kWebBundleId2, kBaseVersion);

  WaitUntilPathExists(GetCachedBundlePath(kWebBundleId, kBaseVersion));
  WaitUntilPathExists(GetCachedBundlePath(kWebBundleId, kBaseVersion));
}

// `kWebBundleId` is no longer in the policy list --> remove from cache.
// `kWebBundleId2` is no longer in the allowlist --> remove from cache.
IN_PROC_BROWSER_TEST_F(IwaCacheNonConfiguredMgsSessionTest,
                       RemoveTwoCachedBundles) {
  SkipIwaAllowlist(/*skip=*/false);
  SetIwasAllowlist({kWebBundleId});
  AddNewIwaToServer(
      IwaServerConfig{kWebBundleId2, kBaseVersion, kPublicKeyPair2});
  ConfigureSession(IwaPolicyConfig{kWebBundleId2});
  LaunchSession(/*expected_iwas=*/{});

  WaitUntilPathDoesNotExist(GetCachedBundlePath(kWebBundleId, kBaseVersion));
  WaitUntilPathDoesNotExist(GetCachedBundlePath(kWebBundleId2, kBaseVersion));
}

// Covers Managed Guest Session (MGS) specific tests which cannot be tested in
// kiosk. For example, kiosk always launch the IWA app, but in MGS it is
// possible to open and close the app inside the sessions.
class IwaCacheMgsTest : public IwaCacheBaseTest {
 public:
  IwaCacheMgsTest()
      : IwaCacheBaseTest(
            SessionType::kManagedGuestSession,
            {IwaPolicyConfig{kWebBundleId}},
            /*add_to_server_iwas=*/
            {IwaServerConfig{kWebBundleId, kBaseVersion, kPublicKeyPair}}) {}

  void CloseApp(const SignedWebBundleId& bundle_id) {
    TestFuture<void> app_closed_future;
    provider().ui_manager().NotifyOnAllAppWindowsClosed(
        GetAppId(bundle_id), app_closed_future.GetCallback());
    provider().ui_manager().CloseAppWindows(GetAppId(bundle_id));
    EXPECT_TRUE(app_closed_future.Wait());
    EXPECT_THAT(GetNumOpenedWindows(bundle_id), Eq(0ul));
  }
};

IN_PROC_BROWSER_TEST_F(IwaCacheMgsTest, UpdateAppWhenAppNotOpened) {
  LaunchSession(kWebBundleId);
  AssertAppInstalledAtVersion(kWebBundleId, kBaseVersion);
  WaitUntilPathExists(GetCachedBundlePath(kWebBundleId, kBaseVersion));

  WaitForInitialUpdateDiscoveryTasksToFinish();
  AddNewIwaToServer(
      IwaServerConfig{kWebBundleId, kUpdateVersion, kPublicKeyPair});
  UpdateApplyTaskFuture apply_update_future;
  UpdateApplyTaskResultWaiter apply_update_waiter(
      provider(), GetAppId(kWebBundleId), apply_update_future.GetCallback());
  DiscoverUpdatesNow();

  EXPECT_THAT(apply_update_future.Get(), HasValue());
  AssertAppInstalledAtVersion(kWebBundleId, kUpdateVersion,
                              /*wait_for_initial_installation=*/false);
  CheckPathExists(GetCachedBundlePath(kWebBundleId, kUpdateVersion));
  ExpectSuccessCopyBundleAfterUpdateMetric();
}

IN_PROC_BROWSER_TEST_F(IwaCacheMgsTest, UpdateApplyTaskWhenAppClosed) {
  LaunchSession(kWebBundleId);
  AssertAppInstalledAtVersion(kWebBundleId, kBaseVersion);
  WaitUntilPathExists(GetCachedBundlePath(kWebBundleId, kBaseVersion));

  OpenIwa(kWebBundleId);
  EXPECT_THAT(GetNumOpenedWindows(kWebBundleId), Eq(1ul));
  WaitForInitialUpdateDiscoveryTasksToFinish();
  AddNewIwaToServer(
      IwaServerConfig{kWebBundleId, kUpdateVersion, kPublicKeyPair});

  // Updates will be applied once the app's window is closed.
  CloseApp(kWebBundleId);

  UpdateApplyTaskFuture apply_update_future;
  UpdateApplyTaskResultWaiter apply_update_waiter(
      provider(), GetAppId(kWebBundleId), apply_update_future.GetCallback());
  DiscoverUpdatesNow();

  EXPECT_THAT(apply_update_future.Get(), HasValue());
  AssertAppInstalledAtVersion(kWebBundleId, kUpdateVersion,
                              /*wait_for_initial_installation=*/false);
  CheckPathExists(GetCachedBundlePath(kWebBundleId, kUpdateVersion));
}

IN_PROC_BROWSER_TEST_F(IwaCacheMgsTest, CopyToCacheFailed) {
  ExpectEmptyCopyBundleAfterUpdateMetric();
  LaunchSession(kWebBundleId);
  AssertAppInstalledAtVersion(kWebBundleId, kBaseVersion);
  WaitUntilPathExists(GetCachedBundlePath(kWebBundleId, kBaseVersion));

  WaitForInitialUpdateDiscoveryTasksToFinish();
  AddNewIwaToServer(
      IwaServerConfig{kWebBundleId, kUpdateVersion, kPublicKeyPair});
  DestroyCacheDir();

  UpdateApplyTaskFuture apply_update_future;
  UpdateApplyTaskResultWaiter apply_update_waiter(
      provider(), GetAppId(kWebBundleId), apply_update_future.GetCallback());
  EXPECT_THAT(DiscoverUpdateAndWaitForResult(kWebBundleId),
              ValueIs(DiscoveryTask::Success::kUpdateFoundAndSavedInDatabase));

  // The update is applied, but it was not saved to cache because of the error
  // during copying to cache.
  EXPECT_THAT(apply_update_future.Get(),
              ErrorIs(Field(&IsolatedWebAppApplyUpdateCommandError::message,
                            HasSubstr(ApplyTask::kCopyToCacheFailedMessage))));
  AssertAppInstalledAtVersion(kWebBundleId, kUpdateVersion,
                              /*wait_for_initial_installation=*/false);
  CheckPathDoesNotExist(GetCachedBundlePath(kWebBundleId, kUpdateVersion));
  ExpectErrorCopyBundleAfterUpdateMetric(
      CopyBundleToCacheError::kFailedToCreateDir);
}

// Class to test that Managed Guest Session (MGS) and kiosk cache is cleaned
// during the next (even user) session start when MGS or kiosk are not
// configured anymore.
class IwaCacheCrossSessionCleanupTest
    : public IwaCacheBaseTest,
      public testing::WithParamInterface<SessionType> {
 public:
  IwaCacheCrossSessionCleanupTest()
      : IwaCacheBaseTest(
            GetParam(),
            {IwaPolicyConfig{kWebBundleId}},
            /*add_to_server_iwas=*/
            {IwaServerConfig{kWebBundleId, kBaseVersion, kPublicKeyPair}}) {}
};

IN_PROC_BROWSER_TEST_P(IwaCacheCrossSessionCleanupTest,
                       RemoveObsoleteKioskIwaCache) {
  base::FilePath kiosk_bundle =
      CreateBundlePath(kWebBundleId2, kUpdateVersion, SessionType::kKiosk);

  LaunchSession(kWebBundleId);

  WaitUntilPathDoesNotExist(kiosk_bundle);
  CheckCacheManagerDebugOperationResult(
      kRemoveCacheForIwaKioskDeletedFromPolicy,
      "Successfully finished cleanup, number of cleaned up directories: 1");
}

IN_PROC_BROWSER_TEST_P(IwaCacheCrossSessionCleanupTest,
                       RemoveTwoObsoleteKioskIwaCaches) {
  base::FilePath kiosk_bundle1 =
      CreateBundlePath(kWebBundleId2, kBaseVersion, SessionType::kKiosk);
  base::FilePath kiosk_bundle2 =
      CreateBundlePath(kWebBundleId2, kUpdateVersion, SessionType::kKiosk);

  LaunchSession(kWebBundleId);

  WaitUntilPathDoesNotExist(kiosk_bundle1);
  WaitUntilPathDoesNotExist(kiosk_bundle2);
}

IN_PROC_BROWSER_TEST_P(IwaCacheCrossSessionCleanupTest,
                       RemoveObsoleteMgsCache) {
  if (IsManagedGuestSession()) {
    // MGS is cleaned only if it is not configured.
    return;
  }
  base::FilePath mgs_bundle = CreateBundlePath(
      kWebBundleId2, kUpdateVersion, SessionType::kManagedGuestSession);

  LaunchSession(kWebBundleId);

  WaitUntilPathDoesNotExist(mgs_bundle);
  CheckCacheManagerDebugOperationResult(
      kRemoveManagedGuestSessionCache,
      "Successfully finished cleanup, number of cleaned up directories: 1");
}

IN_PROC_BROWSER_TEST_P(IwaCacheCrossSessionCleanupTest,
                       RemoveObsoleteMgsAndKioskCache) {
  base::FilePath mgs_bundle = CreateBundlePath(
      kWebBundleId2, kUpdateVersion, SessionType::kManagedGuestSession);
  base::FilePath kiosk_bundle =
      CreateBundlePath(kWebBundleId2, kBaseVersion, SessionType::kKiosk);

  LaunchSession(kWebBundleId);

  WaitUntilPathDoesNotExist(mgs_bundle);
  WaitUntilPathDoesNotExist(kiosk_bundle);
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    IwaCacheCrossSessionCleanupTest,
    testing::Values(SessionType::kManagedGuestSession,
                    SessionType::kKiosk,
                    SessionType::kUserSession));

// Covers Kiosk specific tests which cannot be tested in other sessions.
class IwaCacheKioskTest : public IwaCacheBaseTest {
 public:
  IwaCacheKioskTest()
      : IwaCacheBaseTest(
            SessionType::kKiosk,
            {IwaPolicyConfig{kWebBundleId}},
            /*add_to_server_iwas=*/
            {IwaServerConfig{kWebBundleId, kBaseVersion, kPublicKeyPair}}) {}

  void SetUpInProcessBrowserTestFixture() override {
    IwaCacheBaseTest::SetUpInProcessBrowserTestFixture();
    provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
  }

  void DisableKioskOfflineLaunch() {
    policy::PolicyMap values;
    values.Set(policy::key::kKioskWebAppOfflineEnabled,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD, base::Value(false), nullptr);
    provider_.UpdateChromePolicy(values);
  }

 protected:
  testing::NiceMock<policy::MockConfigurationPolicyProvider> provider_;
  ash::NetworkStateMixin network_state_{&mixin_host_};
};

IN_PROC_BROWSER_TEST_F(IwaCacheKioskTest, PRE_OfflineLaunchFromCache) {
  network_state_.SimulateOnline();
  ASSERT_TRUE(LaunchAppManually(TheKioskApp()));

  ASSERT_TRUE(WaitKioskLaunched());
  AssertAppInstalledAtVersion(kWebBundleId, kBaseVersion);
  WaitUntilPathExists(GetCachedBundlePath(kWebBundleId, kBaseVersion));
}

IN_PROC_BROWSER_TEST_F(IwaCacheKioskTest, OfflineLaunchFromCache) {
  CheckPathExists(GetCachedBundlePath(kWebBundleId, kBaseVersion));
  network_state_.SimulateOffline();
  RemoveAllBundlesFromUpdateServer();

  ASSERT_TRUE(LaunchAppManually(TheKioskApp()));

  ASSERT_TRUE(WaitKioskLaunched());
  AssertAppInstalledAtVersion(kWebBundleId, kBaseVersion);
}

IN_PROC_BROWSER_TEST_F(IwaCacheKioskTest,
                       PRE_DoNotLaunchFromCacheWhenDisabledByPolicy) {
  network_state_.SimulateOnline();
  ASSERT_TRUE(LaunchAppManually(TheKioskApp()));

  ASSERT_TRUE(WaitKioskLaunched());
  AssertAppInstalledAtVersion(kWebBundleId, kBaseVersion);
  WaitUntilPathExists(GetCachedBundlePath(kWebBundleId, kBaseVersion));
}

// By default `KioskWebAppOfflineEnabled` policy is enabled, this test checks
// when the policy is disabled and the device is offline, the app will not be
// installed from cache, but the device will show the network dialog.
IN_PROC_BROWSER_TEST_F(IwaCacheKioskTest,
                       DoNotLaunchFromCacheWhenDisabledByPolicy) {
  CheckPathExists(GetCachedBundlePath(kWebBundleId, kBaseVersion));
  network_state_.SimulateOffline();
  RemoveAllBundlesFromUpdateServer();
  DisableKioskOfflineLaunch();
  ASSERT_TRUE(LaunchAppManually(TheKioskApp()));

  WaitNetworkScreen();

  network_state_.SimulateOnline();
  AddNewIwaToServer(
      IwaServerConfig{kWebBundleId, kBaseVersion, kPublicKeyPair});

  ASSERT_TRUE(WaitKioskLaunched());
}

// This test times out on ASan / LSan:
// https://ci.chromium.org/ui/p/chromium/builders/ci/Linux%20Chromium%20OS%20ASan%20LSan%20Tests%20(1)/65295/overview
// and on a (less exotic) Linux CQ bot:
// https://ci.chromium.org/ui/p/chromium/builders/ci/linux-chromeos-dbg/41086/overview
// Cache is not available, the network dialog should be shown.
IN_PROC_BROWSER_TEST_F(IwaCacheKioskTest,
                       DISABLED_ShowNetworkDialogWhenLaunchFromCacheFailed) {
  CheckPathDoesNotExist(GetCachedBundlePath(kWebBundleId, kBaseVersion));
  network_state_.SimulateOffline();
  RemoveAllBundlesFromUpdateServer();
  ASSERT_TRUE(LaunchAppManually(TheKioskApp()));

  WaitNetworkScreen();

  network_state_.SimulateOnline();
  AddNewIwaToServer(
      IwaServerConfig{kWebBundleId, kBaseVersion, kPublicKeyPair});
  ASSERT_TRUE(WaitKioskLaunched());
}

class IwaCacheMultipleAppsConfigurationMgs : public IwaCacheBaseTest {
 public:
  IwaCacheMultipleAppsConfigurationMgs()
      : IwaCacheBaseTest(
            SessionType::kManagedGuestSession,
            {IwaPolicyConfig{kWebBundleId}, IwaPolicyConfig{kWebBundleId2}},
            /*add_to_server_iwas=*/
            {IwaServerConfig{kWebBundleId, kBaseVersion, kPublicKeyPair},
             IwaServerConfig{kWebBundleId2, kBaseVersion, kPublicKeyPair2}}) {}
};

IN_PROC_BROWSER_TEST_F(IwaCacheMultipleAppsConfigurationMgs, TwoAppsAreCached) {
  LaunchSession({kWebBundleId, kWebBundleId2});
  AssertAppInstalledAtVersion(kWebBundleId, kBaseVersion);
  AssertAppInstalledAtVersion(kWebBundleId2, kBaseVersion);

  WaitUntilPathExists(GetCachedBundlePath(kWebBundleId, kBaseVersion));
  WaitUntilPathExists(GetCachedBundlePath(kWebBundleId2, kBaseVersion));
}

IN_PROC_BROWSER_TEST_F(IwaCacheMultipleAppsConfigurationMgs,
                       PRE_RemoveNotAllowlistedIwa) {
  SkipIwaAllowlist(/*skip=*/false);
  SetIwasAllowlist({kWebBundleId, kWebBundleId2});
  LaunchSession({kWebBundleId, kWebBundleId2});
  AssertAppInstalledAtVersion(kWebBundleId, kBaseVersion);
  AssertAppInstalledAtVersion(kWebBundleId2, kBaseVersion);

  WaitUntilPathExists(GetCachedBundlePath(kWebBundleId, kBaseVersion));
  WaitUntilPathExists(GetCachedBundlePath(kWebBundleId2, kBaseVersion));
}

IN_PROC_BROWSER_TEST_F(IwaCacheMultipleAppsConfigurationMgs,
                       RemoveNotAllowlistedIwa) {
  SkipIwaAllowlist(/*skip=*/false);
  SetIwasAllowlist({kWebBundleId});
  LaunchSession({kWebBundleId});
  AssertAppInstalledAtVersion(kWebBundleId, kBaseVersion);

  WaitUntilPathDoesNotExist(GetCachedBundlePath(kWebBundleId2, kBaseVersion));
  CheckPathExists(GetCachedBundlePath(kWebBundleId, kBaseVersion));
}

class IwaCacheMultipleAppsConfigurationKiosk : public IwaCacheBaseTest {
 public:
  IwaCacheMultipleAppsConfigurationKiosk()
      : IwaCacheBaseTest(
            SessionType::kKiosk,
            {IwaPolicyConfig{kWebBundleId}, IwaPolicyConfig{kWebBundleId2}},
            /*add_to_server_iwas=*/
            {IwaServerConfig{kWebBundleId, kBaseVersion, kPublicKeyPair},
             IwaServerConfig{kWebBundleId2, kBaseVersion, kPublicKeyPair2}}) {}
};

IN_PROC_BROWSER_TEST_F(IwaCacheMultipleAppsConfigurationKiosk,
                       PRE_TwoAppsAreCached) {
  LaunchSession(kWebBundleId);
  AssertAppInstalledAtVersion(kWebBundleId, kBaseVersion);

  WaitUntilPathExists(GetCachedBundlePath(kWebBundleId, kBaseVersion));
}

IN_PROC_BROWSER_TEST_F(IwaCacheMultipleAppsConfigurationKiosk,
                       TwoAppsAreCached) {
  LaunchSession(kWebBundleId2);
  AssertAppInstalledAtVersion(kWebBundleId2, kBaseVersion);

  CheckPathExists(GetCachedBundlePath(kWebBundleId, kBaseVersion));
  WaitUntilPathExists(GetCachedBundlePath(kWebBundleId2, kBaseVersion));
}

IN_PROC_BROWSER_TEST_F(IwaCacheMultipleAppsConfigurationKiosk,
                       PRE_RemoveNotAllowlistedIwa) {
  SkipIwaAllowlist(/*skip=*/false);
  SetIwasAllowlist({kWebBundleId});

  LaunchSession(kWebBundleId);
  AssertAppInstalledAtVersion(kWebBundleId, kBaseVersion);

  WaitUntilPathExists(GetCachedBundlePath(kWebBundleId, kBaseVersion));
}

IN_PROC_BROWSER_TEST_F(IwaCacheMultipleAppsConfigurationKiosk,
                       RemoveNotAllowlistedIwa) {
  SkipIwaAllowlist(/*skip=*/false);
  SetIwasAllowlist({kWebBundleId2});

  LaunchSession({kWebBundleId2});
  AssertAppInstalledAtVersion(kWebBundleId2, kBaseVersion);

  WaitUntilPathDoesNotExist(GetCachedBundlePath(kWebBundleId, kBaseVersion));
  WaitUntilPathExists(GetCachedBundlePath(kWebBundleId2, kBaseVersion));
}

class IwaCacheVersionManagementTest
    : public IwaCacheBaseTest,
      public testing::WithParamInterface<SessionType> {
 public:
  IwaCacheVersionManagementTest()
      : IwaCacheBaseTest(GetParam(),
                         /*iwa_policy_configs=*/{},
                         /*add_to_server_iwas=*/{}) {}
};

IN_PROC_BROWSER_TEST_P(IwaCacheVersionManagementTest,
                       PRE_InstallPinnedVersionFromCache) {
  ConfigureSession(IwaPolicyConfig{kWebBundleId,
                                   /*update_channel=*/std::nullopt,
                                   /*pinned_version=*/kBaseVersion});
  AddNewIwaToServer(
      IwaServerConfig{kWebBundleId, kBaseVersion, kPublicKeyPair});

  LaunchSession(kWebBundleId, /*should_wait_for_initial_update=*/false);
  AssertAppInstalledAtVersion(kWebBundleId, kBaseVersion);
  WaitUntilPathExists(GetCachedBundlePath(kWebBundleId, kBaseVersion));
}

IN_PROC_BROWSER_TEST_P(IwaCacheVersionManagementTest,
                       InstallPinnedVersionFromCache) {
  // Add `kUpdateVersion` to cache to check that the installation does not use
  // `kUpdateVersion` version from cache since it is not pinned.
  CreateBundlePath(kWebBundleId, kUpdateVersion, session_type());
  ConfigureSession(IwaPolicyConfig{kWebBundleId,
                                   /*update_channel=*/std::nullopt,
                                   /*pinned_version=*/kBaseVersion});

  // When the version is pinned, the initial update is not performed, so do not
  // wait for the result as usual.
  LaunchSession(kWebBundleId, /*should_wait_for_initial_update=*/false);

  // Install pinned version from the cache.
  AssertAppInstalledAtVersion(kWebBundleId, kBaseVersion);
}

IN_PROC_BROWSER_TEST_P(IwaCacheVersionManagementTest,
                       InstallFromInternetWhenPinnedVersionNotCached) {
  // Add `kUpdateVersion` to cache, but IWA installation should choose
  // `kBaseVersion` from the PRE_ test because `kBaseVersion` is pinned.
  CreateBundlePath(kWebBundleId, kUpdateVersion, session_type());
  ConfigureSession(IwaPolicyConfig{kWebBundleId,
                                   /*update_channel=*/std::nullopt,
                                   /*pinned_version=*/kBaseVersion});
  AddNewIwaToServer(
      IwaServerConfig{kWebBundleId, kBaseVersion, kPublicKeyPair});

  // When the version is pinned, the initial update is not performed, so do not
  // wait for the result as usual.
  LaunchSession(kWebBundleId, /*should_wait_for_initial_update=*/false);

  AssertAppInstalledAtVersion(kWebBundleId, kBaseVersion);
}

IN_PROC_BROWSER_TEST_P(IwaCacheVersionManagementTest, PRE_IntallNewestVersion) {
  ConfigureSession(IwaPolicyConfig{kWebBundleId});
  AddNewIwaToServer(
      IwaServerConfig{kWebBundleId, kUpdateVersion, kPublicKeyPair});

  LaunchSession(kWebBundleId);
  AssertAppInstalledAtVersion(kWebBundleId, kUpdateVersion);
  WaitUntilPathExists(GetCachedBundlePath(kWebBundleId, kUpdateVersion));
}

IN_PROC_BROWSER_TEST_P(IwaCacheVersionManagementTest, IntallNewestVersion) {
  // Installation should use the newest version when the version is not pinned.
  CreateBundlePath(kWebBundleId, kBaseVersion, session_type());
  ConfigureSession(IwaPolicyConfig{kWebBundleId});

  LaunchSession(kWebBundleId);

  AssertAppInstalledAtVersion(kWebBundleId, kUpdateVersion);
}

IN_PROC_BROWSER_TEST_P(IwaCacheVersionManagementTest, PRE_InstallBetaChannel) {
  ConfigureSession(IwaPolicyConfig{kWebBundleId, kBetaChannel});
  AddNewIwaToServer(IwaServerConfig{kWebBundleId, kBaseVersion, kPublicKeyPair},
                    std::vector{kBetaChannel});

  LaunchSession(kWebBundleId);
  AssertAppInstalledAtVersion(kWebBundleId, kBaseVersion);
  WaitUntilPathExists(GetCachedBundlePath(kWebBundleId, kBaseVersion));
}

IN_PROC_BROWSER_TEST_P(IwaCacheVersionManagementTest, InstallBetaChannel) {
  ConfigureSession(IwaPolicyConfig{kWebBundleId, kBetaChannel});
  // The updated version should not be used, since it is not from the beta
  // channel.
  AddNewIwaToServer(
      IwaServerConfig{kWebBundleId, kUpdateVersion, kPublicKeyPair});

  LaunchSession(kWebBundleId);

  AssertAppInstalledAtVersion(kWebBundleId, kBaseVersion);
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    IwaCacheVersionManagementTest,
    testing::Values(SessionType::kManagedGuestSession, SessionType::kKiosk));

}  // namespace web_app
