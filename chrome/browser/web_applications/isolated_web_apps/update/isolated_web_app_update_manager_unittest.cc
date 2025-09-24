// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/update/isolated_web_app_update_manager.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/to_value_list.h"
#include "base/containers/to_vector.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_writer.h"
#include "base/json/values_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/test/test_timeouts.h"
#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/version.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate_factory.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/commands/isolated_web_app_apply_update_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_trust_checker.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolation_data.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_installer.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_policy_constants.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_policy_manager.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/integrity_block_data_matcher.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_test.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/iwa_test_server_configurator.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/key_distribution/test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/policy_generator.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/policy_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/update/isolated_web_app_update_discovery_task.h"
#include "chrome/browser/web_applications/test/fake_web_app_database_factory.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/fake_web_app_ui_manager.h"
#include "chrome/browser/web_applications/test/fake_web_contents_manager.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_management_type.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_contents/web_contents_manager.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/web_package/signed_web_bundles/ed25519_public_key.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/web_package/test_support/signed_web_bundles/ed25519_key_pair.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/uninstall_result_code.h"
#include "components/webapps/common/web_app_id.h"
#include "components/webapps/isolated_web_apps/iwa_key_distribution_info_provider.h"
#include "components/webapps/isolated_web_apps/test_support/signing_keys.h"
#include "components/webapps/isolated_web_apps/types/source.h"
#include "components/webapps/isolated_web_apps/types/storage_location.h"
#include "components/webapps/isolated_web_apps/types/update_channel.h"
#include "content/public/common/content_features.h"
#include "net/http/http_status_code.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace web_app {
namespace {

using base::ToVector;
using base::test::DictionaryHasValue;
using base::test::ErrorIs;
using base::test::ValueIs;
using ::testing::_;
using ::testing::AllOf;
using ::testing::Each;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::ExplainMatchResult;
using ::testing::Field;
using ::testing::Ge;
using ::testing::Invoke;
using ::testing::IsEmpty;
using ::testing::IsFalse;
using ::testing::IsTrue;
using ::testing::Le;
using ::testing::Ne;
using ::testing::NiceMock;
using ::testing::NotNull;
using ::testing::Optional;
using ::testing::Property;
using ::testing::SizeIs;
using ::testing::VariantWith;
using ::testing::WithArg;

const UpdateChannel kBetaChannel = UpdateChannel::Create("beta").value();
constexpr char kInitialIwaVersion[] = "1.0.0";
constexpr char kUpdateIwaVersion[] = "2.0.0";

using UpdateDiscoveryTaskFuture =
    base::test::TestFuture<IsolatedWebAppUpdateDiscoveryTask::CompletionStatus>;

class MockCommandScheduler : public WebAppCommandScheduler {
 public:
  using WebAppCommandScheduler::WebAppCommandScheduler;

  MOCK_METHOD(
      void,
      ApplyPendingIsolatedWebAppUpdate,
      (const IsolatedWebAppUrlInfo& url_info,
       std::unique_ptr<ScopedKeepAlive> optional_keep_alive,
       std::unique_ptr<ScopedProfileKeepAlive> optional_profile_keep_alive,
       base::OnceCallback<void(IsolatedWebAppApplyUpdateCommandResult)>
           callback,
       const base::Location& call_location),
      (override));

  MOCK_METHOD(
      void,
      PrepareAndStoreIsolatedWebAppUpdate,
      (const IsolatedWebAppUpdatePrepareAndStoreCommandUpdateInfo& update_info,
       const IsolatedWebAppUrlInfo& url_info,
       std::unique_ptr<ScopedKeepAlive> optional_keep_alive,
       std::unique_ptr<ScopedProfileKeepAlive> optional_profile_keep_alive,
       PrepareAndStoreIsolatedWebAppUpdateCallback callback,
       const base::Location& call_location),
      (override));

  void DelegateToRealImpl() {
    ON_CALL(*this, ApplyPendingIsolatedWebAppUpdate)
        .WillByDefault(
            [this](
                const IsolatedWebAppUrlInfo& url_info,
                std::unique_ptr<ScopedKeepAlive> optional_keep_alive,
                std::unique_ptr<ScopedProfileKeepAlive>
                    optional_profile_keep_alive,
                base::OnceCallback<void(
                    base::expected<
                        void, IsolatedWebAppApplyUpdateCommandError>)> callback,
                const base::Location& call_location) {
              return this
                  ->WebAppCommandScheduler::ApplyPendingIsolatedWebAppUpdate(
                      url_info, std::move(optional_keep_alive),
                      std::move(optional_profile_keep_alive),
                      std::move(callback), call_location);
            });
    ON_CALL(*this, PrepareAndStoreIsolatedWebAppUpdate)
        .WillByDefault(
            [this](const IsolatedWebAppUpdatePrepareAndStoreCommandUpdateInfo&
                       update_info,
                   const IsolatedWebAppUrlInfo& url_info,
                   std::unique_ptr<ScopedKeepAlive> optional_keep_alive,
                   std::unique_ptr<ScopedProfileKeepAlive>
                       optional_profile_keep_alive,
                   PrepareAndStoreIsolatedWebAppUpdateCallback callback,
                   const base::Location& call_location) {
              return this
                  ->WebAppCommandScheduler::PrepareAndStoreIsolatedWebAppUpdate(
                      update_info, url_info, std::move(optional_keep_alive),
                      std::move(optional_profile_keep_alive),
                      std::move(callback), call_location);
            });
  }
};

class IsolatedWebAppUpdateManagerTest : public IsolatedWebAppTest {
 public:
  explicit IsolatedWebAppUpdateManagerTest(
      base::test::TaskEnvironment::TimeSource time_source =
          base::test::TaskEnvironment::TimeSource::DEFAULT)
      : IsolatedWebAppTest(time_source) {}

 protected:
  IsolatedWebAppUpdateManager& update_manager() {
    return provider().iwa_update_manager();
  }

  FakeWebAppUiManager& fake_ui_manager() {
    return static_cast<FakeWebAppUiManager&>(provider().ui_manager());
  }

  void InitialIwaBundleForceInstall(
      std::unique_ptr<ScopedBundledIsolatedWebApp> app) {
    auto bundle_id = app->web_bundle_id();
    auto version = app->version();

    test_update_server().AddBundle(std::move(app));

    WebAppTestInstallObserver install_observer(profile());
    install_observer.BeginListening({GetAppId(bundle_id)});

    base::test::TestFuture<web_package::SignedWebBundleId, IwaInstallerResult>
        future;
    IsolatedWebAppPolicyManager::SetOnInstallTaskCompletedCallbackForTesting(
        future.GetRepeatingCallback());

    test::AddForceInstalledIwaToPolicy(
        profile()->GetPrefs(),
        test_update_server().CreateForceInstallPolicyEntry(bundle_id));

    auto [web_bundle_id, result] = future.Take();
    ASSERT_EQ(web_bundle_id, bundle_id);
    ASSERT_EQ(result.type(), IwaInstallerResultType::kSuccess);

    ASSERT_EQ(install_observer.Wait(), GetAppId(bundle_id));

    AssertAppInstalledAtVersion(bundle_id, version);
  }

  std::unique_ptr<ScopedBundledIsolatedWebApp> CreateIwa1Bundle(
      std::string_view version) {
    std::unique_ptr<ScopedBundledIsolatedWebApp> app =
        IsolatedWebAppBuilder(ManifestBuilder().SetVersion(version))
            .BuildBundle(GetIwa1WebBundleId(),
                         {test::GetDefaultEd25519KeyPair()});
    app->TrustSigningKey();
    app->FakeInstallPageState(profile());
    return app;
  }

  std::unique_ptr<ScopedBundledIsolatedWebApp> CreateIwa2Bundle(
      std::string_view version) {
    std::unique_ptr<ScopedBundledIsolatedWebApp> app =
        IsolatedWebAppBuilder(ManifestBuilder().SetVersion(version))
            .BuildBundle(GetIwa2WebBundleId(),
                         {test::GetDefaultEcdsaP256KeyPair()});
    app->TrustSigningKey();
    app->FakeInstallPageState(profile());
    return app;
  }

  webapps::AppId GetAppId(web_package::SignedWebBundleId web_bundle_id) const {
    return IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(web_bundle_id)
        .app_id();
  }

  web_package::SignedWebBundleId GetIwa1WebBundleId() const {
    return test::GetDefaultEd25519WebBundleId();
  }

  web_package::SignedWebBundleId GetIwa2WebBundleId() const {
    return test::GetDefaultEcdsaP256WebBundleId();
  }

  void AssertAppDiscoveryTaskSuccessful(
      const web_package::SignedWebBundleId& web_bundle_id) {
    UpdateDiscoveryTaskFuture future;
    UpdateDiscoveryTaskResultWaiter waiter(provider(), GetAppId(web_bundle_id),
                                           future.GetCallback());
    EXPECT_THAT(future.Take(),
                ValueIs(IsolatedWebAppUpdateDiscoveryTask::Success::
                            kUpdateFoundAndSavedInDatabase));
  }

  void AssertInstallationFinish(
      const web_package::SignedWebBundleId& web_bundle_id) {
    WebAppTestManifestUpdatedObserver manifest_updated_observer(
        &provider().install_manager());
    manifest_updated_observer.BeginListeningAndWait({GetAppId(web_bundle_id)});
  }

  void AssertAppInstalledAtVersion(
      const web_package::SignedWebBundleId& web_bundle_id,
      IwaVersion version) {
    ASSERT_EQ(provider()
                  .registrar_unsafe()
                  .GetAppById(GetAppId(web_bundle_id))
                  ->isolation_data()
                  ->version(),
              version);
  }

  void AssertAppNotInstalled(
      const web_package::SignedWebBundleId& web_bundle_id) {
    ASSERT_FALSE(
        provider().registrar_unsafe().GetAppById(GetAppId(web_bundle_id)));
  }

  data_decoder::test::InProcessDataDecoder data_decoder_;
};

class IsolatedWebAppUpdateManagerDevModeUpdateTest : public IsolatedWebAppTest {
 public:
  IsolatedWebAppUpdateManagerDevModeUpdateTest()
      : IsolatedWebAppTest(WithDevMode()) {}

  void SetUp() override {
    IsolatedWebAppTest::SetUp();

    provider().SetEnableAutomaticIwaUpdates(
        FakeWebAppProvider::AutomaticIwaUpdateStrategy::kForceEnabled);
    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

 protected:
  base::ScopedTempDir temp_dir_;
};

TEST_F(IsolatedWebAppUpdateManagerDevModeUpdateTest,
       DiscoversLocalDevModeUpdate) {
  web_package::SignedWebBundleId bundle_id =
      test::GetDefaultEd25519WebBundleId();

  auto initial_bundle =
      IsolatedWebAppBuilder(ManifestBuilder()
                                .SetVersion(kInitialIwaVersion)
                                .SetName("initial iwa"))
          .BuildBundle(bundle_id, {test::GetDefaultEd25519KeyPair()});

  auto update_bundle =
      IsolatedWebAppBuilder(ManifestBuilder()
                                .SetVersion(kUpdateIwaVersion)
                                .SetName("updated iwa"))
          .BuildBundle(bundle_id, {test::GetDefaultEd25519KeyPair()});

  ASSERT_OK_AND_ASSIGN(IsolatedWebAppUrlInfo url_info,
                       initial_bundle->InstallWithSource(
                           profile(), &IsolatedWebAppInstallSource::FromDevUi,
                           web_app::IwaSourceBundleDevFileOp::kCopy));

  update_bundle->FakeInstallPageState(profile());

  base::test::TestFuture<base::expected<IwaVersion, std::string>> future;
  provider().iwa_update_manager().DiscoverApplyAndPrioritizeLocalDevModeUpdate(
      IwaSourceBundleDevModeWithFileOp(update_bundle->path(),
                                       IwaSourceBundleDevFileOp::kCopy),
      url_info, future.GetCallback());

  EXPECT_THAT(future.Get(),
              ValueIs(Eq(*IwaVersion::Create(kUpdateIwaVersion))));
  EXPECT_THAT(provider().registrar_unsafe().GetAppById(url_info.app_id()),
              test::IwaIs(Eq("updated iwa"),
                          test::IsolationDataIs(
                              Property("variant",
                                       &IsolatedWebAppStorageLocation::variant,
                                       VariantWith<IwaStorageOwnedBundle>(_)),
                              Eq(*IwaVersion::Create(kUpdateIwaVersion)),
                              /*controlled_frame_partitions=*/_,
                              /*pending_update_info=*/Eq(std::nullopt),
                              /*integrity_block_data=*/_)));
}

class IsolatedWebAppUpdateManagerUpdateTest
    : public IsolatedWebAppUpdateManagerTest {
 public:
  explicit IsolatedWebAppUpdateManagerUpdateTest(
      base::test::TaskEnvironment::TimeSource time_source =
          base::test::TaskEnvironment::TimeSource::DEFAULT)
      : IsolatedWebAppUpdateManagerTest(time_source) {}

 protected:
  void SetUp() override {
    IsolatedWebAppUpdateManagerTest::SetUp();
    provider().SetEnableAutomaticIwaUpdates(
        FakeWebAppProvider::AutomaticIwaUpdateStrategy::kForceEnabled);

    auto command_scheduler =
        std::make_unique<NiceMock<MockCommandScheduler>>(*profile());
    command_scheduler->DelegateToRealImpl();
    provider().SetScheduler(std::move(command_scheduler));

    SeedWebAppDatabase();
    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  virtual void SeedWebAppDatabase() {}

  MockCommandScheduler& mock_command_scheduler() {
    return static_cast<MockCommandScheduler&>(provider().scheduler());
  }

  base::Value debug_log() {
    return provider().iwa_update_manager().AsDebugValue();
  }

  base::Value::List UpdateDiscoveryLog() {
    return debug_log()
        .GetDict()
        .FindDict("task_queue")
        ->FindList("update_discovery_log")
        ->Clone();
  }

  base::Value::List UpdateDiscoveryTasks() {
    return debug_log()
        .GetDict()
        .FindDict("task_queue")
        ->FindList("update_discovery_tasks")
        ->Clone();
  }

  base::Value::List UpdateApplyLog() {
    return debug_log()
        .GetDict()
        .FindDict("task_queue")
        ->FindList("update_apply_log")
        ->Clone();
  }

  base::Value::List UpdateApplyTasks() {
    return debug_log()
        .GetDict()
        .FindDict("task_queue")
        ->FindList("update_apply_tasks")
        ->Clone();
  }

  base::Value::List UpdateApplyWaiters() {
    return debug_log().GetDict().FindList("update_apply_waiters")->Clone();
  }
};

class IsolatedWebAppUpdateManagerUpdateMockTimeTest
    : public IsolatedWebAppUpdateManagerUpdateTest {
 public:
  IsolatedWebAppUpdateManagerUpdateMockTimeTest()
      : IsolatedWebAppUpdateManagerUpdateTest(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
};

TEST_F(IsolatedWebAppUpdateManagerUpdateMockTimeTest,
       DiscoversAndPreparesUpdateOfPolicyInstalledAppsOnBetaChannel) {
  // Initial Beta channel bundle installation.
  {
    test_update_server().AddBundle(CreateIwa1Bundle(kInitialIwaVersion),
                                   std::vector<UpdateChannel>{kBetaChannel});

    test::AddForceInstalledIwaToPolicy(
        profile()->GetPrefs(),
        test_update_server().CreateForceInstallPolicyEntry(
            GetIwa1WebBundleId(), /*update_channel=*/kBetaChannel));

    web_app::WebAppTestInstallObserver(profile()).BeginListeningAndWait(
        {GetAppId(GetIwa1WebBundleId())});

    AssertAppInstalledAtVersion(GetIwa1WebBundleId(),
                                *IwaVersion::Create(kInitialIwaVersion));
  }

  test_update_server().AddBundle(CreateIwa1Bundle(kUpdateIwaVersion),
                                 std::vector<UpdateChannel>{kBetaChannel});

  task_environment().FastForwardBy(
      *update_manager().GetNextUpdateDiscoveryTimeForTesting() -
      base::TimeTicks::Now());

  AssertAppInstalledAtVersion(GetIwa1WebBundleId(),
                              *IwaVersion::Create(kUpdateIwaVersion));

  EXPECT_THAT(
      UpdateDiscoveryLog(),
      UnorderedElementsAre(DictionaryHasValue(
          "result", base::Value("Success::kUpdateFoundAndDryRunSuccessful"))));
  EXPECT_THAT(UpdateApplyLog(), UnorderedElementsAre(DictionaryHasValue(
                                    "result", base::Value("Success"))));
}

TEST_F(IsolatedWebAppUpdateManagerUpdateMockTimeTest,
       DoesNotDiscoverUpdateOfPolicyInstalledAppsOnDefaultChannel) {
  InitialIwaBundleForceInstall(CreateIwa1Bundle(kInitialIwaVersion));

  test_update_server().AddBundle(CreateIwa1Bundle("4.0.0"),
                                 std::vector<UpdateChannel>{kBetaChannel});

  UpdateDiscoveryTaskFuture update_future;
  UpdateDiscoveryTaskResultWaiter update_waiter(
      provider(), GetAppId(GetIwa1WebBundleId()), update_future.GetCallback());

  task_environment().FastForwardBy(
      *update_manager().GetNextUpdateDiscoveryTimeForTesting() -
      base::TimeTicks::Now());

  EXPECT_THAT(
      update_future.Take(),
      ValueIs(IsolatedWebAppUpdateDiscoveryTask::Success::kNoUpdateFound));

  AssertAppInstalledAtVersion(GetIwa1WebBundleId(),
                              *IwaVersion::Create(kInitialIwaVersion));

  EXPECT_THAT(UpdateDiscoveryLog(),
              UnorderedElementsAre(DictionaryHasValue(
                  "result", base::Value("Success::kNoUpdateFound"))));
  EXPECT_THAT(UpdateApplyLog(), IsEmpty());
}

TEST_F(IsolatedWebAppUpdateManagerUpdateMockTimeTest,
       DiscoversAndPreparesUpdateOfPolicyInstalledAppsToPinnedVersion) {
  InitialIwaBundleForceInstall(CreateIwa1Bundle(kInitialIwaVersion));

  // Pins the IWA to v2.0.0.
  test::EditForceInstalledIwaPolicy(
      profile()->GetPrefs(), GetIwa1WebBundleId(),
      test_update_server().CreateForceInstallPolicyEntry(
          GetIwa1WebBundleId(),
          /*update_channel=*/std::nullopt,
          /*pinned_version=*/*IwaVersion::Create(kUpdateIwaVersion)));

  test_update_server().AddBundle(CreateIwa1Bundle(kUpdateIwaVersion));

  task_environment().FastForwardBy(
      *update_manager().GetNextUpdateDiscoveryTimeForTesting() -
      base::TimeTicks::Now());

  AssertAppInstalledAtVersion(GetIwa1WebBundleId(),
                              *IwaVersion::Create(kUpdateIwaVersion));

  EXPECT_THAT(
      UpdateDiscoveryLog(),
      UnorderedElementsAre(DictionaryHasValue(
          "result",
          base::Value(
              "Success::kPinnedVersionUpdateFoundAndSavedInDatabase"))));
  EXPECT_THAT(UpdateApplyLog(), UnorderedElementsAre(DictionaryHasValue(
                                    "result", base::Value("Success"))));

  test_update_server().AddBundle(CreateIwa1Bundle("3.0.0"));

  task_environment().FastForwardBy(
      *update_manager().GetNextUpdateDiscoveryTimeForTesting() -
      base::TimeTicks::Now());

  AssertAppInstalledAtVersion(GetIwa1WebBundleId(),
                              *IwaVersion::Create(kUpdateIwaVersion));

  EXPECT_THAT(UpdateDiscoveryLog(), IsEmpty());
}

TEST_F(IsolatedWebAppUpdateManagerUpdateMockTimeTest,
       DoesNotDiscoverUpdateOfPolicyInstalledAppsWithPinnedCurrentVersion) {
  // Initial pinned IWA force installation.
  {
    test_update_server().AddBundle(CreateIwa1Bundle(kUpdateIwaVersion));

    test::AddForceInstalledIwaToPolicy(
        profile()->GetPrefs(),
        test_update_server().CreateForceInstallPolicyEntry(
            GetIwa1WebBundleId(),
            /*update_channel=*/std::nullopt,
            /*pinned_version=*/*IwaVersion::Create(kUpdateIwaVersion)));

    web_app::WebAppTestInstallObserver(profile()).BeginListeningAndWait(
        {GetAppId(GetIwa1WebBundleId())});

    AssertAppInstalledAtVersion(GetIwa1WebBundleId(),
                                *IwaVersion::Create(kUpdateIwaVersion));
  }
  test_update_server().AddBundle(CreateIwa1Bundle("3.0.0"));

  task_environment().FastForwardBy(
      *update_manager().GetNextUpdateDiscoveryTimeForTesting() -
      base::TimeTicks::Now());

  // As log as the IWA is pinned to v2.0.0 no updates should be discovered.
  EXPECT_THAT(UpdateDiscoveryLog(), IsEmpty());
}

TEST_F(IsolatedWebAppUpdateManagerUpdateMockTimeTest,
       DoesNotDiscoverUpdateOfPolicyInstalledAppsWhenPinnedToIncorrectVersion) {
  InitialIwaBundleForceInstall(CreateIwa1Bundle(kInitialIwaVersion));

  // Pin IWA to a version that is higher than the latest version.
  test::EditForceInstalledIwaPolicy(
      profile()->GetPrefs(), GetIwa1WebBundleId(),
      test_update_server().CreateForceInstallPolicyEntry(
          GetIwa1WebBundleId(),
          /*update_channel=*/std::nullopt,
          /*pinned_version=*/*IwaVersion::Create("5.0.0")));

  // New version appears, it is still lower than the `pinned_version`.
  test_update_server().AddBundle(CreateIwa1Bundle("3.0.0"));

  task_environment().FastForwardBy(
      *update_manager().GetNextUpdateDiscoveryTimeForTesting() -
      base::TimeTicks::Now());

  AssertAppInstalledAtVersion(GetIwa1WebBundleId(),
                              *IwaVersion::Create(kInitialIwaVersion));

  // Pin IWA to lower version than the current one (downgrading by setting
  // `pinned_version` without setting `allow_downgrades` to true is impossible.
  test::AddForceInstalledIwaToPolicy(
      profile()->GetPrefs(),
      test_update_server().CreateForceInstallPolicyEntry(
          GetIwa1WebBundleId(),
          /*update_channel=*/std::nullopt,
          /*pinned_version=*/*IwaVersion::Create("0.5.0")));

  task_environment().FastForwardBy(
      *update_manager().GetNextUpdateDiscoveryTimeForTesting() -
      base::TimeTicks::Now());

  AssertAppInstalledAtVersion(GetIwa1WebBundleId(),
                              *IwaVersion::Create(kInitialIwaVersion));

  EXPECT_THAT(
      UpdateDiscoveryLog(),
      UnorderedElementsAre(DictionaryHasValue(
          "result",
          base::Value("Error::kPinnedVersionNotFoundInUpdateManifest"))));
  EXPECT_THAT(UpdateDiscoveryLog(), SizeIs(1));
  EXPECT_THAT(UpdateApplyLog(), IsEmpty());
}

TEST_F(IsolatedWebAppUpdateManagerUpdateMockTimeTest,
       DiscoverDowngradeOfPolicyInstalledApps) {
  InitialIwaBundleForceInstall(CreateIwa1Bundle("5.0.0"));

  // Pin IWA to a lower version than the current one.
  test::EditForceInstalledIwaPolicy(
      profile()->GetPrefs(), GetIwa1WebBundleId(),
      test_update_server().CreateForceInstallPolicyEntry(
          GetIwa1WebBundleId(),
          /*update_channel=*/std::nullopt,
          /*pinned_version=*/*IwaVersion::Create(kInitialIwaVersion),
          /*allow_downgrades=*/true));

  test_update_server().AddBundle(CreateIwa1Bundle(kInitialIwaVersion));

  task_environment().FastForwardBy(
      *update_manager().GetNextUpdateDiscoveryTimeForTesting() -
      base::TimeTicks::Now());

  AssertAppInstalledAtVersion(GetIwa1WebBundleId(),
                              *IwaVersion::Create(kInitialIwaVersion));

  EXPECT_THAT(
      UpdateDiscoveryLog(),
      UnorderedElementsAre(DictionaryHasValue(
          "result",
          base::Value("Success::kDowngradeVersionFoundAndSavedInDatabase"))));
  EXPECT_THAT(UpdateDiscoveryLog(), SizeIs(1));
  EXPECT_THAT(UpdateApplyLog(), SizeIs(1));
}

TEST_F(IsolatedWebAppUpdateManagerUpdateMockTimeTest,
       MaybeDiscoverUpdatesForApp) {
  // Trigger updates for an app that is not installed. This should fail.
  EXPECT_THAT(update_manager().MaybeDiscoverUpdatesForApp(
                  GetAppId(GetIwa1WebBundleId())),
              IsFalse());

  // Trigger updates for an app that is not an IWA. This should fail.
  webapps::AppId non_iwa_app_id = test::InstallDummyWebApp(
      profile(), "non-iwa", GURL("https://example.com"));
  EXPECT_THAT(update_manager().MaybeDiscoverUpdatesForApp(non_iwa_app_id),
              IsFalse());

  auto url_info =
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(GetIwa1WebBundleId());
  InitialIwaBundleForceInstall(CreateIwa1Bundle(kInitialIwaVersion));
  fake_ui_manager().SetNumWindowsForApp(GetAppId(GetIwa1WebBundleId()), 1);

  test::RemoveForceInstalledIwaFromPolicy(profile()->GetPrefs(),
                                          GetIwa1WebBundleId());

  AssertAppInstalledAtVersion(GetIwa1WebBundleId(),
                              *IwaVersion::Create(kInitialIwaVersion));

  // Trigger updates for an app that is not installed via policy. This should
  // fail.
  EXPECT_THAT(update_manager().MaybeDiscoverUpdatesForApp(
                  GetAppId(GetIwa1WebBundleId())),
              IsFalse());

  test_update_server().AddBundle(CreateIwa1Bundle(kUpdateIwaVersion));

  test::AddForceInstalledIwaToPolicy(
      profile()->GetPrefs(),
      test_update_server().CreateForceInstallPolicyEntry(GetIwa1WebBundleId()));

  EXPECT_THAT(update_manager().MaybeDiscoverUpdatesForApp(
                  GetAppId(GetIwa1WebBundleId())),
              IsTrue());

  fake_ui_manager().SetNumWindowsForApp(GetAppId(GetIwa1WebBundleId()), 0);

  AssertAppDiscoveryTaskSuccessful(GetIwa1WebBundleId());
}

TEST_F(IsolatedWebAppUpdateManagerUpdateMockTimeTest, DiscoverUpdatesNow) {
  InitialIwaBundleForceInstall(CreateIwa1Bundle(kInitialIwaVersion));

  fake_ui_manager().SetNumWindowsForApp(GetAppId(GetIwa1WebBundleId()), 1);
  test_update_server().AddBundle(CreateIwa1Bundle(kUpdateIwaVersion));

  // After one hour, the update should not yet have run, but still be scheduled
  // (i.e. containing a value in the `std::optional`).
  task_environment().FastForwardBy(base::Hours(1));

  auto old_update_discovery_time =
      update_manager().GetNextUpdateDiscoveryTimeForTesting();
  EXPECT_THAT(old_update_discovery_time.has_value(), IsTrue());

  EXPECT_THAT(update_manager().DiscoverUpdatesNow(), Eq(1ul));
  EXPECT_THAT(update_manager().GetNextUpdateDiscoveryTimeForTesting(),
              AllOf(Ne(old_update_discovery_time), Ge(base::TimeTicks::Now())));

  AssertAppDiscoveryTaskSuccessful(GetIwa1WebBundleId());

  EXPECT_THAT(UpdateApplyLog(), IsEmpty());
}

TEST_F(IsolatedWebAppUpdateManagerUpdateMockTimeTest, KeyRotationUpdateRetry) {
  // Install bundle with ed25519 id.
  InitialIwaBundleForceInstall(CreateIwa1Bundle(kInitialIwaVersion));

  auto capture_discovery_task_result = [&](base::FunctionRef<void()> trigger) {
    UpdateDiscoveryTaskFuture future;
    UpdateDiscoveryTaskResultWaiter waiter(
        provider(), GetAppId(GetIwa1WebBundleId()), future.GetCallback());
    trigger();
    return future.Take();
  };

  ASSERT_THAT(
      capture_discovery_task_result([&] {
        // Rotate the signing key from ed25519 to ecdsaP256. This will
        // trigger an unsuccessful update.
        ASSERT_THAT(
            test::UpdateKeyDistributionInfo(
                base::Version(kInitialIwaVersion), GetIwa1WebBundleId().id(),
                test::GetDefaultEcdsaP256KeyPair().public_key.bytes()),
            base::test::HasValue());
      }),
      ErrorIs(_));

  ASSERT_THAT(capture_discovery_task_result([&] {
                // Fast forward by a minute -- this will trigger the first
                // exponential update attempt.
                task_environment().FastForwardBy(base::Minutes(1));
              }),
              ErrorIs(_));

  ASSERT_THAT(capture_discovery_task_result([&] {
                // Fast forward by two minutes -- this will trigger the second
                // exponential update attempt.
                task_environment().FastForwardBy(base::Minutes(2));
              }),
              ErrorIs(_));

  // Now substitute the bundle served by the manifest.
  test_update_server().AddBundle(
      IsolatedWebAppBuilder(ManifestBuilder().SetVersion(kInitialIwaVersion))
          .BuildBundle(GetIwa1WebBundleId(),
                       {test::GetDefaultEcdsaP256KeyPair()}));

  ASSERT_THAT(capture_discovery_task_result([&] {
                // Fast forward by another four minutes -- this will trigger the
                // third & successful exponential update attempt.
                task_environment().FastForwardBy(base::Minutes(4));
              }),
              ValueIs(_));
}

TEST_F(IsolatedWebAppUpdateManagerUpdateMockTimeTest, SubsequentKeyRotations) {
  // Install bundle with ed25519 id.
  InitialIwaBundleForceInstall(CreateIwa1Bundle(kInitialIwaVersion));

  auto web_bundle_id = GetIwa1WebBundleId();
  auto app_id = GetAppId(web_bundle_id);

  for (uint32_t comp_v = 1; comp_v <= 10; comp_v++) {
    auto key_pair = web_package::test::Ed25519KeyPair::CreateRandom();
    test_update_server().AddBundle(
        IsolatedWebAppBuilder(ManifestBuilder().SetVersion(kInitialIwaVersion))
            .BuildBundle(web_bundle_id, {key_pair}));

    ASSERT_THAT(
        provider().registrar_unsafe().GetAppById(app_id),
        test::IwaIs(_, test::IsolationDataIs(
                           _, _, _, _, /*integrity_block_data=*/
                           testing::Not(test::IntegrityBlockDataPublicKeysAre(
                               key_pair.public_key)))));

    WebAppTestManifestUpdatedObserver manifest_updated_observer(
        &provider().install_manager());
    manifest_updated_observer.BeginListening({app_id});
    ASSERT_THAT(test::UpdateKeyDistributionInfo(
                    base::Version(base::StringPrintf("%d.0.0", comp_v)),
                    web_bundle_id.id(), key_pair.public_key.bytes()),
                base::test::HasValue());
    manifest_updated_observer.Wait();

    ASSERT_THAT(provider().registrar_unsafe().GetAppById(app_id),
                test::IwaIs(_, test::IsolationDataIs(
                                   _, _, _, _, /*integrity_block_data=*/
                                   test::IntegrityBlockDataPublicKeysAre(
                                       key_pair.public_key))));
  }
}

TEST_F(IsolatedWebAppUpdateManagerUpdateTest,
       AppliesUpdatesAfterWindowIsClosed) {
  InitialIwaBundleForceInstall(CreateIwa1Bundle(kInitialIwaVersion));

  fake_ui_manager().SetNumWindowsForApp(GetAppId(GetIwa1WebBundleId()), 1);

  test_update_server().AddBundle(CreateIwa1Bundle(kUpdateIwaVersion));
  update_manager().DiscoverUpdatesNow();

  AssertAppDiscoveryTaskSuccessful(GetIwa1WebBundleId());

  // Due to not closed IWA window, update is not applied yet.
  AssertAppInstalledAtVersion(GetIwa1WebBundleId(),
                              *IwaVersion::Create(kInitialIwaVersion));

  EXPECT_THAT(
      UpdateDiscoveryLog(),
      UnorderedElementsAre(DictionaryHasValue(
          "result", base::Value("Success::kUpdateFoundAndDryRunSuccessful"))));
  EXPECT_THAT(UpdateApplyLog(), IsEmpty());

  fake_ui_manager().SetNumWindowsForApp(GetAppId(GetIwa1WebBundleId()), 0);

  AssertInstallationFinish(GetIwa1WebBundleId());
  AssertAppInstalledAtVersion(GetIwa1WebBundleId(),
                              *IwaVersion::Create(kUpdateIwaVersion));
}

TEST_F(IsolatedWebAppUpdateManagerUpdateTest,
       AppliesUpdatesWithHigherPriorityThanUpdateDiscovery) {
  InitialIwaBundleForceInstall(CreateIwa1Bundle(kInitialIwaVersion));
  InitialIwaBundleForceInstall(CreateIwa2Bundle(kUpdateIwaVersion));

  // Add new bundles for both apps.
  test_update_server().AddBundle(CreateIwa1Bundle("1.1.0"));
  test_update_server().AddBundle(CreateIwa2Bundle("2.2.0"));

  update_manager().DiscoverUpdatesNow();
  WebAppTestManifestUpdatedObserver manifest_updated_observer(
      &provider().install_manager());
  manifest_updated_observer.BeginListeningAndWait(
      {GetAppId(GetIwa1WebBundleId()), GetAppId(GetIwa2WebBundleId())});

  {
    auto update_discovery_log = UpdateDiscoveryLog();
    auto update_apply_log = UpdateApplyLog();

    EXPECT_THAT(
        update_discovery_log,
        UnorderedElementsAre(
            DictionaryHasValue(
                "result",
                base::Value("Success::kUpdateFoundAndDryRunSuccessful")),
            DictionaryHasValue(
                "result",
                base::Value("Success::kUpdateFoundAndDryRunSuccessful"))));

    EXPECT_THAT(update_apply_log,
                UnorderedElementsAre(
                    DictionaryHasValue("result", base::Value("Success")),
                    DictionaryHasValue("result", base::Value("Success"))));

    std::vector<base::Value*> times(
        {update_discovery_log[0].GetDict().Find("start_time"),
         update_discovery_log[0].GetDict().Find("end_time"),
         update_apply_log[0].GetDict().Find("start_time"),
         update_apply_log[0].GetDict().Find("end_time"),

         update_discovery_log[1].GetDict().Find("start_time"),
         update_discovery_log[1].GetDict().Find("end_time"),
         update_apply_log[1].GetDict().Find("start_time"),
         update_apply_log[1].GetDict().Find("end_time")});
    EXPECT_THAT(std::ranges::is_sorted(times, {},
                                       [](base::Value* value) {
                                         return *base::ValueToTime(value);
                                       }),
                IsTrue())
        << base::JoinString(ToVector(times, &base::Value::DebugString), "");
  }

  AssertAppInstalledAtVersion(GetIwa1WebBundleId(),
                              *IwaVersion::Create("1.1.0"));

  AssertAppInstalledAtVersion(GetIwa2WebBundleId(),
                              *IwaVersion::Create("2.2.0"));
}

TEST_F(IsolatedWebAppUpdateManagerUpdateTest,
       SkipsUpdateDiscoveryTaskForNotAllowlistedIwa) {
  base::HistogramTester ht;
  // Turn off default skipping of allowlist for IWA tests
  IwaKeyDistributionInfoProvider::GetInstance()
      .SkipManagedAllowlistChecksForTesting(false);

  // Add both app to allowlist for installing them
  EXPECT_THAT(
      test::UpdateKeyDistributionInfoWithAllowlist(
          base::Version("1.0.1"),
          /*managed_allowlist=*/{GetIwa1WebBundleId(), GetIwa2WebBundleId()}),
      base::test::HasValue());

  InitialIwaBundleForceInstall(CreateIwa1Bundle(kUpdateIwaVersion));
  InitialIwaBundleForceInstall(CreateIwa2Bundle("3.0.0"));

  // Remove the first app from the allowlist
  EXPECT_THAT(test::UpdateKeyDistributionInfoWithAllowlist(
                  base::Version("1.0.2"),
                  /*managed_allowlist=*/{GetIwa2WebBundleId()}),
              base::test::HasValue());

  EXPECT_FALSE(
      IwaKeyDistributionInfoProvider::GetInstance().IsManagedUpdatePermitted(
          GetIwa1WebBundleId().id()));
  EXPECT_TRUE(
      IwaKeyDistributionInfoProvider::GetInstance().IsManagedUpdatePermitted(
          GetIwa2WebBundleId().id()));

  EXPECT_THAT(
      ht.GetAllSamples(kIwaKeyDistributionManagedUpdateAllowedHistogramName),
      base::BucketsAre(base::Bucket(false, 1), base::Bucket(true, 1)));

  test_update_server().AddBundle(CreateIwa1Bundle("2.1.0"));
  test_update_server().AddBundle(CreateIwa2Bundle("3.1.0"));

  EXPECT_THAT(update_manager().DiscoverUpdatesNow(), Eq(1ul));

  WebAppTestManifestUpdatedObserver manifest_updated_observer(
      &provider().install_manager());
  manifest_updated_observer.BeginListeningAndWait(
      {GetAppId(GetIwa2WebBundleId())});

  EXPECT_THAT(
      ht.GetAllSamples(kIwaKeyDistributionManagedUpdateAllowedHistogramName),
      base::BucketsAre(base::Bucket(false, 2), base::Bucket(true, 2)));

  AssertAppInstalledAtVersion(GetIwa1WebBundleId(),
                              *IwaVersion::Create(kUpdateIwaVersion));
  AssertAppInstalledAtVersion(GetIwa2WebBundleId(),
                              *IwaVersion::Create("3.1.0"));
}

TEST_F(IsolatedWebAppUpdateManagerUpdateTest,
       StopsNonStartedUpdateDiscoveryTasksIfIwaIsUninstalled) {
  InitialIwaBundleForceInstall(CreateIwa1Bundle(kInitialIwaVersion));
  InitialIwaBundleForceInstall(CreateIwa2Bundle(kUpdateIwaVersion));

  test_update_server().AddBundle(CreateIwa1Bundle("1.1.0"));
  test_update_server().AddBundle(CreateIwa2Bundle("2.2.0"));

  // Wait for the first discovery task to be in progress.
  base::test::TestFuture<web_package::SignedWebBundleId> future;
  EXPECT_CALL(mock_command_scheduler(), PrepareAndStoreIsolatedWebAppUpdate)
      .WillOnce(
          WithArg<1>([&future](const IsolatedWebAppUrlInfo& url_info) -> void {
            future.SetValue(url_info.web_bundle_id());
          }))
      .RetiresOnSaturation();

  EXPECT_THAT(update_manager().DiscoverUpdatesNow(), Eq(2ul));

  web_package::SignedWebBundleId iwa_to_keep = future.Take();

  // Uninstall the other IWA whose discovery task has not yet started.
  web_package::SignedWebBundleId iwa_to_uninstall =
      (iwa_to_keep == GetIwa1WebBundleId()) ? GetIwa2WebBundleId()
                                            : GetIwa1WebBundleId();
  test::RemoveForceInstalledIwaFromPolicy(profile()->GetPrefs(),
                                          iwa_to_uninstall);

  WebAppTestUninstallObserver uninstall_observer(profile());
  uninstall_observer.BeginListeningAndWait({GetAppId(iwa_to_uninstall)});

  AssertAppNotInstalled(iwa_to_uninstall);
}

TEST_F(IsolatedWebAppUpdateManagerUpdateTest, StopsWaitingIfIwaIsUninstalled) {
  InitialIwaBundleForceInstall(CreateIwa1Bundle(kInitialIwaVersion));

  fake_ui_manager().SetNumWindowsForApp(GetAppId(GetIwa1WebBundleId()), 1);

  test_update_server().AddBundle(CreateIwa1Bundle(kUpdateIwaVersion));
  update_manager().DiscoverUpdatesNow();
  AssertAppDiscoveryTaskSuccessful(GetIwa1WebBundleId());

  EXPECT_THAT(UpdateApplyWaiters(),
              UnorderedElementsAre(DictionaryHasValue(
                  "app_id", base::Value(GetAppId(GetIwa1WebBundleId())))));

  AssertAppInstalledAtVersion(GetIwa1WebBundleId(),
                              *IwaVersion::Create(kInitialIwaVersion));

  // IWA uninstallation.
  {
    test::RemoveForceInstalledIwaFromPolicy(profile()->GetPrefs(),
                                            GetIwa1WebBundleId());
    WebAppTestUninstallObserver uninstall_observer(profile());
    uninstall_observer.BeginListeningAndWait({GetAppId(GetIwa1WebBundleId())});
  }
  EXPECT_THAT(UpdateApplyWaiters(), IsEmpty());
  EXPECT_THAT(UpdateApplyTasks(), IsEmpty());
  EXPECT_THAT(UpdateApplyLog(), IsEmpty());
}

TEST_F(IsolatedWebAppUpdateManagerUpdateTest,
       StopsNonStartedUpdateApplyTasksIfIwaIsUninstalled) {
  InitialIwaBundleForceInstall(CreateIwa1Bundle(kInitialIwaVersion));
  InitialIwaBundleForceInstall(CreateIwa2Bundle(kUpdateIwaVersion));

  fake_ui_manager().SetNumWindowsForApp(GetAppId(GetIwa1WebBundleId()), 1);
  fake_ui_manager().SetNumWindowsForApp(GetAppId(GetIwa2WebBundleId()), 1);

  test_update_server().AddBundle(CreateIwa1Bundle("1.1.0"));
  test_update_server().AddBundle(CreateIwa2Bundle("2.2.0"));

  update_manager().DiscoverUpdatesNow();
  AssertAppDiscoveryTaskSuccessful(GetIwa1WebBundleId());
  AssertAppDiscoveryTaskSuccessful(GetIwa2WebBundleId());

  EXPECT_THAT(
      UpdateDiscoveryLog(),
      UnorderedElementsAre(
          DictionaryHasValue(
              "result",
              base::Value("Success::kUpdateFoundAndDryRunSuccessful")),
          DictionaryHasValue(
              "result",
              base::Value("Success::kUpdateFoundAndDryRunSuccessful"))));
  EXPECT_THAT(UpdateApplyWaiters(),
              UnorderedElementsAre(
                  DictionaryHasValue(
                      "app_id", base::Value(GetAppId(GetIwa1WebBundleId()))),
                  DictionaryHasValue(
                      "app_id", base::Value(GetAppId(GetIwa2WebBundleId())))));

  // Wait for the update apply task of either app 1 or app 2 to start.
  base::test::TestFuture<IsolatedWebAppUrlInfo> future;
  EXPECT_CALL(mock_command_scheduler(),
              ApplyPendingIsolatedWebAppUpdate(_, _, _, _, _))
      .WillOnce(WithArg<0>(Invoke(
          &future, &base::test::TestFuture<IsolatedWebAppUrlInfo>::SetValue)));

  fake_ui_manager().SetNumWindowsForApp(GetAppId(GetIwa1WebBundleId()), 0);
  fake_ui_manager().SetNumWindowsForApp(GetAppId(GetIwa2WebBundleId()), 0);
  webapps::AppId iwa_to_keep = future.Take().app_id();

  EXPECT_THAT(UpdateApplyTasks(), SizeIs(2));  // two tasks should be queued
  EXPECT_THAT(UpdateApplyLog(), IsEmpty());    // no task should have finished

  // Uninstall the other IWA whose update apply task has not yet started.
  web_package::SignedWebBundleId iwa_to_uninstall =
      (iwa_to_keep == GetAppId(GetIwa1WebBundleId())) ? GetIwa2WebBundleId()
                                                      : GetIwa1WebBundleId();

  test::RemoveForceInstalledIwaFromPolicy(profile()->GetPrefs(),
                                          iwa_to_uninstall);
  WebAppTestUninstallObserver uninstall_observer(profile());
  uninstall_observer.BeginListeningAndWait({GetAppId(iwa_to_uninstall)});

  EXPECT_THAT(UpdateApplyTasks(), UnorderedElementsAre(DictionaryHasValue(
                                      "app_id", base::Value(iwa_to_keep))));
  EXPECT_THAT(UpdateApplyLog(), IsEmpty());
}

class IsolatedWebAppUpdateManagerUpdateApplyOnStartupTest
    : public IsolatedWebAppUpdateManagerUpdateTest {
 protected:
  void SeedWebAppDatabase() override {
    // Seed the `WebAppProvider` with an IWA before it is started.
    EXPECT_THAT(provider().is_registry_ready(), IsFalse());

    base::FilePath path = update_location_.GetPath(profile()->GetPath());
    EXPECT_THAT(base::CreateDirectory(path.DirName()), IsTrue());

    auto update_bundle =
        IsolatedWebAppBuilder(ManifestBuilder()
                                  .SetVersion(kUpdateIwaVersion)
                                  .SetName("updated iwa"))
            .BuildBundle(path, test::GetDefaultEd25519KeyPair());
    update_bundle->TrustSigningKey();
    update_bundle->FakeInstallPageState(profile());

    auto url_info = IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
        GetIwa1WebBundleId());

    std::unique_ptr<WebApp> iwa = CreateIsolatedWebApp(
        url_info.origin().GetURL(),
        IsolationData::Builder(
            IwaStorageOwnedBundle{"iwa1", /*dev_mode=*/false},
            *IwaVersion::Create(kInitialIwaVersion))
            .SetPendingUpdateInfo(IsolationData::PendingUpdateInfo(
                update_location_, *IwaVersion::Create(kUpdateIwaVersion)))
            .Build());

    CreateStoragePartition(url_info);

    Registry registry;
    registry.emplace(iwa->app_id(), std::move(iwa));
    auto& database_factory =
        static_cast<FakeWebAppDatabaseFactory&>(provider().database_factory());
    database_factory.WriteRegistry(registry);
  }

  IwaStorageOwnedBundle update_location_{"update_folder", /*dev_mode=*/false};

 private:
  void CreateStoragePartition(IsolatedWebAppUrlInfo& url_info) {
    content::StoragePartition* new_storage_partition =
        profile()->GetStoragePartition(
            url_info.storage_partition_config(profile()),
            /*can_create=*/true);
    EXPECT_THAT(new_storage_partition, NotNull());
  }

  std::unique_ptr<WebApp> CreateIsolatedWebApp(const GURL& start_url,
                                               IsolationData isolation_data) {
    webapps::AppId app_id = GenerateAppId(/*manifest_id=*/"", start_url);
    auto web_app = std::make_unique<WebApp>(app_id);
    web_app->SetName("iwa name");
    web_app->SetStartUrl(start_url);
    web_app->SetScope(start_url.DeprecatedGetOriginAsURL());
    web_app->SetManifestId(start_url.DeprecatedGetOriginAsURL());
    web_app->AddSource(WebAppManagement::Type::kIwaUserInstalled);
    web_app->SetIsolationData(isolation_data);
    web_app->SetUserDisplayMode(mojom::UserDisplayMode::kStandalone);
    return web_app;
  }

  base::ScopedTempDir temp_dir_;
};

TEST_F(IsolatedWebAppUpdateManagerUpdateApplyOnStartupTest,
       SchedulesPendingUpdateApplyTasks) {
  AssertInstallationFinish(GetIwa1WebBundleId());

  EXPECT_THAT(
      provider().registrar_unsafe().GetAppById(GetAppId(GetIwa1WebBundleId())),
      test::IwaIs("updated iwa", test::IsolationDataIs(
                                     update_location_,
                                     Eq(*IwaVersion::Create(kUpdateIwaVersion)),
                                     /*controlled_frame_partitions=*/_,
                                     /*pending_update_info=*/Eq(std::nullopt),
                                     /*integrity_block_data=*/_)));
}

class IsolatedWebAppUpdateManagerDiscoveryTimerTest
    : public IsolatedWebAppUpdateManagerTest {
 protected:
  void SetUp() override {
    IsolatedWebAppUpdateManagerTest::SetUp();
    provider().SetEnableAutomaticIwaUpdates(
        FakeWebAppProvider::AutomaticIwaUpdateStrategy::kForceEnabled);
    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }
};

TEST_F(IsolatedWebAppUpdateManagerDiscoveryTimerTest,
       DoesNotStartUpdateDiscoveryIfNoIwaIsInstalled) {
  EXPECT_THAT(
      update_manager().GetNextUpdateDiscoveryTimeForTesting().has_value(),
      IsFalse());
}

TEST_F(IsolatedWebAppUpdateManagerDiscoveryTimerTest,
       StartsUpdateDiscoveryTimerWithJitter) {
  std::vector<base::TimeTicks> times;
  for (int i = 0; i < 10; ++i) {
    std::unique_ptr<ScopedBundledIsolatedWebApp> app =
        IsolatedWebAppBuilder(ManifestBuilder().SetVersion(kInitialIwaVersion))
            .BuildBundle();
    app->FakeInstallPageState(profile());
    app->TrustSigningKey();
    app->InstallChecked(profile());

    auto time = update_manager().GetNextUpdateDiscoveryTimeForTesting();
    EXPECT_THAT(time,
                Optional(AllOf(Ge(base::TimeTicks::Now() + base::Hours(4)),
                               Le(base::TimeTicks::Now() + base::Hours(6)))));
    if (time.has_value()) {
      // Check that the time is truly random (and thus different) from all
      // previously generated times.
      EXPECT_THAT(times, Each(Ne(*time)));
      times.push_back(*time);
    }

    test::UninstallWebApp(profile(), GetAppId(app->web_bundle_id()));
    EXPECT_THAT(
        update_manager().GetNextUpdateDiscoveryTimeForTesting().has_value(),
        IsFalse());
  }
}

TEST_F(IsolatedWebAppUpdateManagerDiscoveryTimerTest,
       RunsUpdateDiscoveryWhileIwaIsInstalled) {
  EXPECT_THAT(
      update_manager().GetNextUpdateDiscoveryTimeForTesting().has_value(),
      IsFalse());

  webapps::AppId non_iwa_id =
      test::InstallDummyWebApp(profile(), "non-iwa", GURL("https://a"));
  EXPECT_THAT(
      update_manager().GetNextUpdateDiscoveryTimeForTesting().has_value(),
      IsFalse());

  CreateIwa1Bundle(kInitialIwaVersion)->InstallChecked(profile());
  EXPECT_THAT(
      update_manager().GetNextUpdateDiscoveryTimeForTesting().has_value(),
      IsTrue());

  CreateIwa2Bundle(kUpdateIwaVersion)->InstallChecked(profile());
  EXPECT_THAT(
      update_manager().GetNextUpdateDiscoveryTimeForTesting().has_value(),
      IsTrue());

  test::UninstallWebApp(profile(), GetAppId(GetIwa1WebBundleId()));
  EXPECT_THAT(
      update_manager().GetNextUpdateDiscoveryTimeForTesting().has_value(),
      IsTrue());

  test::UninstallWebApp(profile(), non_iwa_id);
  EXPECT_THAT(
      update_manager().GetNextUpdateDiscoveryTimeForTesting().has_value(),
      IsTrue());

  test::UninstallWebApp(profile(), GetAppId(GetIwa2WebBundleId()));
  EXPECT_THAT(
      update_manager().GetNextUpdateDiscoveryTimeForTesting().has_value(),
      IsFalse());
}

TEST_F(IsolatedWebAppUpdateManagerUpdateTest, UpdateDiscoveryTaskSuccess) {
  base::HistogramTester histogram_tester;
  IsolatedWebAppUpdateDiscoveryTask::CompletionStatus status =
      IsolatedWebAppUpdateDiscoveryTask::Success::kNoUpdateFound;

  update_manager().TrackResultOfUpdateDiscoveryTaskForTesting(status);

  // When the discovery task is successful, no UMA metric should be recorded.
  histogram_tester.ExpectTotalCount("WebApp.Isolated.UpdateSuccess", 0);
  histogram_tester.ExpectTotalCount("WebApp.Isolated.UpdateError", 0);
}

TEST_F(IsolatedWebAppUpdateManagerUpdateTest, UpdateDiscoveryTaskFails) {
  base::HistogramTester histogram_tester;
  IsolatedWebAppUpdateDiscoveryTask::CompletionStatus status = base::unexpected(
      IsolatedWebAppUpdateDiscoveryTask::Error::kUpdateManifestInvalidJson);

  update_manager().TrackResultOfUpdateDiscoveryTaskForTesting(status);

  EXPECT_THAT(
      histogram_tester.GetAllSamples("WebApp.Isolated.UpdateSuccess"),
      AllOf(SizeIs(1), BucketsAre(base::Bucket(/*update_success=*/false, 1))));

  EXPECT_THAT(
      histogram_tester.GetAllSamples("WebApp.Isolated.UpdateError"),
      AllOf(SizeIs(1),
            BucketsAre(base::Bucket(
                IsolatedWebAppUpdateError::kUpdateManifestInvalidJson, 1))));
}

TEST_F(IsolatedWebAppUpdateManagerUpdateTest, UpdateApplyTaskSuccess) {
  base::HistogramTester histogram_tester;
  update_manager().TrackResultOfUpdateApplyTaskForTesting(base::ok());

  EXPECT_THAT(
      histogram_tester.GetAllSamples("WebApp.Isolated.UpdateSuccess"),
      AllOf(SizeIs(1), BucketsAre(base::Bucket(/*update_success=*/true, 1))));
  histogram_tester.ExpectTotalCount("WebApp.Isolated.UpdateError", 0);
}

TEST_F(IsolatedWebAppUpdateManagerUpdateTest, UpdateApplyTaskFails) {
  base::HistogramTester histogram_tester;
  IsolatedWebAppApplyUpdateCommandResult status =
      base::unexpected<IsolatedWebAppApplyUpdateCommandError>(
          {"error message"});

  update_manager().TrackResultOfUpdateApplyTaskForTesting(status);

  EXPECT_THAT(
      histogram_tester.GetAllSamples("WebApp.Isolated.UpdateSuccess"),
      AllOf(SizeIs(1), BucketsAre(base::Bucket(/*update_success=*/false, 1))));

  EXPECT_THAT(
      histogram_tester.GetAllSamples("WebApp.Isolated.UpdateError"),
      AllOf(SizeIs(1), BucketsAre(base::Bucket(
                           IsolatedWebAppUpdateError::kUpdateApplyFailed, 1))));
}

}  // namespace
}  // namespace web_app
