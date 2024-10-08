// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_update_manager.h"

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
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
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
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_apply_update_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_source.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_storage_location.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_trust_checker.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_update_discovery_task.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolation_data.h"
#include "chrome/browser/web_applications/isolated_web_apps/iwa_identity_validator.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_policy_constants.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/test_signed_web_bundle_builder.h"
#include "chrome/browser/web_applications/test/fake_web_app_database_factory.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/fake_web_app_ui_manager.h"
#include "chrome/browser/web_applications/test/fake_web_contents_manager.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_contents/web_contents_manager.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/nacl/common/buildflags.h"
#include "components/web_package/signed_web_bundles/ed25519_public_key.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/uninstall_result_code.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/common/content_features.h"
#include "net/http/http_status_code.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

#if BUILDFLAG(ENABLE_NACL)
#include "chrome/browser/nacl_host/nacl_browser_delegate_impl.h"
#include "components/nacl/browser/nacl_browser.h"
#endif  // BUILDFLAG(ENABLE_NACL)

namespace web_app {
namespace {

using base::ToVector;
using base::test::DictionaryHasValue;
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

struct IwaForceInstallPolicyEntry {
  IsolatedWebAppUrlInfo url_info;
  std::string_view update_manifest_url;
  std::optional<std::string_view> update_channel;
};

blink::mojom::ManifestPtr CreateDefaultManifest(const GURL& application_url,
                                                std::u16string_view short_name,
                                                const base::Version& version) {
  auto manifest = blink::mojom::Manifest::New();
  manifest->id = application_url.DeprecatedGetOriginAsURL();
  manifest->scope = application_url.Resolve("/");
  manifest->start_url = application_url.Resolve("/testing-start-url.html");
  manifest->display = DisplayMode::kStandalone;
  manifest->short_name = short_name;
  manifest->version = base::UTF8ToUTF16(version.GetString());

  return manifest;
}

MATCHER_P(IsDict, dict_matcher, "") {
  return ExplainMatchResult(
      Property("GetDict", &base::Value::GetDict, dict_matcher), arg,
      result_listener);
}

class MockCommandScheduler : public WebAppCommandScheduler {
 public:
  using WebAppCommandScheduler::WebAppCommandScheduler;

  MOCK_METHOD(
      void,
      ApplyPendingIsolatedWebAppUpdate,
      (const IsolatedWebAppUrlInfo& url_info,
       std::unique_ptr<ScopedKeepAlive> optional_keep_alive,
       std::unique_ptr<ScopedProfileKeepAlive> optional_profile_keep_alive,
       base::OnceCallback<
           void(base::expected<void, IsolatedWebAppApplyUpdateCommandError>)>
           callback,
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
  }
};

#if BUILDFLAG(ENABLE_NACL)
class ScopedNaClBrowserDelegate {
 public:
  ~ScopedNaClBrowserDelegate() { nacl::NaClBrowser::ClearAndDeleteDelegate(); }

  void Init(ProfileManager* profile_manager) {
    nacl::NaClBrowser::SetDelegate(
        std::make_unique<NaClBrowserDelegateImpl>(profile_manager));
  }
};
#endif  // BUILDFLAG(ENABLE_NACL)

// TODO(b/304691179): Rely less on `RunUntilIdle` and more on concrete events in
// all of these tests.
class IsolatedWebAppUpdateManagerTest : public WebAppTest {
 public:
  explicit IsolatedWebAppUpdateManagerTest(
      const base::flat_map<base::test::FeatureRef, bool>& feature_states =
          {{features::kIsolatedWebApps, true}},
      base::test::TaskEnvironment::TimeSource time_source =
          base::test::TaskEnvironment::TimeSource::DEFAULT)
      : WebAppTest(WebAppTest::WithTestUrlLoaderFactory(), time_source) {
    scoped_feature_list_.InitWithFeatureStates(feature_states);
  }

  void SetUp() override {
    WebAppTest::SetUp();
#if BUILDFLAG(ENABLE_NACL)
    // Clearing Cache will clear PNACL cache, which needs this delegate set.
    nacl_browser_delegate_.Init(profile_manager().profile_manager());
#endif  // BUILDFLAG(ENABLE_NACL)
    IwaIdentityValidator::CreateSingleton();
  }

  void TearDown() override {
    if (task_environment()->UsesMockTime()) {
      // TODO(b/299074540): Without this line, subsequent tests are unable to
      // use `test::UninstallWebApp`, which will hang forever. This has
      // something to do with the combination of `MOCK_TIME` and NaCl, because
      // the code ends up hanging forever in
      // `PnaclTranslationCache::DoomEntriesBetween`. A simple `FastForwardBy`
      // here seems to alleviate this issue.
      task_environment()->FastForwardBy(TestTimeouts::tiny_timeout());
    }

    WebAppTest::TearDown();
  }

 protected:
  IsolatedWebAppUpdateManager& update_manager() {
    return fake_provider().iwa_update_manager();
  }

  FakeWebContentsManager& fake_web_contents_manager() {
    return static_cast<FakeWebContentsManager&>(
        fake_provider().web_contents_manager());
  }

  FakeWebAppUiManager& fake_ui_manager() {
    return static_cast<FakeWebAppUiManager&>(fake_provider().ui_manager());
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  data_decoder::test::InProcessDataDecoder data_decoder_;
#if BUILDFLAG(ENABLE_NACL)
  ScopedNaClBrowserDelegate nacl_browser_delegate_;
#endif  // BUILDFLAG(ENABLE_NACL)
};

class IsolatedWebAppUpdateManagerDevModeUpdateTest
    : public IsolatedWebAppUpdateManagerTest {
 public:
  IsolatedWebAppUpdateManagerDevModeUpdateTest()
      : IsolatedWebAppUpdateManagerTest(
            {{features::kIsolatedWebApps, true},
             {features::kIsolatedWebAppDevMode, true}}) {}

  void SetUp() override {
    IsolatedWebAppUpdateManagerTest::SetUp();

    fake_provider().SetEnableAutomaticIwaUpdates(
        FakeWebAppProvider::AutomaticIwaUpdateStrategy::kForceEnabled);
    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  void CreateInstallPageState(const IsolatedWebAppUrlInfo& url_info) {
    GURL install_url = url_info.origin().GetURL().Resolve(
        "/.well-known/_generated_install_page.html");
    auto& page_state =
        fake_web_contents_manager().GetOrCreatePageState(install_url);
    page_state.url_load_result = webapps::WebAppUrlLoaderResult::kUrlLoaded;
    page_state.error_code = webapps::InstallableStatusCode::NO_ERROR_DETECTED;
    page_state.manifest_url =
        url_info.origin().GetURL().Resolve("manifest.webmanifest");
    page_state.valid_manifest_for_web_app = true;
    page_state.manifest_before_default_processing = CreateDefaultManifest(
        url_info.origin().GetURL(), u"updated iwa", base::Version("2.0.0"));
  }

 protected:
  base::ScopedTempDir temp_dir_;
};

TEST_F(IsolatedWebAppUpdateManagerDevModeUpdateTest,
       DiscoversLocalDevModeUpdate) {
  auto key_pair = web_package::test::Ed25519KeyPair::CreateRandom();
  TestSignedWebBundle update_bundle = TestSignedWebBundleBuilder::BuildDefault(
      TestSignedWebBundleBuilder::BuildOptions()
          .SetVersion(base::Version("2.0.0"))
          .SetAppName("updated iwa")
          .AddKeyPair(key_pair));

  ASSERT_THAT(temp_dir_.CreateUniqueTempDir(), IsTrue());
  base::FilePath path = temp_dir_.GetPath().AppendASCII("bundle.swbn");
  base::WriteFile(path, update_bundle.data);

  IsolatedWebAppUrlInfo url_info =
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
          web_package::SignedWebBundleId::CreateForPublicKey(
              key_pair.public_key));

  AddDummyIsolatedAppToRegistry(
      profile(), url_info.origin().GetURL(), "installed iwa 1 (unowned bundle)",
      IsolationData::Builder(IwaStorageUnownedBundle{base::FilePath()},
                             base::Version("1.0.0"))
          .Build());

  CreateInstallPageState(url_info);

  base::test::TestFuture<base::expected<base::Version, std::string>> future;
  fake_provider()
      .iwa_update_manager()
      .DiscoverApplyAndPrioritizeLocalDevModeUpdate(
          IwaSourceBundleDevModeWithFileOp(path, kDefaultBundleDevFileOp),
          url_info, future.GetCallback());

  EXPECT_THAT(future.Get(), ValueIs(Eq(base::Version("2.0.0"))));
  EXPECT_THAT(fake_provider().registrar_unsafe().GetAppById(url_info.app_id()),
              test::IwaIs(Eq("updated iwa"),
                          test::IsolationDataIs(
                              Property("variant",
                                       &IsolatedWebAppStorageLocation::variant,
                                       VariantWith<IwaStorageOwnedBundle>(_)),
                              Eq(base::Version("2.0.0")),
                              /*controlled_frame_partitions=*/_,
                              /*pending_update_info=*/Eq(std::nullopt),
                              /*integrity_block_data=*/_)));

  // TODO(crbug.com/40277668): As a temporary fix to avoid race conditions with
  // `ScopedProfileKeepAlive`s, manually shutdown `KeyedService`s holding them.
  fake_provider().Shutdown();
  ChromeBrowsingDataRemoverDelegateFactory::GetForProfile(profile())
      ->Shutdown();
}

class IsolatedWebAppUpdateManagerUpdateTest
    : public IsolatedWebAppUpdateManagerTest {
 public:
  explicit IsolatedWebAppUpdateManagerUpdateTest(
      base::test::TaskEnvironment::TimeSource time_source =
          base::test::TaskEnvironment::TimeSource::DEFAULT)
      : IsolatedWebAppUpdateManagerTest({{features::kIsolatedWebApps, true}},
                                        time_source) {}

 protected:
  struct IwaInfo {
    IwaInfo(web_package::test::Ed25519KeyPair key_pair,
            IsolatedWebAppStorageLocation installed_location,
            base::Version installed_version,
            GURL update_manifest_url,
            GURL update_bundle_url,
            base::Version update_version,
            std::string update_app_name,
            std::optional<std::vector<UpdateChannel>> update_channels =
                std::nullopt)
        : url_info(IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
              web_package::SignedWebBundleId::CreateForPublicKey(
                  key_pair.public_key))),
          key_pair(std::move(key_pair)),
          installed_location(std::move(installed_location)),
          installed_version(std::move(installed_version)),
          update_manifest_url(std::move(update_manifest_url)),
          update_bundle_url(std::move(update_bundle_url)),
          update_version(std::move(update_version)),
          update_app_name(std::move(update_app_name)),
          update_channels(std::move(update_channels)) {}

    IsolatedWebAppUrlInfo url_info;
    web_package::test::Ed25519KeyPair key_pair;
    IsolatedWebAppStorageLocation installed_location;
    base::Version installed_version;

    GURL update_manifest_url;
    GURL update_bundle_url;
    base::Version update_version;
    std::string update_app_name;
    std::optional<std::vector<UpdateChannel>> update_channels;
  };

  void SetUp() override {
    IsolatedWebAppUpdateManagerTest::SetUp();
    fake_provider().SetEnableAutomaticIwaUpdates(
        FakeWebAppProvider::AutomaticIwaUpdateStrategy::kForceEnabled);

    auto command_scheduler =
        std::make_unique<NiceMock<MockCommandScheduler>>(*profile());
    command_scheduler->DelegateToRealImpl();
    fake_provider().SetScheduler(std::move(command_scheduler));

    iwa_info1_ = IwaInfo(web_package::test::Ed25519KeyPair::CreateRandom(),
                         IwaStorageOwnedBundle{"iwa1", /*dev_mode=*/false},
                         base::Version("1.0.0"),
                         GURL("https://example.com/update_manifest1.json"),
                         GURL("https://example.com/bundle1.swbn"),
                         base::Version("2.0.0"), "updated app 1");
    SetUpIwaInfo(*iwa_info1_);

    iwa_info2_ = IwaInfo(web_package::test::Ed25519KeyPair::CreateRandom(),
                         IwaStorageOwnedBundle{"iwa2", /*dev_mode=*/false},
                         base::Version("4.0.0"),
                         GURL("https://example.com/update_manifest2.json"),
                         GURL("https://example.com/bundle2.swbn"),
                         base::Version("7.0.0"), "updated app 2");
    SetUpIwaInfo(*iwa_info2_);

    auto iwa3_update_channels = {UpdateChannel::Create("beta").value(),
                                 UpdateChannel::Create("🍪").value()};
    iwa_info3_ =
        IwaInfo(web_package::test::Ed25519KeyPair::CreateRandom(),
                IwaStorageOwnedBundle{"iwa3", /*dev_mode=*/false},
                base::Version("4.0.0"),
                GURL("https://example.com/update_manifest3.json"),
                GURL("https://example.com/bundle3.swbn"),
                base::Version("8.0.0"), "updated app 3", iwa3_update_channels);
    SetUpIwaInfo(*iwa_info3_);

    SetTrustedWebBundleIdsForTesting({iwa_info1_->url_info.web_bundle_id(),
                                      iwa_info2_->url_info.web_bundle_id(),
                                      iwa_info3_->url_info.web_bundle_id()});

    SeedWebAppDatabase();
    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  void SetUpIwaInfo(const IwaInfo& iwa_info) {
    TestSignedWebBundle update_bundle =
        CreateBundle(iwa_info.update_version, iwa_info.key_pair);

    auto version_entry =
        base::Value::Dict()
            .Set("src", iwa_info.update_bundle_url.spec())
            .Set("version", iwa_info.update_version.GetString());

    if (iwa_info.update_channels.has_value()) {
      version_entry.Set("channels",
                        base::ToValueList(iwa_info.update_channels.value(),
                                          &UpdateChannel::ToString));
    }

    profile_url_loader_factory().AddResponse(
        iwa_info.update_manifest_url.spec(),
        *base::WriteJson(base::Value::Dict().Set(
            "versions", base::Value::List().Append(std::move(version_entry)))));

    profile_url_loader_factory().AddResponse(
        iwa_info.update_bundle_url.spec(),
        std::string(update_bundle.data.begin(), update_bundle.data.end()));

    GURL install_url = iwa_info.url_info.origin().GetURL().Resolve(
        "/.well-known/_generated_install_page.html");

    auto& page_state =
        fake_web_contents_manager().GetOrCreatePageState(install_url);
    page_state.url_load_result = webapps::WebAppUrlLoaderResult::kUrlLoaded;
    page_state.error_code = webapps::InstallableStatusCode::NO_ERROR_DETECTED;
    page_state.manifest_url =
        iwa_info.url_info.origin().GetURL().Resolve("manifest.webmanifest");
    page_state.valid_manifest_for_web_app = true;
    page_state.manifest_before_default_processing = CreateDefaultManifest(
        iwa_info.url_info.origin().GetURL(),
        base::UTF8ToUTF16(iwa_info.update_app_name), iwa_info.update_version);
  }

  TestSignedWebBundle CreateBundle(
      const base::Version& version,
      const web_package::test::Ed25519KeyPair& key_pair) const {
    return TestSignedWebBundleBuilder::BuildDefault(
        TestSignedWebBundleBuilder::BuildOptions()
            .SetVersion(version)
            .AddKeyPair(key_pair));
  }

  virtual void SeedWebAppDatabase() {}

#if BUILDFLAG(IS_CHROMEOS)
  void SetIwaForceInstallPolicy(
      std::vector<IwaForceInstallPolicyEntry> entries) {
    profile()->GetPrefs()->SetList(
        prefs::kIsolatedWebAppInstallForceList,
        base::ToValueList(entries, [](const auto& entry) {
          auto dict =
              base::Value::Dict()
                  .Set(kPolicyWebBundleIdKey,
                       entry.url_info.web_bundle_id().id())
                  .Set(kPolicyUpdateManifestUrlKey, entry.update_manifest_url);

          if (entry.update_channel.has_value()) {
            dict.Set(kPolicyUpdateChannelKey, entry.update_channel.value());
          }

          return dict;
        }));
  }

  // TODO(crbug.com/298005569): This should eventually go away and instead rely
  // on the `IsolatedWebAppPolicyManager` to perform the uninstall by setting
  // `SetIwaForceInstallPolicy` appropriately.
  [[nodiscard]] webapps::UninstallResultCode UninstallPolicyInstalledIwa(
      const webapps::AppId& app_id) {
    base::test::TestFuture<webapps::UninstallResultCode> future;
    fake_provider().scheduler().RemoveInstallManagementMaybeUninstall(
        app_id, WebAppManagement::Type::kIwaPolicy,
        webapps::WebappUninstallSource::kIwaEnterprisePolicy,
        future.GetCallback());
    return future.Take();
  }
#endif

  NiceMock<MockCommandScheduler>& mock_command_scheduler() {
    return static_cast<NiceMock<MockCommandScheduler>&>(
        fake_provider().scheduler());
  }

  base::Value debug_log() {
    return fake_provider().iwa_update_manager().AsDebugValue();
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

  auto UpdateLocationMatcher(Profile* profile) {
    return Property("variant", &IsolatedWebAppStorageLocation::variant,
                    VariantWith<IwaStorageOwnedBundle>(
                        test::OwnedIwaBundleExists(profile->GetPath())));
  }

  std::optional<IwaInfo> iwa_info1_;
  std::optional<IwaInfo> iwa_info2_;
  std::optional<IwaInfo> iwa_info3_;
};

class IsolatedWebAppUpdateManagerUpdateMockTimeTest
    : public IsolatedWebAppUpdateManagerUpdateTest {
 public:
  IsolatedWebAppUpdateManagerUpdateMockTimeTest()
      : IsolatedWebAppUpdateManagerUpdateTest(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
};

#if BUILDFLAG(IS_CHROMEOS)
TEST_F(IsolatedWebAppUpdateManagerUpdateMockTimeTest,
       DiscoversAndPreparesUpdateOfPolicyInstalledApps) {
  IsolatedWebAppUrlInfo non_installed_url_info =
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
          *web_package::SignedWebBundleId::Create(
              "5tkrnsmftl4ggvvdkfth3piainqragus2qbhf7rlz2a3wo3rh4wqaaic"));
  IsolatedWebAppUrlInfo dev_bundle_url_info =
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
          *web_package::SignedWebBundleId::Create(
              "aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic"));
  IsolatedWebAppUrlInfo dev_proxy_url_info =
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
          web_package::SignedWebBundleId::CreateRandomForProxyMode());

  test::InstallDummyWebApp(profile(), "non-iwa", GURL("https://a"));
  AddDummyIsolatedAppToRegistry(
      profile(), iwa_info1_->url_info.origin().GetURL(), "installed iwa 1",
      IsolationData::Builder(iwa_info1_->installed_location,
                             iwa_info1_->installed_version)
          .Build(),
      webapps::WebappInstallSource::IWA_EXTERNAL_POLICY);
  AddDummyIsolatedAppToRegistry(
      profile(), dev_proxy_url_info.origin().GetURL(),
      "installed iwa 2 (dev mode proxy)",
      IsolationData::Builder(IwaStorageProxy{dev_proxy_url_info.origin()},
                             base::Version("1.0.0"))
          .Build(),
      webapps::WebappInstallSource::IWA_EXTERNAL_POLICY);
  AddDummyIsolatedAppToRegistry(
      profile(), dev_bundle_url_info.origin().GetURL(),
      "installed iwa 3 (unowned bundle)",
      IsolationData::Builder(IwaStorageUnownedBundle{base::FilePath()},
                             base::Version("1.0.0"))
          .Build(),
      webapps::WebappInstallSource::IWA_EXTERNAL_POLICY);
  AddDummyIsolatedAppToRegistry(
      profile(), GURL("isolated-app://b"), "installed iwa 4",
      IsolationData::Builder(
          IwaStorageOwnedBundle{/*dir_name_ascii=*/"", /*dev_mode=*/false},
          base::Version("1.0.0"))
          .Build(),
      webapps::WebappInstallSource::IWA_EXTERNAL_POLICY);

  fake_ui_manager().SetNumWindowsForApp(iwa_info1_->url_info.app_id(), 1);

  SetIwaForceInstallPolicy(
      {{iwa_info1_->url_info, iwa_info1_->update_manifest_url.spec()},
       {non_installed_url_info, "https://example.com/update_manifest.json"},
       {dev_bundle_url_info, "https://example.com/update_manifest.json"},
       {dev_proxy_url_info, "https://example.com/update_manifest.json"}});

  task_environment()->FastForwardBy(
      *update_manager().GetNextUpdateDiscoveryTimeForTesting() -
      base::TimeTicks::Now());
  task_environment()->RunUntilIdle();

  EXPECT_THAT(
      fake_provider().registrar_unsafe().GetAppById(
          iwa_info1_->url_info.app_id()),
      test::IwaIs(Eq("installed iwa 1"),
                  test::IsolationDataIs(Eq(iwa_info1_->installed_location),
                                        Eq(iwa_info1_->installed_version),
                                        /*controlled_frame_partitions=*/_,
                                        test::PendingUpdateInfoIs(
                                            UpdateLocationMatcher(profile()),
                                            Eq(base::Version("2.0.0")),
                                            /*integrity_block_data=*/_),
                                        /*integrity_block_data=*/_)));

  EXPECT_THAT(
      UpdateDiscoveryLog(),
      UnorderedElementsAre(IsDict(DictionaryHasValue(
          "result", base::Value("Success::kUpdateFoundAndDryRunSuccessful")))));
  EXPECT_THAT(UpdateApplyLog(), IsEmpty());

  // TODO(crbug.com/40277668): As a temporary fix to avoid race conditions with
  // `ScopedProfileKeepAlive`s, manually shutdown `KeyedService`s holding them.
  fake_provider().Shutdown();
  ChromeBrowsingDataRemoverDelegateFactory::GetForProfile(profile())
      ->Shutdown();
}

TEST_F(IsolatedWebAppUpdateManagerUpdateMockTimeTest,
       DiscoversAndPreparesUpdateOfPolicyInstalledAppsOnBetaChannel) {
  AddDummyIsolatedAppToRegistry(
      profile(), iwa_info3_->url_info.origin().GetURL(), "installed iwa 3",
      IsolationData::Builder(iwa_info3_->installed_location,
                             iwa_info3_->installed_version)
          .Build(),
      webapps::WebappInstallSource::IWA_EXTERNAL_POLICY);
  SetIwaForceInstallPolicy(
      {{iwa_info3_->url_info, iwa_info3_->update_manifest_url.spec(), "beta"}});

  task_environment()->FastForwardBy(
      *update_manager().GetNextUpdateDiscoveryTimeForTesting() -
      base::TimeTicks::Now());
  task_environment()->RunUntilIdle();

  EXPECT_THAT(fake_provider().registrar_unsafe().GetAppById(
                  iwa_info3_->url_info.app_id()),
              test::IwaIs(iwa_info3_->update_app_name,
                          test::IsolationDataIs(
                              UpdateLocationMatcher(profile()),
                              Eq(iwa_info3_->update_version),
                              /*controlled_frame_partitions=*/_,
                              /*pending_update_info=*/Eq(std::nullopt),
                              /*integrity_block_data=*/_)));

  EXPECT_THAT(
      UpdateDiscoveryLog(),
      UnorderedElementsAre(IsDict(DictionaryHasValue(
          "result", base::Value("Success::kUpdateFoundAndDryRunSuccessful")))));
  EXPECT_THAT(UpdateApplyLog(), UnorderedElementsAre(IsDict(DictionaryHasValue(
                                    "result", base::Value("Success")))));

  // TODO(crbug.com/40277668): As a temporary fix to avoid race conditions with
  // `ScopedProfileKeepAlive`s, manually shutdown `KeyedService`s holding them.
  fake_provider().Shutdown();
  ChromeBrowsingDataRemoverDelegateFactory::GetForProfile(profile())
      ->Shutdown();
}

TEST_F(IsolatedWebAppUpdateManagerUpdateMockTimeTest,
       DoesNotDiscoverUpdateOfPolicyInstalledAppsOnDefaultChannel) {
  AddDummyIsolatedAppToRegistry(
      profile(), iwa_info3_->url_info.origin().GetURL(), "installed iwa 3",
      IsolationData::Builder(iwa_info3_->installed_location,
                             iwa_info3_->installed_version)
          .Build(),
      webapps::WebappInstallSource::IWA_EXTERNAL_POLICY);
  SetIwaForceInstallPolicy(
      {{iwa_info3_->url_info, iwa_info3_->update_manifest_url.spec()}});

  task_environment()->FastForwardBy(
      *update_manager().GetNextUpdateDiscoveryTimeForTesting() -
      base::TimeTicks::Now());
  task_environment()->RunUntilIdle();

  EXPECT_THAT(fake_provider().registrar_unsafe().GetAppById(
                  iwa_info3_->url_info.app_id()),
              test::IwaIs(Eq("installed iwa 3"),
                          test::IsolationDataIs(
                              Eq(iwa_info3_->installed_location),
                              Eq(iwa_info3_->installed_version),
                              /*controlled_frame_partitions=*/_,
                              /*pending_update_info=*/Eq(std::nullopt),
                              /*integrity_block_data=*/_)));

  EXPECT_THAT(UpdateDiscoveryLog(),
              UnorderedElementsAre(IsDict(DictionaryHasValue(
                  "result",
                  base::Value("Error::kUpdateManifestNoApplicableVersion")))));
  EXPECT_THAT(UpdateApplyLog(), IsEmpty());

  // TODO(crbug.com/40277668): As a temporary fix to avoid race conditions with
  // `ScopedProfileKeepAlive`s, manually shutdown `KeyedService`s holding them.
  fake_provider().Shutdown();
  ChromeBrowsingDataRemoverDelegateFactory::GetForProfile(profile())
      ->Shutdown();
}
#endif  // BUILDFLAG(IS_CHROMEOS)

TEST_F(IsolatedWebAppUpdateManagerUpdateMockTimeTest,
       MaybeDiscoverUpdatesForApp) {
  // Trigger updates for an app that is not installed. This should fail.
  EXPECT_THAT(update_manager().MaybeDiscoverUpdatesForApp(
                  iwa_info1_->url_info.app_id()),
              IsFalse());

  // Trigger updates for an app that is not an IWA. This should fail.
  webapps::AppId non_iwa_app_id = test::InstallDummyWebApp(
      profile(), "non-iwa", GURL("https://example.com"));
  EXPECT_THAT(update_manager().MaybeDiscoverUpdatesForApp(non_iwa_app_id),
              IsFalse());

  AddDummyIsolatedAppToRegistry(
      profile(), iwa_info1_->url_info.origin().GetURL(), "installed iwa 1",
      IsolationData::Builder(iwa_info1_->installed_location,
                             iwa_info1_->installed_version)
          .Build(),
      webapps::WebappInstallSource::IWA_EXTERNAL_POLICY);

  fake_ui_manager().SetNumWindowsForApp(iwa_info1_->url_info.app_id(), 1);

  // Trigger updates for an app that is not installed via policy. This should
  // fail.
  EXPECT_THAT(update_manager().MaybeDiscoverUpdatesForApp(
                  iwa_info1_->url_info.app_id()),
              IsFalse());

#if BUILDFLAG(IS_CHROMEOS)
  SetIwaForceInstallPolicy(
      {{iwa_info1_->url_info, iwa_info1_->update_manifest_url.spec()}});

  EXPECT_THAT(update_manager().MaybeDiscoverUpdatesForApp(
                  iwa_info1_->url_info.app_id()),
              IsTrue());
  task_environment()->RunUntilIdle();

  EXPECT_THAT(
      fake_provider().registrar_unsafe().GetAppById(
          iwa_info1_->url_info.app_id()),
      test::IwaIs(Eq("installed iwa 1"),
                  test::IsolationDataIs(Eq(iwa_info1_->installed_location),
                                        Eq(iwa_info1_->installed_version),
                                        /*controlled_frame_partitions=*/_,
                                        test::PendingUpdateInfoIs(
                                            UpdateLocationMatcher(profile()),
                                            Eq(base::Version("2.0.0")),
                                            /*integrity_block_data=*/_),
                                        /*integrity_block_data=*/_)));

  EXPECT_THAT(
      UpdateDiscoveryLog(),
      UnorderedElementsAre(IsDict(DictionaryHasValue(
          "result", base::Value("Success::kUpdateFoundAndDryRunSuccessful")))));
  EXPECT_THAT(UpdateApplyLog(), IsEmpty());
#endif  // BUILDFLAG(IS_CHROMEOS)

  // TODO(crbug.com/40277668): As a temporary fix to avoid race conditions with
  // `ScopedProfileKeepAlive`s, manually shutdown `KeyedService`s holding them.
  fake_provider().Shutdown();
  ChromeBrowsingDataRemoverDelegateFactory::GetForProfile(profile())
      ->Shutdown();
}

#if BUILDFLAG(IS_CHROMEOS)
TEST_F(IsolatedWebAppUpdateManagerUpdateMockTimeTest, DiscoverUpdatesNow) {
  AddDummyIsolatedAppToRegistry(
      profile(), iwa_info1_->url_info.origin().GetURL(), "installed iwa 1",
      IsolationData::Builder(iwa_info1_->installed_location,
                             iwa_info1_->installed_version)
          .Build(),
      webapps::WebappInstallSource::IWA_EXTERNAL_POLICY);

  fake_ui_manager().SetNumWindowsForApp(iwa_info1_->url_info.app_id(), 1);

  SetIwaForceInstallPolicy(
      {{iwa_info1_->url_info, iwa_info1_->update_manifest_url.spec()}});

  // After one hour, the update should not yet have run, but still be scheduled
  // (i.e. containing a value in the `std::optional`).
  task_environment()->FastForwardBy(base::Hours(1));
  auto old_update_discovery_time =
      update_manager().GetNextUpdateDiscoveryTimeForTesting();
  EXPECT_THAT(old_update_discovery_time.has_value(), IsTrue());

  // Once we manually trigger update discovery, the update discovery timer
  // should reset to a different time in the future.
  EXPECT_THAT(update_manager().DiscoverUpdatesNow(), Eq(1ul));
  EXPECT_THAT(update_manager().GetNextUpdateDiscoveryTimeForTesting(),
              AllOf(Ne(old_update_discovery_time), Ge(base::TimeTicks::Now())));

  task_environment()->RunUntilIdle();

  EXPECT_THAT(
      fake_provider().registrar_unsafe().GetAppById(
          iwa_info1_->url_info.app_id()),
      test::IwaIs(Eq("installed iwa 1"),
                  test::IsolationDataIs(Eq(iwa_info1_->installed_location),
                                        Eq(iwa_info1_->installed_version),
                                        /*controlled_frame_partitions=*/_,
                                        test::PendingUpdateInfoIs(
                                            UpdateLocationMatcher(profile()),
                                            Eq(base::Version("2.0.0")),
                                            /*integrity_block_data=*/_),
                                        /*integrity_block_data=*/_)));

  EXPECT_THAT(
      UpdateDiscoveryLog(),
      UnorderedElementsAre(IsDict(DictionaryHasValue(
          "result", base::Value("Success::kUpdateFoundAndDryRunSuccessful")))));
  EXPECT_THAT(UpdateApplyLog(), IsEmpty());

  // TODO(crbug.com/40277668): As a temporary fix to avoid race conditions with
  // `ScopedProfileKeepAlive`s, manually shutdown `KeyedService`s holding them.
  fake_provider().Shutdown();
  ChromeBrowsingDataRemoverDelegateFactory::GetForProfile(profile())
      ->Shutdown();
}

TEST_F(IsolatedWebAppUpdateManagerUpdateTest,
       ApplysUpdatesAfterWindowIsClosed) {
  AddDummyIsolatedAppToRegistry(
      profile(), iwa_info1_->url_info.origin().GetURL(), "installed app",
      IsolationData::Builder(iwa_info1_->installed_location,
                             iwa_info1_->installed_version)
          .Build(),
      webapps::WebappInstallSource::IWA_EXTERNAL_POLICY);

  fake_ui_manager().SetNumWindowsForApp(iwa_info1_->url_info.app_id(), 1);

  SetIwaForceInstallPolicy(
      {{iwa_info1_->url_info, iwa_info1_->update_manifest_url.spec()}});
  update_manager().DiscoverUpdatesNow();
  task_environment()->RunUntilIdle();

  EXPECT_THAT(
      fake_provider().registrar_unsafe().GetAppById(
          iwa_info1_->url_info.app_id()),
      test::IwaIs(Eq("installed app"),
                  test::IsolationDataIs(Eq(iwa_info1_->installed_location),
                                        Eq(iwa_info1_->installed_version),
                                        /*controlled_frame_partitions=*/_,
                                        test::PendingUpdateInfoIs(
                                            UpdateLocationMatcher(profile()),
                                            Eq(iwa_info1_->update_version),
                                            /*integrity_block_data=*/_),
                                        /*integrity_block_data=*/_)));

  EXPECT_THAT(
      UpdateDiscoveryLog(),
      UnorderedElementsAre(IsDict(DictionaryHasValue(
          "result", base::Value("Success::kUpdateFoundAndDryRunSuccessful")))));
  EXPECT_THAT(UpdateApplyLog(), IsEmpty());

  fake_ui_manager().SetNumWindowsForApp(iwa_info1_->url_info.app_id(), 0);
  task_environment()->RunUntilIdle();

  EXPECT_THAT(UpdateApplyLog(), UnorderedElementsAre(IsDict(DictionaryHasValue(
                                    "result", base::Value("Success")))));

  EXPECT_THAT(fake_provider().registrar_unsafe().GetAppById(
                  iwa_info1_->url_info.app_id()),
              test::IwaIs(iwa_info1_->update_app_name,
                          test::IsolationDataIs(
                              UpdateLocationMatcher(profile()),
                              Eq(iwa_info1_->update_version),
                              /*controlled_frame_partitions=*/_,
                              /*pending_update_info=*/Eq(std::nullopt),
                              /*integrity_block_data=*/_)));
}

TEST_F(IsolatedWebAppUpdateManagerUpdateTest,
       ApplysUpdatesWithHigherPriorityThanUpdateDiscovery) {
  AddDummyIsolatedAppToRegistry(
      profile(), iwa_info1_->url_info.origin().GetURL(), "installed app 1",
      IsolationData::Builder(iwa_info1_->installed_location,
                             iwa_info1_->installed_version)
          .Build(),
      webapps::WebappInstallSource::IWA_EXTERNAL_POLICY);
  AddDummyIsolatedAppToRegistry(
      profile(), iwa_info2_->url_info.origin().GetURL(), "installed app 2",
      IsolationData::Builder(iwa_info2_->installed_location,
                             iwa_info2_->installed_version)
          .Build(),
      webapps::WebappInstallSource::IWA_EXTERNAL_POLICY);

  SetIwaForceInstallPolicy(
      {{iwa_info1_->url_info, iwa_info1_->update_manifest_url.spec()},
       {iwa_info2_->url_info, iwa_info2_->update_manifest_url.spec()}});
  update_manager().DiscoverUpdatesNow();
  task_environment()->RunUntilIdle();

  {
    auto update_discovery_log = UpdateDiscoveryLog();
    auto update_apply_log = UpdateApplyLog();

    EXPECT_THAT(
        update_discovery_log,
        UnorderedElementsAre(
            IsDict(DictionaryHasValue(
                "result",
                base::Value("Success::kUpdateFoundAndDryRunSuccessful"))),
            IsDict(DictionaryHasValue(
                "result",
                base::Value("Success::kUpdateFoundAndDryRunSuccessful")))));

    EXPECT_THAT(
        update_apply_log,
        UnorderedElementsAre(
            IsDict(DictionaryHasValue("result", base::Value("Success"))),
            IsDict(DictionaryHasValue("result", base::Value("Success")))));

    std::vector<base::Value*> times(
        {update_discovery_log[0].GetDict().Find("start_time"),
         update_discovery_log[0].GetDict().Find("end_time"),
         update_apply_log[0].GetDict().Find("start_time"),
         update_apply_log[0].GetDict().Find("end_time"),

         update_discovery_log[1].GetDict().Find("start_time"),
         update_discovery_log[1].GetDict().Find("end_time"),
         update_apply_log[1].GetDict().Find("start_time"),
         update_apply_log[1].GetDict().Find("end_time")});
    EXPECT_THAT(base::ranges::is_sorted(times, {},
                                        [](base::Value* value) {
                                          return *base::ValueToTime(value);
                                        }),
                IsTrue())
        << base::JoinString(ToVector(times, &base::Value::DebugString), "");
  }

  EXPECT_THAT(fake_provider().registrar_unsafe().GetAppById(
                  iwa_info1_->url_info.app_id()),
              test::IwaIs(iwa_info1_->update_app_name,
                          test::IsolationDataIs(
                              UpdateLocationMatcher(profile()),
                              Eq(iwa_info1_->update_version),
                              /*controlled_frame_partitions=*/_,
                              /*pending_update_info=*/Eq(std::nullopt),
                              /*integrity_block_data=*/_)));
  EXPECT_THAT(fake_provider().registrar_unsafe().GetAppById(
                  iwa_info2_->url_info.app_id()),
              test::IwaIs(iwa_info2_->update_app_name,
                          test::IsolationDataIs(
                              UpdateLocationMatcher(profile()),
                              Eq(iwa_info2_->update_version),
                              /*controlled_frame_partitions=*/_,
                              /*pending_update_info=*/Eq(std::nullopt),
                              /*integrity_block_data=*/_)));
}

TEST_F(IsolatedWebAppUpdateManagerUpdateTest,
       StopsNonStartedUpdateDiscoveryTasksIfIwaIsUninstalled) {
  profile_url_loader_factory().ClearResponses();

  AddDummyIsolatedAppToRegistry(
      profile(), iwa_info1_->url_info.origin().GetURL(), "installed app 1",
      IsolationData::Builder(iwa_info1_->installed_location,
                             iwa_info1_->installed_version)
          .Build(),
      webapps::WebappInstallSource::IWA_EXTERNAL_POLICY);
  AddDummyIsolatedAppToRegistry(
      profile(), iwa_info2_->url_info.origin().GetURL(), "installed app 2",
      IsolationData::Builder(iwa_info2_->installed_location,
                             iwa_info2_->installed_version)
          .Build(),
      webapps::WebappInstallSource::IWA_EXTERNAL_POLICY);

  SetIwaForceInstallPolicy(
      {{iwa_info1_->url_info, iwa_info1_->update_manifest_url.spec()},
       {iwa_info2_->url_info, iwa_info2_->update_manifest_url.spec()}});
  update_manager().DiscoverUpdatesNow();
  task_environment()->RunUntilIdle();

  // Wait for the update discovery task of either app 1 or app 2 to request the
  // update manifest (which task starts first is undefined).
  ASSERT_THAT(profile_url_loader_factory().NumPending(), Eq(1));
  EXPECT_THAT(UpdateDiscoveryTasks(), SizeIs(2));  // two tasks should be queued
  EXPECT_THAT(UpdateDiscoveryLog(), IsEmpty());  // no task should have finished

  // Uninstall the other IWA whose update discovery task has not yet started.
  GURL pending_url =
      profile_url_loader_factory().GetPendingRequest(0)->request.url;
  IwaInfo* iwa_to_keep;
  IwaInfo* iwa_to_uninstall;
  if (pending_url == iwa_info1_->update_manifest_url) {
    iwa_to_keep = &*iwa_info1_;
    iwa_to_uninstall = &*iwa_info2_;
  } else if (pending_url == iwa_info2_->update_manifest_url) {
    iwa_to_keep = &*iwa_info2_;
    iwa_to_uninstall = &*iwa_info1_;
  } else {
    FAIL() << "Unexpected pending request for: " << pending_url;
  }

  EXPECT_THAT(UninstallPolicyInstalledIwa(iwa_to_uninstall->url_info.app_id()),
              Eq(webapps::UninstallResultCode::kAppRemoved));

  EXPECT_THAT(UpdateDiscoveryTasks(),
              UnorderedElementsAre(IsDict(DictionaryHasValue(
                  "app_id", base::Value(iwa_to_keep->url_info.app_id())))));
  EXPECT_THAT(UpdateDiscoveryLog(), IsEmpty());

  // TODO(crbug.com/40277668): As a temporary fix to avoid race conditions with
  // `ScopedProfileKeepAlive`s, manually shutdown `KeyedService`s holding them.
  fake_provider().Shutdown();
  ChromeBrowsingDataRemoverDelegateFactory::GetForProfile(profile())
      ->Shutdown();
}

// TODO(b/338380813): The test is flaky on asan ChromeOS builder.
TEST_F(IsolatedWebAppUpdateManagerUpdateTest,
       DISABLED_StopsWaitingIfIwaIsUninstalled) {
  AddDummyIsolatedAppToRegistry(
      profile(), iwa_info1_->url_info.origin().GetURL(), "installed app",
      IsolationData::Builder(iwa_info1_->installed_location,
                             iwa_info1_->installed_version)
          .Build(),
      webapps::WebappInstallSource::IWA_EXTERNAL_POLICY);

  fake_ui_manager().SetNumWindowsForApp(iwa_info1_->url_info.app_id(), 1);

  SetIwaForceInstallPolicy(
      {{iwa_info1_->url_info, iwa_info1_->update_manifest_url.spec()}});
  update_manager().DiscoverUpdatesNow();
  task_environment()->RunUntilIdle();

  EXPECT_THAT(
      UpdateDiscoveryLog(),
      UnorderedElementsAre(IsDict(DictionaryHasValue(
          "result", base::Value("Success::kUpdateFoundAndDryRunSuccessful")))));
  EXPECT_THAT(UpdateApplyWaiters(),
              UnorderedElementsAre(IsDict(DictionaryHasValue(
                  "app_id", base::Value(iwa_info1_->url_info.app_id())))));

  EXPECT_THAT(UninstallPolicyInstalledIwa(iwa_info1_->url_info.app_id()),
              Eq(webapps::UninstallResultCode::kAppRemoved));

  EXPECT_THAT(UpdateApplyWaiters(), IsEmpty());
  EXPECT_THAT(UpdateApplyTasks(), IsEmpty());
  EXPECT_THAT(UpdateApplyLog(), IsEmpty());

  // TODO(crbug.com/40277668): As a temporary fix to avoid race conditions with
  // `ScopedProfileKeepAlive`s, manually shutdown `KeyedService`s holding them.
  fake_provider().Shutdown();
  ChromeBrowsingDataRemoverDelegateFactory::GetForProfile(profile())
      ->Shutdown();
}

// TODO(b/326527744): This test is flaky on asan ChromeOS builder.
TEST_F(IsolatedWebAppUpdateManagerUpdateTest,
       DISABLED_StopsNonStartedUpdateApplyTasksIfIwaIsUninstalled) {
  AddDummyIsolatedAppToRegistry(
      profile(), iwa_info1_->url_info.origin().GetURL(), "installed app 1",
      IsolationData::Builder(iwa_info1_->installed_location,
                             iwa_info1_->installed_version)
          .Build(),
      webapps::WebappInstallSource::IWA_EXTERNAL_POLICY);
  AddDummyIsolatedAppToRegistry(
      profile(), iwa_info2_->url_info.origin().GetURL(), "installed app 2",
      IsolationData::Builder(iwa_info2_->installed_location,
                             iwa_info2_->installed_version)
          .Build(),
      webapps::WebappInstallSource::IWA_EXTERNAL_POLICY);

  fake_ui_manager().SetNumWindowsForApp(iwa_info1_->url_info.app_id(), 1);
  fake_ui_manager().SetNumWindowsForApp(iwa_info2_->url_info.app_id(), 1);

  SetIwaForceInstallPolicy(
      {{iwa_info1_->url_info, iwa_info1_->update_manifest_url.spec()},
       {iwa_info2_->url_info, iwa_info2_->update_manifest_url.spec()}});
  update_manager().DiscoverUpdatesNow();
  task_environment()->RunUntilIdle();

  EXPECT_THAT(
      UpdateDiscoveryLog(),
      UnorderedElementsAre(
          IsDict(DictionaryHasValue(
              "result",
              base::Value("Success::kUpdateFoundAndDryRunSuccessful"))),
          IsDict(DictionaryHasValue(
              "result",
              base::Value("Success::kUpdateFoundAndDryRunSuccessful")))));
  EXPECT_THAT(UpdateApplyWaiters(),
              UnorderedElementsAre(
                  IsDict(DictionaryHasValue(
                      "app_id", base::Value(iwa_info1_->url_info.app_id()))),
                  IsDict(DictionaryHasValue(
                      "app_id", base::Value(iwa_info2_->url_info.app_id())))));

  // Wait for the update apply task of either app 1 or app 2 to start.
  base::test::TestFuture<IsolatedWebAppUrlInfo> future;
  EXPECT_CALL(mock_command_scheduler(),
              ApplyPendingIsolatedWebAppUpdate(_, _, _, _, _))
      .WillOnce(WithArg<0>(Invoke(
          &future, &base::test::TestFuture<IsolatedWebAppUrlInfo>::SetValue)));
  fake_ui_manager().SetNumWindowsForApp(iwa_info1_->url_info.app_id(), 0);
  fake_ui_manager().SetNumWindowsForApp(iwa_info2_->url_info.app_id(), 0);
  webapps::AppId iwa_to_keep = future.Take().app_id();

  EXPECT_THAT(UpdateApplyTasks(), SizeIs(2));  // two tasks should be queued
  EXPECT_THAT(UpdateApplyLog(), IsEmpty());    // no task should have finished

  // Uninstall the other IWA whose update apply task has not yet started.
  IwaInfo* iwa_to_uninstall;
  if (iwa_to_keep == iwa_info1_->url_info.app_id()) {
    iwa_to_uninstall = &*iwa_info2_;
  } else if (iwa_to_keep == iwa_info2_->url_info.app_id()) {
    iwa_to_uninstall = &*iwa_info1_;
  } else {
    FAIL() << "Unexpected IWA app id: " << iwa_to_keep;
  }

  EXPECT_THAT(UninstallPolicyInstalledIwa(iwa_to_uninstall->url_info.app_id()),
              Eq(webapps::UninstallResultCode::kAppRemoved));

  EXPECT_THAT(UpdateApplyTasks(),
              UnorderedElementsAre(IsDict(
                  DictionaryHasValue("app_id", base::Value(iwa_to_keep)))));
  EXPECT_THAT(UpdateApplyLog(), IsEmpty());

  // TODO(crbug.com/40277668): As a temporary fix to avoid race conditions with
  // `ScopedProfileKeepAlive`s, manually shutdown `KeyedService`s holding them.
  fake_provider().Shutdown();
  ChromeBrowsingDataRemoverDelegateFactory::GetForProfile(profile())
      ->Shutdown();
}
#endif  // BUILDFLAG(IS_CHROMEOS)

class IsolatedWebAppUpdateManagerUpdateApplyOnStartupTest
    : public IsolatedWebAppUpdateManagerUpdateTest {
 protected:
  void SeedWebAppDatabase() override {
    // Seed the `WebAppProvider` with an IWA before it is started.
    EXPECT_THAT(fake_provider().is_registry_ready(), IsFalse());

    base::FilePath path = update_location_.GetPath(profile()->GetPath());
    EXPECT_THAT(base::CreateDirectory(path.DirName()), IsTrue());

    auto update_bundle =
        CreateBundle(iwa_info1_->update_version, iwa_info1_->key_pair);
    EXPECT_THAT(base::WriteFile(path, update_bundle.data), IsTrue());

    std::unique_ptr<WebApp> iwa = CreateIsolatedWebApp(
        iwa_info1_->url_info.origin().GetURL(),
        IsolationData::Builder(iwa_info1_->installed_location,
                               iwa_info1_->installed_version)
            .SetPendingUpdateInfo(IsolationData::PendingUpdateInfo(
                update_location_, iwa_info1_->update_version))
            .Build());
    CreateStoragePartition(iwa_info1_->url_info);

    Registry registry;
    registry.emplace(iwa->app_id(), std::move(iwa));
    auto& database_factory = static_cast<FakeWebAppDatabaseFactory&>(
        fake_provider().database_factory());
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
  WebAppTestManifestUpdatedObserver manifest_updated_observer(
      &fake_provider().install_manager());
  manifest_updated_observer.BeginListening({iwa_info1_->url_info.app_id()});
  manifest_updated_observer.Wait();

  EXPECT_THAT(fake_provider().registrar_unsafe().GetAppById(
                  iwa_info1_->url_info.app_id()),
              test::IwaIs(iwa_info1_->update_app_name,
                          test::IsolationDataIs(
                              update_location_, Eq(iwa_info1_->update_version),
                              /*controlled_frame_partitions=*/_,
                              /*pending_update_info=*/Eq(std::nullopt),
                              /*integrity_block_data=*/_)));
}

class IsolatedWebAppUpdateManagerDiscoveryTimerTest
    : public IsolatedWebAppUpdateManagerTest {
 protected:
  void SetUp() override {
    IsolatedWebAppUpdateManagerTest::SetUp();
    fake_provider().SetEnableAutomaticIwaUpdates(
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
    webapps::AppId app_id = AddDummyIsolatedAppToRegistry(
        profile(), GURL("isolated-app://a"), "iwa");
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

    test::UninstallWebApp(profile(), app_id);
    EXPECT_THAT(
        update_manager().GetNextUpdateDiscoveryTimeForTesting().has_value(),
        IsFalse());
  }

  // TODO(crbug.com/40277668): As a temporary fix to avoid race conditions with
  // `ScopedProfileKeepAlive`s, manually shutdown `KeyedService`s holding them.
  fake_provider().Shutdown();
  ChromeBrowsingDataRemoverDelegateFactory::GetForProfile(profile())
      ->Shutdown();
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

  webapps::AppId iwa_app_id1 = AddDummyIsolatedAppToRegistry(
      profile(), GURL("isolated-app://a"), "iwa1");
  EXPECT_THAT(
      update_manager().GetNextUpdateDiscoveryTimeForTesting().has_value(),
      IsTrue());

  webapps::AppId iwa_app_id2 = AddDummyIsolatedAppToRegistry(
      profile(), GURL("isolated-app://b"), "iwa2");
  EXPECT_THAT(
      update_manager().GetNextUpdateDiscoveryTimeForTesting().has_value(),
      IsTrue());

  test::UninstallWebApp(profile(), iwa_app_id1);
  EXPECT_THAT(
      update_manager().GetNextUpdateDiscoveryTimeForTesting().has_value(),
      IsTrue());

  test::UninstallWebApp(profile(), non_iwa_id);
  EXPECT_THAT(
      update_manager().GetNextUpdateDiscoveryTimeForTesting().has_value(),
      IsTrue());

  test::UninstallWebApp(profile(), iwa_app_id2);
  EXPECT_THAT(
      update_manager().GetNextUpdateDiscoveryTimeForTesting().has_value(),
      IsFalse());
}

struct FeatureFlagParam {
  base::flat_map<base::test::FeatureRef, bool> feature_states;
  bool expected_result;
};

class IsolatedWebAppUpdateManagerFeatureFlagTest
    : public IsolatedWebAppUpdateManagerTest,
      public ::testing::WithParamInterface<FeatureFlagParam> {
 public:
  IsolatedWebAppUpdateManagerFeatureFlagTest()
      : IsolatedWebAppUpdateManagerTest(GetParam().feature_states) {}

 protected:
  void SetUp() override {
    IsolatedWebAppUpdateManagerTest::SetUp();
    // Disable manual overwrite of automatic update behavior and thus behave
    // like it would outside of tests.
    fake_provider().SetEnableAutomaticIwaUpdates(
        FakeWebAppProvider::AutomaticIwaUpdateStrategy::kDefault);
    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }
};

TEST_P(IsolatedWebAppUpdateManagerFeatureFlagTest,
       DoesUpdateDiscoveryIfFeatureFlagsAreEnabled) {
  AddDummyIsolatedAppToRegistry(profile(), GURL("isolated-app://a"), "iwa");

  EXPECT_THAT(
      update_manager().GetNextUpdateDiscoveryTimeForTesting().has_value(),
      Eq(GetParam().expected_result));
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    IsolatedWebAppUpdateManagerFeatureFlagTest,
    ::testing::Values(
// TODO: crbug.com/40274058 - Enable automatic updates on other platforms.
#if !BUILDFLAG(IS_CHROMEOS)
        FeatureFlagParam{
            .feature_states = {{features::kIsolatedWebApps, false}},
            .expected_result = false},
        FeatureFlagParam{.feature_states = {{features::kIsolatedWebApps, true}},
                         .expected_result = false}
#else   // BUILDFLAG(IS_CHROMEOS)
        FeatureFlagParam{
            .feature_states = {{features::kIsolatedWebApps, false},
                               {features::kIsolatedWebAppAutomaticUpdates,
                                true}},
            .expected_result = false},
        FeatureFlagParam{
            .feature_states = {{features::kIsolatedWebApps, true},
                               {features::kIsolatedWebAppAutomaticUpdates,
                                false}},
            .expected_result = false},
        FeatureFlagParam{
            .feature_states = {{features::kIsolatedWebApps, true},
                               {features::kIsolatedWebAppAutomaticUpdates,
                                true}},
            .expected_result = true}
#endif  // BUILDFLAG(IS_CHROMEOS)
        ));

}  // namespace

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
  IsolatedWebAppUpdateApplyTask::CompletionStatus status = base::ok();

  update_manager().TrackResultOfUpdateApplyTaskForTesting(status);

  EXPECT_THAT(
      histogram_tester.GetAllSamples("WebApp.Isolated.UpdateSuccess"),
      AllOf(SizeIs(1), BucketsAre(base::Bucket(/*update_success=*/true, 1))));
  histogram_tester.ExpectTotalCount("WebApp.Isolated.UpdateError", 0);
}

TEST_F(IsolatedWebAppUpdateManagerUpdateTest, UpdateApplyTaskFails) {
  base::HistogramTester histogram_tester;
  IsolatedWebAppUpdateApplyTask::CompletionStatus status =
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

}  // namespace web_app
