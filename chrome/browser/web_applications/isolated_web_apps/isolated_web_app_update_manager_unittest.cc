// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_update_manager.h"

#include <memory>
#include <string>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/values_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/to_vector.h"
#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "base/version.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_builder.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_location.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_policy_constants.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/fake_web_app_ui_manager.h"
#include "chrome/browser/web_applications/test/fake_web_contents_manager.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_contents/web_contents_manager.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/nacl/common/buildflags.h"
#include "components/web_package/signed_web_bundles/ed25519_public_key.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
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

using base::test::DictionaryHasValue;
using base::test::ToVector;
using ::testing::_;
using ::testing::Eq;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::IsFalse;
using ::testing::IsTrue;
using ::testing::Optional;
using ::testing::SizeIs;

blink::mojom::ManifestPtr CreateDefaultManifest(const GURL& application_url,
                                                base::StringPiece16 short_name,
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

#if BUILDFLAG(ENABLE_NACL)
class ScopedNaClBrowserDelegate {
 public:
  ~ScopedNaClBrowserDelegate() {
    nacl::NaClBrowser::ClearAndDeleteDelegateForTest();
  }

  void Init(ProfileManager* profile_manager) {
    nacl::NaClBrowser::SetDelegate(
        std::make_unique<NaClBrowserDelegateImpl>(profile_manager));
  }
};
#endif  // BUILDFLAG(ENABLE_NACL)

class IsolatedWebAppUpdateManagerTest : public WebAppTest {
 public:
  explicit IsolatedWebAppUpdateManagerTest(
      const base::flat_map<base::test::FeatureRef, bool>& feature_states =
          {{features::kIsolatedWebApps, true}})
      : WebAppTest(WebAppTest::WithTestUrlLoaderFactory(),
                   base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    scoped_feature_list_.InitWithFeatureStates(feature_states);
  }

  void SetUp() override {
    WebAppTest::SetUp();
#if BUILDFLAG(ENABLE_NACL)
    // Clearing Cache will clear PNACL cache, which needs this delegate set.
    nacl_browser_delegate_.Init(profile_manager().profile_manager());
#endif  // BUILDFLAG(ENABLE_NACL)
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

class IsolatedWebAppUpdateManagerUpdateTest
    : public IsolatedWebAppUpdateManagerTest {
 protected:
  struct IwaInfo {
    IwaInfo(web_package::WebBundleSigner::KeyPair key_pair,
            IsolatedWebAppLocation installed_location,
            base::Version installed_version,
            GURL update_manifest_url,
            GURL update_bundle_url,
            base::Version update_version,
            std::string update_app_name)
        : url_info(IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
              web_package::SignedWebBundleId::CreateForEd25519PublicKey(
                  key_pair.public_key))),
          key_pair(std::move(key_pair)),
          installed_location(std::move(installed_location)),
          installed_version(std::move(installed_version)),
          update_manifest_url(std::move(update_manifest_url)),
          update_bundle_url(std::move(update_bundle_url)),
          update_version(std::move(update_version)),
          update_app_name(std::move(update_app_name)) {}

    IsolatedWebAppUrlInfo url_info;
    web_package::WebBundleSigner::KeyPair key_pair;
    IsolatedWebAppLocation installed_location;
    base::Version installed_version;

    GURL update_manifest_url;
    GURL update_bundle_url;
    base::Version update_version;
    std::string update_app_name;
  };

  void SetUp() override {
    IsolatedWebAppUpdateManagerTest::SetUp();
    fake_provider().SetEnableAutomaticIwaUpdates(
        FakeWebAppProvider::AutomaticIwaUpdateStrategy::kForceEnabled);
    test::AwaitStartWebAppProviderAndSubsystems(profile());

    iwa_info1_ = IwaInfo(
        web_package::WebBundleSigner::KeyPair::CreateRandom(),
        InstalledBundle{
            .path = base::FilePath(FILE_PATH_LITERAL("/path/to/iwa1.swbn"))},
        base::Version("1.0.0"),
        GURL("https://example.com/update_manifest1.json"),
        GURL("https://example.com/bundle1.swbn"), base::Version("2.0.0"),
        "updated app 1");
    SetUpIwaInfo(*iwa_info1_);

    iwa_info2_ = IwaInfo(
        web_package::WebBundleSigner::KeyPair::CreateRandom(),
        InstalledBundle{
            .path = base::FilePath(FILE_PATH_LITERAL("/path/to/iwa2.swbn"))},
        base::Version("4.0.0"),
        GURL("https://example.com/update_manifest2.json"),
        GURL("https://example.com/bundle2.swbn"), base::Version("7.0.0"),
        "updated app 2");
    SetUpIwaInfo(*iwa_info2_);
  }

  void SetUpIwaInfo(const IwaInfo& iwa_info) {
    TestSignedWebBundle update_bundle =
        TestSignedWebBundleBuilder::BuildDefault(
            TestSignedWebBundleBuilder::BuildOptions()
                .SetVersion(iwa_info.update_version)
                .SetKeyPair(iwa_info.key_pair));

    profile_url_loader_factory().AddResponse(
        iwa_info.update_manifest_url.spec(),
        base::ReplaceStringPlaceholders(R"(
            { "versions": [ { "src": "$1", "version": "$2" } ] }
        )",
                                        {iwa_info.update_bundle_url.spec(),
                                         iwa_info.update_version.GetString()},
                                        /*offsets=*/nullptr));
    profile_url_loader_factory().AddResponse(
        iwa_info.update_bundle_url.spec(),
        std::string(update_bundle.data.begin(), update_bundle.data.end()));

    GURL install_url = iwa_info.url_info.origin().GetURL().Resolve(
        "/.well-known/_generated_install_page.html");

    auto& page_state =
        fake_web_contents_manager().GetOrCreatePageState(install_url);
    page_state.url_load_result = WebAppUrlLoaderResult::kUrlLoaded;
    page_state.error_code = webapps::InstallableStatusCode::NO_ERROR_DETECTED;
    page_state.manifest_url =
        iwa_info.url_info.origin().GetURL().Resolve("manifest.webmanifest");
    page_state.valid_manifest_for_web_app = true;
    page_state.opt_manifest = CreateDefaultManifest(
        iwa_info.url_info.origin().GetURL(),
        base::UTF8ToUTF16(iwa_info.update_app_name), iwa_info.update_version);
  }

  void SetIwaForceInstallPolicy(
      std::vector<std::pair<IsolatedWebAppUrlInfo, base::StringPiece>>
          entries) {
    base::Value::List list;
    for (const auto& [url_info, update_manifest_url] : entries) {
      list.Append(base::Value::Dict()
                      .Set(kPolicyWebBundleIdKey, url_info.web_bundle_id().id())
                      .Set(kPolicyUpdateManifestUrlKey, update_manifest_url));
    }
    profile()->GetPrefs()->SetList(prefs::kIsolatedWebAppInstallForceList,
                                   std::move(list));
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

  base::Value::List UpdateApplyLog() {
    return debug_log()
        .GetDict()
        .FindDict("task_queue")
        ->FindList("update_apply_log")
        ->Clone();
  }

  base::Value::List UpdateApplyWaitersLog() {
    return debug_log().GetDict().FindList("update_apply_waiters")->Clone();
  }

  auto UpdateLocationMatcher() {
    base::FilePath temp_dir;
    EXPECT_TRUE(base::GetTempDir(&temp_dir));

    return VariantWith<InstalledBundle>(
        Field("path", &InstalledBundle::path, test::IsInDir(temp_dir)));
  }

  absl::optional<IwaInfo> iwa_info1_;
  absl::optional<IwaInfo> iwa_info2_;
};

TEST_F(IsolatedWebAppUpdateManagerUpdateTest,
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
          web_package::SignedWebBundleId::CreateRandomForDevelopment());

  test::InstallDummyWebApp(profile(), "non-iwa", GURL("https://a"));
  AddDummyIsolatedAppToRegistry(
      profile(), iwa_info1_->url_info.origin().GetURL(), "installed iwa 1",
      WebApp::IsolationData(iwa_info1_->installed_location,
                            iwa_info1_->installed_version));
  AddDummyIsolatedAppToRegistry(
      profile(), dev_proxy_url_info.origin().GetURL(),
      "installed iwa 2 (dev mode proxy)",
      WebApp::IsolationData(
          DevModeProxy{.proxy_url = dev_proxy_url_info.origin()},
          base::Version("1.0.0")));
  AddDummyIsolatedAppToRegistry(
      profile(), dev_bundle_url_info.origin().GetURL(),
      "installed iwa 3 (dev mode bundle)",
      WebApp::IsolationData(DevModeBundle{.path = base::FilePath()},
                            base::Version("1.0.0")));
  AddDummyIsolatedAppToRegistry(profile(), GURL("isolated-app://b"),
                                "installed iwa 4");

  fake_ui_manager().SetNumWindowsForApp(iwa_info1_->url_info.app_id(), 1);

  SetIwaForceInstallPolicy(
      {{iwa_info1_->url_info, iwa_info1_->update_manifest_url.spec()},
       {non_installed_url_info, "https://example.com/update_manifest.json"},
       {dev_bundle_url_info, "https://example.com/update_manifest.json"},
       {dev_proxy_url_info, "https://example.com/update_manifest.json"}});

  task_environment()->FastForwardBy(base::Hours(5));
  task_environment()->RunUntilIdle();

  EXPECT_THAT(fake_provider().registrar_unsafe().GetAppById(
                  iwa_info1_->url_info.app_id()),
              IwaIs(Eq("installed iwa 1"),
                    test::IsolationDataIs(Eq(iwa_info1_->installed_location),
                                          Eq(iwa_info1_->installed_version),
                                          /*controlled_frame_partitions=*/_,
                                          test::PendingUpdateInfoIs(
                                              UpdateLocationMatcher(),
                                              Eq(base::Version("2.0.0"))))));

  ASSERT_THAT(UpdateDiscoveryLog(), SizeIs(1));
  EXPECT_THAT(
      UpdateDiscoveryLog()[0].GetDict(),
      DictionaryHasValue(
          "result", base::Value("Success::kUpdateFoundAndDryRunSuccessful")));
  EXPECT_THAT(UpdateApplyLog(), IsEmpty());

  // Temporary fix for crbug.com/1469880
  fake_provider().Shutdown();
}

TEST_F(IsolatedWebAppUpdateManagerUpdateTest,
       ApplysUpdatesAfterWindowIsClosed) {
  AddDummyIsolatedAppToRegistry(
      profile(), iwa_info1_->url_info.origin().GetURL(), "installed app",
      WebApp::IsolationData(iwa_info1_->installed_location,
                            iwa_info1_->installed_version));

  fake_ui_manager().SetNumWindowsForApp(iwa_info1_->url_info.app_id(), 1);

  SetIwaForceInstallPolicy(
      {{iwa_info1_->url_info, iwa_info1_->update_manifest_url.spec()}});
  task_environment()->FastForwardBy(base::Hours(5));
  task_environment()->RunUntilIdle();

  EXPECT_THAT(
      fake_provider().registrar_unsafe().GetAppById(
          iwa_info1_->url_info.app_id()),
      IwaIs(Eq("installed app"),
            test::IsolationDataIs(
                Eq(iwa_info1_->installed_location),
                Eq(iwa_info1_->installed_version),
                /*controlled_frame_partitions=*/_,
                test::PendingUpdateInfoIs(UpdateLocationMatcher(),
                                          Eq(iwa_info1_->update_version)))));

  ASSERT_THAT(UpdateDiscoveryLog(), SizeIs(1));
  EXPECT_THAT(
      UpdateDiscoveryLog()[0].GetDict(),
      DictionaryHasValue(
          "result", base::Value("Success::kUpdateFoundAndDryRunSuccessful")));
  EXPECT_THAT(UpdateApplyLog(), IsEmpty());

  fake_ui_manager().SetNumWindowsForApp(iwa_info1_->url_info.app_id(), 0);
  task_environment()->RunUntilIdle();

  ASSERT_THAT(UpdateApplyLog(), SizeIs(1));
  EXPECT_THAT(UpdateApplyLog()[0].GetDict(),
              DictionaryHasValue("result", base::Value("Success")));

  EXPECT_THAT(fake_provider().registrar_unsafe().GetAppById(
                  iwa_info1_->url_info.app_id()),
              IwaIs(iwa_info1_->update_app_name,
                    test::IsolationDataIs(
                        UpdateLocationMatcher(), Eq(iwa_info1_->update_version),
                        /*controlled_frame_partitions=*/_,
                        /*pending_update_info=*/Eq(absl::nullopt))));
}

TEST_F(IsolatedWebAppUpdateManagerUpdateTest,
       ApplysUpdatesWithHigherPriorityThanUpdateDiscovery) {
  AddDummyIsolatedAppToRegistry(
      profile(), iwa_info1_->url_info.origin().GetURL(), "installed app 1",
      WebApp::IsolationData(iwa_info1_->installed_location,
                            iwa_info1_->installed_version));
  AddDummyIsolatedAppToRegistry(
      profile(), iwa_info2_->url_info.origin().GetURL(), "installed app 2",
      WebApp::IsolationData(iwa_info2_->installed_location,
                            iwa_info2_->installed_version));

  SetIwaForceInstallPolicy(
      {{iwa_info1_->url_info, iwa_info1_->update_manifest_url.spec()},
       {iwa_info2_->url_info, iwa_info2_->update_manifest_url.spec()}});
  task_environment()->FastForwardBy(base::Hours(5));
  task_environment()->RunUntilIdle();

  auto update_discovery_log = UpdateDiscoveryLog();
  auto update_apply_log = UpdateApplyLog();

  ASSERT_THAT(update_discovery_log, SizeIs(2));
  EXPECT_THAT(
      update_discovery_log[0].GetDict(),
      DictionaryHasValue(
          "result", base::Value("Success::kUpdateFoundAndDryRunSuccessful")));
  EXPECT_THAT(
      update_discovery_log[1].GetDict(),
      DictionaryHasValue(
          "result", base::Value("Success::kUpdateFoundAndDryRunSuccessful")));

  ASSERT_THAT(update_apply_log, SizeIs(2));
  EXPECT_THAT(update_apply_log[0].GetDict(),
              DictionaryHasValue("result", base::Value("Success")));
  EXPECT_THAT(update_apply_log[1].GetDict(),
              DictionaryHasValue("result", base::Value("Success")));

  std::vector<base::Value*> times(
      {update_discovery_log[0].GetDict().Find("start_time"),
       update_discovery_log[0].GetDict().Find("end_time"),
       update_apply_log[0].GetDict().Find("start_time"),
       update_apply_log[0].GetDict().Find("end_time"),

       update_discovery_log[1].GetDict().Find("start_time"),
       update_discovery_log[1].GetDict().Find("end_time"),
       update_apply_log[1].GetDict().Find("start_time"),
       update_apply_log[1].GetDict().Find("end_time")});
  EXPECT_THAT(base::ranges::is_sorted(
                  times, {},
                  [](base::Value* value) { return *base::ValueToTime(value); }),
              IsTrue())
      << base::JoinString(ToVector(times, &base::Value::DebugString), ", ");

  EXPECT_THAT(fake_provider().registrar_unsafe().GetAppById(
                  iwa_info1_->url_info.app_id()),
              IwaIs(iwa_info1_->update_app_name,
                    test::IsolationDataIs(
                        UpdateLocationMatcher(), Eq(iwa_info1_->update_version),
                        /*controlled_frame_partitions=*/_,
                        /*pending_update_info=*/Eq(absl::nullopt))));
  EXPECT_THAT(fake_provider().registrar_unsafe().GetAppById(
                  iwa_info2_->url_info.app_id()),
              IwaIs(iwa_info2_->update_app_name,
                    test::IsolationDataIs(
                        UpdateLocationMatcher(), Eq(iwa_info2_->update_version),
                        /*controlled_frame_partitions=*/_,
                        /*pending_update_info=*/Eq(absl::nullopt))));
}

TEST_F(IsolatedWebAppUpdateManagerUpdateTest, StopsWaitingIfIwaIsUninstalled) {
  AddDummyIsolatedAppToRegistry(
      profile(), iwa_info1_->url_info.origin().GetURL(), "installed app",
      WebApp::IsolationData(iwa_info1_->installed_location,
                            iwa_info1_->installed_version));

  fake_ui_manager().SetNumWindowsForApp(iwa_info1_->url_info.app_id(), 1);

  SetIwaForceInstallPolicy(
      {{iwa_info1_->url_info, iwa_info1_->update_manifest_url.spec()}});
  task_environment()->FastForwardBy(base::Hours(5));
  task_environment()->RunUntilIdle();

  ASSERT_THAT(UpdateDiscoveryLog(), SizeIs(1));
  EXPECT_THAT(
      UpdateDiscoveryLog()[0].GetDict(),
      DictionaryHasValue(
          "result", base::Value("Success::kUpdateFoundAndDryRunSuccessful")));

  ASSERT_THAT(UpdateApplyWaitersLog(), SizeIs(1));
  EXPECT_THAT(
      UpdateApplyWaitersLog()[0].GetDict(),
      DictionaryHasValue("app_id", base::Value(iwa_info1_->url_info.app_id())));

  fake_ui_manager().SetNumWindowsForApp(iwa_info1_->url_info.app_id(), 0);
  task_environment()->RunUntilIdle();

  EXPECT_THAT(UpdateApplyWaitersLog(), IsEmpty());
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
  EXPECT_THAT(update_manager().GetUpdateDiscoveryTimerForTesting().IsRunning(),
              IsFalse());
}

TEST_F(IsolatedWebAppUpdateManagerDiscoveryTimerTest,
       StartsUpdateDiscoveryTimerWithAppropriateFrequency) {
  AddDummyIsolatedAppToRegistry(profile(), GURL("isolated-app://a"), "iwa");

  EXPECT_THAT(
      update_manager().GetUpdateDiscoveryTimerForTesting().GetCurrentDelay(),
      Eq(base::Hours(5)));
}

TEST_F(IsolatedWebAppUpdateManagerDiscoveryTimerTest,
       RunsUpdateDiscoveryWhileIwaIsInstalled) {
  EXPECT_THAT(update_manager().GetUpdateDiscoveryTimerForTesting().IsRunning(),
              IsFalse());

  AppId non_iwa_id =
      test::InstallDummyWebApp(profile(), "non-iwa", GURL("https://a"));
  EXPECT_THAT(update_manager().GetUpdateDiscoveryTimerForTesting().IsRunning(),
              IsFalse());

  AppId iwa_app_id1 = AddDummyIsolatedAppToRegistry(
      profile(), GURL("isolated-app://a"), "iwa1");
  EXPECT_THAT(update_manager().GetUpdateDiscoveryTimerForTesting().IsRunning(),
              IsTrue());

  AppId iwa_app_id2 = AddDummyIsolatedAppToRegistry(
      profile(), GURL("isolated-app://b"), "iwa2");
  EXPECT_THAT(update_manager().GetUpdateDiscoveryTimerForTesting().IsRunning(),
              IsTrue());

  test::UninstallWebApp(profile(), iwa_app_id1);
  EXPECT_THAT(update_manager().GetUpdateDiscoveryTimerForTesting().IsRunning(),
              IsTrue());

  test::UninstallWebApp(profile(), non_iwa_id);
  EXPECT_THAT(update_manager().GetUpdateDiscoveryTimerForTesting().IsRunning(),
              IsTrue());

  test::UninstallWebApp(profile(), iwa_app_id2);
  EXPECT_THAT(update_manager().GetUpdateDiscoveryTimerForTesting().IsRunning(),
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

  EXPECT_THAT(update_manager().GetUpdateDiscoveryTimerForTesting().IsRunning(),
              Eq(GetParam().expected_result));
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    IsolatedWebAppUpdateManagerFeatureFlagTest,
    ::testing::Values(
        FeatureFlagParam{.feature_states = {}, .expected_result = false},
        FeatureFlagParam{.feature_states = {{features::kIsolatedWebApps, true}},
                         .expected_result = false},
        FeatureFlagParam{
            .feature_states = {{features::kIsolatedWebAppAutomaticUpdates,
                                true}},
            .expected_result = false},
        FeatureFlagParam{
            .feature_states = {{features::kIsolatedWebApps, true},
                               {features::kIsolatedWebAppAutomaticUpdates,
                                true}},
            .expected_result = true}));

}  // namespace
}  // namespace web_app
