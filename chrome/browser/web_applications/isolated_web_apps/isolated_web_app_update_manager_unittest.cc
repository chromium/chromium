// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_update_manager.h"

#include <memory>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_builder.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_location.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_policy_constants.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
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

using ::testing::AllOf;
using ::testing::Eq;
using ::testing::Field;
using ::testing::IsFalse;
using ::testing::IsTrue;
using ::testing::NotNull;
using ::testing::Optional;
using ::testing::Pointee;
using ::testing::Property;

blink::mojom::ManifestPtr CreateDefaultManifest(const GURL& application_url,
                                                const base::Version version) {
  auto manifest = blink::mojom::Manifest::New();
  manifest->id = application_url.DeprecatedGetOriginAsURL();
  manifest->scope = application_url.Resolve("/");
  manifest->start_url = application_url.Resolve("/testing-start-url.html");
  manifest->display = DisplayMode::kStandalone;
  manifest->short_name = u"updated app";
  manifest->version = base::UTF8ToUTF16(version.GetString());

  return manifest;
}

MATCHER_P(IsInDir, directory, "") {
  *result_listener << "where the directory is " << directory;
  return arg.DirName() == directory;
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

  base::test::ScopedFeatureList scoped_feature_list_;
  data_decoder::test::InProcessDataDecoder data_decoder_;
#if BUILDFLAG(ENABLE_NACL)
  ScopedNaClBrowserDelegate nacl_browser_delegate_;
#endif  // BUILDFLAG(ENABLE_NACL)
};

class IsolatedWebAppUpdateManagerUpdateDiscoveryTest
    : public IsolatedWebAppUpdateManagerTest {
 protected:
  void SetUp() override {
    IsolatedWebAppUpdateManagerTest::SetUp();
    fake_provider().SetEnableAutomaticIwaUpdates(
        FakeWebAppProvider::AutomaticIwaUpdateStrategy::kForceEnabled);
    test::AwaitStartWebAppProviderAndSubsystems(profile());

    base::Version update_version("2.0.0");
    TestSignedWebBundle bundle =
        TestSignedWebBundleBuilder::BuildDefault({.version = update_version});

    profile_url_loader_factory().AddResponse(
        "https://example.com/update_manifest.json",
        base::ReplaceStringPlaceholders(R"(
        {
          "versions": [
            { "src": "https://example.com/bundle.swbn", "version": "$1" }
          ]
        }
    )",
                                        {update_version.GetString()}, nullptr));
    profile_url_loader_factory().AddResponse(
        "https://example.com/bundle.swbn",
        std::string(bundle.data.begin(), bundle.data.end()));

    GURL install_url = installed_url_info_.origin().GetURL().Resolve(
        "/.well-known/_generated_install_page.html");

    auto& page_state =
        fake_web_contents_manager().GetOrCreatePageState(install_url);
    page_state.url_load_result = WebAppUrlLoaderResult::kUrlLoaded;
    page_state.error_code = webapps::InstallableStatusCode::NO_ERROR_DETECTED;
    page_state.manifest_url =
        installed_url_info_.origin().GetURL().Resolve("manifest.webmanifest");
    page_state.valid_manifest_for_web_app = true;
    page_state.opt_manifest = CreateDefaultManifest(
        installed_url_info_.origin().GetURL(), update_version);
  }

  IsolatedWebAppUrlInfo installed_url_info_ =
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
          *web_package::SignedWebBundleId::Create(
              "4tkrnsmftl4ggvvdkfth3piainqragus2qbhf7rlz2a3wo3rh4wqaaic"));

  IsolatedWebAppLocation installed_location_ = InstalledBundle{
      .path = base::FilePath(FILE_PATH_LITERAL("/path/to/iwa.swbn"))};

  IsolatedWebAppUrlInfo non_installed_url_info_ =
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
          *web_package::SignedWebBundleId::Create(
              "5tkrnsmftl4ggvvdkfth3piainqragus2qbhf7rlz2a3wo3rh4wqaaic"));

  IsolatedWebAppUrlInfo dev_bundle_url_info_ =
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
          *web_package::SignedWebBundleId::Create(
              "aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic"));

  IsolatedWebAppUrlInfo dev_proxy_url_info_ =
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
          web_package::SignedWebBundleId::CreateRandomForDevelopment());
};

TEST_F(IsolatedWebAppUpdateManagerUpdateDiscoveryTest,
       DiscoversAndPreparesUpdateOfPolicyInstalledApps) {
  test::InstallDummyWebApp(profile(), "non-iwa", GURL("https://a"));
  AddDummyIsolatedAppToRegistry(
      profile(), installed_url_info_.origin().GetURL(), "installed iwa 1",
      WebApp::IsolationData(installed_location_, base::Version("1.0.0")));
  AddDummyIsolatedAppToRegistry(
      profile(), dev_proxy_url_info_.origin().GetURL(),
      "installed iwa 2 (dev mode proxy)",
      WebApp::IsolationData(
          DevModeProxy{.proxy_url = dev_proxy_url_info_.origin()},
          base::Version("1.0.0")));
  AddDummyIsolatedAppToRegistry(
      profile(), dev_bundle_url_info_.origin().GetURL(),
      "installed iwa 3 (dev mode bundle)",
      WebApp::IsolationData(DevModeBundle{.path = base::FilePath()},
                            base::Version("1.0.0")));
  AddDummyIsolatedAppToRegistry(profile(), GURL("isolated-app://b"),
                                "installed iwa 4");

  profile()->GetPrefs()->SetList(
      prefs::kIsolatedWebAppInstallForceList,
      base::Value::List()
          .Append(base::Value::Dict()
                      .Set(kPolicyUpdateManifestUrlKey,
                           "https://example.com/update_manifest.json")
                      .Set(kPolicyWebBundleIdKey,
                           installed_url_info_.web_bundle_id().id()))
          .Append(base::Value::Dict()
                      .Set(kPolicyUpdateManifestUrlKey,
                           "https://example.com/update_manifest.json")
                      .Set(kPolicyWebBundleIdKey,
                           non_installed_url_info_.web_bundle_id().id()))
          .Append(base::Value::Dict()
                      .Set(kPolicyUpdateManifestUrlKey,
                           "https://example.com/update_manifest.json")
                      .Set(kPolicyWebBundleIdKey,
                           dev_bundle_url_info_.web_bundle_id().id()))
          .Append(base::Value::Dict()
                      .Set(kPolicyUpdateManifestUrlKey,
                           "https://example.com/update_manifest.json")
                      .Set(kPolicyWebBundleIdKey,
                           dev_proxy_url_info_.web_bundle_id().id())));
  task_environment()->FastForwardBy(base::Hours(5));

  base::FilePath temp_dir;
  EXPECT_TRUE(base::GetTempDir(&temp_dir));

  const WebApp* web_app = fake_provider().registrar_unsafe().GetAppById(
      installed_url_info_.app_id());
  ASSERT_THAT(web_app, NotNull());
  EXPECT_THAT(web_app->untranslated_name(), Eq("installed iwa 1"));
  EXPECT_THAT(
      web_app->isolation_data(),
      Optional(AllOf(
          Field("location", &WebApp::IsolationData::location,
                Eq(installed_location_)),
          Field("version", &WebApp::IsolationData::version,
                Eq(base::Version("1.0.0"))),
          Property(
              "pending_update_info",
              &WebApp::IsolationData::pending_update_info,
              Optional(AllOf(
                  Field(
                      "location",
                      &WebApp::IsolationData::PendingUpdateInfo::location,
                      VariantWith<InstalledBundle>(Field(
                          "path", &InstalledBundle::path, IsInDir(temp_dir)))),
                  Field("version",
                        &WebApp::IsolationData::PendingUpdateInfo::version,
                        Eq(base::Version("2.0.0")))))))));

  base::Value debug_value = fake_provider().iwa_update_manager().AsDebugValue();
  base::Value::List* log =
      debug_value.GetDict().FindList("update_discovery_log");
  ASSERT_THAT(log, NotNull());
  ASSERT_THAT(log->size(), Eq(1ul));
  EXPECT_THAT(log->front().GetDict().FindString("result"),
              Pointee(Eq("Success::kUpdateFoundAndDryRunSuccessful")));
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
