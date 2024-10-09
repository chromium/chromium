// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_policy_manager.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/strings/to_string.h"
#include "base/test/bind.h"
#include "base/test/repeating_test_future.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/install_isolated_web_app_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_install_source.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_storage_location.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_update_discovery_task.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_update_manager.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolation_data.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_external_install_options.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/iwa_test_server_configurator.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/mock_isolated_web_app_install_command_wrapper.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/policy_generator.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/test_iwa_installer_factory.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/fake_web_contents_manager.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/nacl/common/buildflags.h"
#include "components/user_manager/user.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/common/content_features.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(ENABLE_NACL)
#include "chrome/browser/nacl_host/nacl_browser_delegate_impl.h"
#include "components/nacl/browser/nacl_browser.h"
#endif  // BUILDFLAG(ENABLE_NACL)

namespace web_app {

namespace {

using base::test::DictionaryHasValue;
using testing::_;
using ::testing::AllOf;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::IsNull;
using ::testing::NotNull;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::UnorderedElementsAre;

constexpr char kUpdateManifestUrl1[] =
    "https://example.com/1/update-manifest-1.json";
constexpr char kUpdateManifestUrl1Beta[] =
    "https://example.com/1/update-manifest-1-beta.json";
constexpr char kUpdateManifestUrl2[] =
    "https://example.com/2/update-manifest-2.json";
constexpr char kUpdateManifestUrl3[] =
    "https://example.com/3/update-manifest-3.json";
constexpr char kUpdateManifestUrl4[] =
    "https://example.com/4/update-manifest-4.json";
constexpr char kUpdateManifestUrl5[] =
    "https://example.com/5/update-manifest-5.json";
constexpr char kUpdateManifestUrl6[] =
    "https://example.com/6/update-manifest-6.json";
constexpr char kUpdateManifestUrl7[] =
    "https://example.com/7/update-manifest-7.json";

constexpr char kUpdateManifestValue1[] = R"(
    {"versions":[
      {"version": "1.0.0", "src": "https://example.com/not-used.swbn"},
      {"version": "7.0.6", "src": "https://example.com/app1.swbn"}]
    })";
constexpr char kUpdateManifestValue1Beta[] = R"(
    {"versions":[
      {"version": "1.0.0", "src": "https://example.com/not-used.swbn"},
      {"version": "7.0.6", "src": "https://example.com/app1.swbn"},
      {"version": "7.0.8", "src": "https://example.com/app1-beta.swbn", "channels":["beta"]}]
    })";
constexpr char kUpdateManifestValue2[] = R"(
    {"versions":
    [{"version": "3.0.0","src": "https://example.com/app2.swbn"}]})";
constexpr char kUpdateManifestValue3[] =
    "This update manifest should return error 404";
constexpr char kUpdateManifestValue4[] = R"(This is not JSON)";
// This manifest contains an invalid `src` URL.
constexpr char kUpdateManifestValue5[] = R"(
    {"versions":
    [{"version": "1.0.0", "src": "chrome-extension://app5.wbn"}]})";
constexpr char kUpdateManifestValue6[] = R"(
    {"versions":
    [{"version": "1.0.0","src": "https://example.com/app6.swbn"}]})";
constexpr char kUpdateManifestValue7[] = R"(
    {"versions":
    [{"version": "1.0.0", "src": "https://example.com/app7.swbn"}]})";

constexpr char kWebBundleId1[] =
    "aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic";
constexpr char kWebBundleId1Beta[] =
    "a2rugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic";
constexpr char kWebBundleId2[] =
    "berugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic";
constexpr char kWebBundleId3[] =
    "cerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic";
constexpr char kWebBundleId4[] =
    "derugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic";
constexpr char kWebBundleId5[] =
    "eerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic";
constexpr char kWebBundleId6[] =
    "herugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic";
constexpr char kWebBundleId7[] =
    "gerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic";

class MockIwaInstallCommandWrapper
    : public IwaInstaller::IwaInstallCommandWrapper {
 public:
  MockIwaInstallCommandWrapper() = default;
  ~MockIwaInstallCommandWrapper() override = default;

  MOCK_METHOD(void,
              Install,
              (const IsolatedWebAppInstallSource& install_source,
               const IsolatedWebAppUrlInfo& url_info,
               const base::Version& expected_version,
               WebAppCommandScheduler::InstallIsolatedWebAppCallback callback),
              (override));
};

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

#if BUILDFLAG(ENABLE_NACL)
class ScopedNaClBrowserDelegate {
 public:
  explicit ScopedNaClBrowserDelegate(ProfileManager* profile_manager) {
    nacl::NaClBrowser::SetDelegate(
        std::make_unique<NaClBrowserDelegateImpl>(profile_manager));
  }

  ~ScopedNaClBrowserDelegate() { nacl::NaClBrowser::ClearAndDeleteDelegate(); }
};
#endif  // BUILDFLAG(ENABLE_NACL)

void HandleInstallBasedOnId(
    const IsolatedWebAppInstallSource& install_source,
    const IsolatedWebAppUrlInfo& url_info,
    const base::Version& expected_version,
    WebAppCommandScheduler::InstallIsolatedWebAppCallback callback) {
  if (url_info.web_bundle_id().id() == kWebBundleId1 ||
      url_info.web_bundle_id().id() == kWebBundleId2 ||
      url_info.web_bundle_id().id() == kWebBundleId1Beta) {
    if (url_info.web_bundle_id().id() == kWebBundleId1) {
      EXPECT_EQ(expected_version, base::Version("7.0.6"));
    } else if (url_info.web_bundle_id().id() == kWebBundleId2) {
      EXPECT_EQ(expected_version, base::Version("3.0.0"));
    } else if (url_info.web_bundle_id().id() == kWebBundleId1Beta) {
      EXPECT_EQ(expected_version, base::Version("7.0.8"));
    }

    std::move(callback).Run(InstallIsolatedWebAppCommandSuccess(
        url_info, expected_version,
        IwaStorageOwnedBundle{"random_folder", /*dev_mode=*/false}));
    return;
  }

  std::move(callback).Run(base::unexpected{InstallIsolatedWebAppCommandError{
      .message = std::string{"Install error message"}}});
}

}  // namespace

namespace internal {

struct IwaInstallerTestParam {
  bool is_mgs_install_enabled;
  bool is_user_session;
  std::string bundle_id;
  std::string manifest_url;
  IwaInstallerResult::Type result_type;
  std::optional<std::string> update_channel;
};

class IwaInstallerTest
    : public ::testing::TestWithParam<IwaInstallerTestParam> {
 public:
  IwaInstallerTest()
      : shared_url_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_factory_)) {
    std::vector<base::test::FeatureRef> enabled_features = {
        features::kIsolatedWebApps};
    if (GetParam().is_mgs_install_enabled) {
      enabled_features.push_back(
          features::kIsolatedWebAppManagedGuestSessionInstall);
    }
    scoped_feature_list_.InitWithFeatures(std::move(enabled_features),
                                          /*disabled_features=*/{});
  }

 protected:
  void SetUp() override {
    ASSERT_TRUE(dir_.CreateUniqueTempDir());
    AddJsonResponse(kUpdateManifestUrl1, kUpdateManifestValue1);
    AddJsonResponse(kUpdateManifestUrl1Beta, kUpdateManifestValue1Beta);
    AddJsonResponse(kUpdateManifestUrl2, kUpdateManifestValue2);
    test_factory_.AddResponse(kUpdateManifestUrl3, kUpdateManifestValue3,
                              net::HttpStatusCode::HTTP_NOT_FOUND);
    AddJsonResponse(kUpdateManifestUrl4, kUpdateManifestValue4);
    AddJsonResponse(kUpdateManifestUrl5, kUpdateManifestValue5);
    AddJsonResponse(kUpdateManifestUrl6, kUpdateManifestValue6);
    AddJsonResponse(kUpdateManifestUrl7, kUpdateManifestValue7);
    test_factory_.AddResponse("https://example.com/app1.swbn",
                              "Content of app1");
    test_factory_.AddResponse("https://example.com/app1-beta.swbn",
                              "Content of app1-beta");
    test_factory_.AddResponse("https://example.com/app2.swbn",
                              "Content of app2");
    test_factory_.AddResponse("https://example.com/app6.swbn",
                              "Content of app6");
    test_factory_.AddResponse("https://example.com/app7.swbn", "",
                              net::HttpStatusCode::HTTP_NOT_FOUND);

    if (!GetParam().is_user_session) {
      test_managed_guest_session_ =
          std::make_unique<profiles::testing::ScopedTestManagedGuestSession>();
    }
  }

  void TearDown() override { test_factory_.ClearResponses(); }

  void AddJsonResponse(std::string_view url, std::string_view content) {
    network::mojom::URLResponseHeadPtr head =
        network::CreateURLResponseHead(net::HttpStatusCode::HTTP_OK);
    head->mime_type = "application/json";
    network::URLLoaderCompletionStatus status;
    test_factory_.AddResponse(GURL(url), std::move(head), std::string(content),
                              status);
  }

  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir dir_;
  network::TestURLLoaderFactory test_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  std::unique_ptr<profiles::testing::ScopedTestManagedGuestSession>
      test_managed_guest_session_;
  base::test::ScopedFeatureList scoped_feature_list_;

  IsolatedWebAppExternalInstallOptions install_options_ =
      IsolatedWebAppExternalInstallOptions::FromPolicyPrefValue(
          PolicyGenerator::CreatePolicyEntry(
              /*web_bundle_id=*/GetParam().bundle_id,
              /*update_manifest_url=*/GetParam().manifest_url,
              /*update_channel=*/GetParam().update_channel))
          .value();
};

// This test case represents the regular flow of force installing IWA for
// ephemeral session. The install options will cover cases of success for
// both, managed guest sessions and managed user sessions.
TEST_P(IwaInstallerTest, MgsRegularFlow) {
  base::test::TestFuture<IwaInstallerResult> future;
  base::Value::List log;

  auto install_command = std::make_unique<MockIwaInstallCommandWrapper>();
  EXPECT_CALL(*install_command, Install(_, _, _, _))
      .WillRepeatedly(Invoke(
          [](const IsolatedWebAppInstallSource& install_source,
             const IsolatedWebAppUrlInfo& url_info,
             const base::Version& expected_version,
             WebAppCommandScheduler::InstallIsolatedWebAppCallback callback) {
            HandleInstallBasedOnId(install_source, url_info, expected_version,
                                   std::move(callback));
          }));
  IwaInstaller installer(install_options_, shared_url_loader_factory_,
                         std::move(install_command), log, future.GetCallback());
  installer.Start();

  EXPECT_THAT(future.Get(),
              Property("type", &IwaInstallerResult::type,
                       Eq(!GetParam().is_user_session &&
                                  !GetParam().is_mgs_install_enabled
                              ? IwaInstallerResult::Type::
                                    kErrorManagedGuestSessionInstallDisabled
                              : GetParam().result_type)));
}

TEST_P(IwaInstallerTest, NotMgs) {
  test_managed_guest_session_.reset();

  base::test::TestFuture<IwaInstallerResult> future;
  base::Value::List log;

  auto install_command = std::make_unique<MockIwaInstallCommandWrapper>();
  EXPECT_CALL(*install_command, Install(_, _, _, _))
      .WillRepeatedly(Invoke(
          [](const IsolatedWebAppInstallSource& install_source,
             const IsolatedWebAppUrlInfo& url_info,
             const base::Version& expected_version,
             WebAppCommandScheduler::InstallIsolatedWebAppCallback callback) {
            HandleInstallBasedOnId(install_source, url_info, expected_version,
                                   std::move(callback));
          }));
  IwaInstaller installer(install_options_, shared_url_loader_factory_,
                         std::move(install_command), log, future.GetCallback());
  installer.Start();

  EXPECT_THAT(future.Get(), Property("type", &IwaInstallerResult::type,
                                     Eq(GetParam().result_type)));
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    IwaInstallerTest,
    ::testing::ValuesIn(std::vector<IwaInstallerTestParam>{
        // App 1 represents the most general case: the Update Manifest has
        // several records. We should determine the latest version, download
        // the appropriate file and install the app. It is successful case.
        {.is_mgs_install_enabled = true,
         .is_user_session = true,
         .bundle_id = kWebBundleId1,
         .manifest_url = kUpdateManifestUrl1,
         .result_type = IwaInstallerResult::Type::kSuccess},
        // Same as the first test case, but with non-default release channel.
        {.is_mgs_install_enabled = true,
         .is_user_session = true,
         .bundle_id = kWebBundleId1Beta,
         .manifest_url = kUpdateManifestUrl1Beta,
         .result_type = IwaInstallerResult::Type::kSuccess,
         .update_channel = "beta"},
        // Same as the first test case, but inside a managed guest session.
        {.is_mgs_install_enabled = true,
         .is_user_session = false,
         .bundle_id = kWebBundleId1,
         .manifest_url = kUpdateManifestUrl1,
         .result_type = IwaInstallerResult::Type::kSuccess},
        // App 2 is similar to App 1 but has only one record in the Update
        // Manifest.
        {.is_mgs_install_enabled = true,
         .is_user_session = true,
         .bundle_id = kWebBundleId2,
         .manifest_url = kUpdateManifestUrl2,
         .result_type = IwaInstallerResult::Type::kSuccess},
        // We can't download Update Manifest for the app 3.
        {.is_mgs_install_enabled = true,
         .is_user_session = true,
         .bundle_id = kWebBundleId3,
         .manifest_url = kUpdateManifestUrl3,
         .result_type =
             IwaInstallerResult::Type::kErrorUpdateManifestDownloadFailed},
        // App 4 represents the case where the Update Manifest if not parsable.
        {.is_mgs_install_enabled = true,
         .is_user_session = true,
         .bundle_id = kWebBundleId4,
         .manifest_url = kUpdateManifestUrl4,
         .result_type =
             IwaInstallerResult::Type::kErrorUpdateManifestParsingFailed},
        // Release channel is not assigned to any version of the app.
        {.is_mgs_install_enabled = true,
         .is_user_session = true,
         .bundle_id = kWebBundleId1Beta,
         .manifest_url = kUpdateManifestUrl1,
         .result_type =
             IwaInstallerResult::Type::kErrorWebBundleUrlCantBeDetermined,
         .update_channel = "beta"},
        // The Web Bundle URL of the App 5 is not valid.
        {.is_mgs_install_enabled = true,
         .is_user_session = true,
         .bundle_id = kWebBundleId5,
         .manifest_url = kUpdateManifestUrl5,
         .result_type =
             IwaInstallerResult::Type::kErrorWebBundleUrlCantBeDetermined},
        // The Web Bundle of the App 6 can't be installed.
        {.is_mgs_install_enabled = true,
         .is_user_session = true,
         .bundle_id = kWebBundleId6,
         .manifest_url = kUpdateManifestUrl6,
         .result_type =
             IwaInstallerResult::Type::kErrorCantInstallFromWebBundle},
        // The Web Bundle file of the App 7 can't be downloaded.
        {.is_mgs_install_enabled = true,
         .is_user_session = true,
         .bundle_id = kWebBundleId7,
         .manifest_url = kUpdateManifestUrl7,
         .result_type = IwaInstallerResult::Type::kErrorCantDownloadWebBundle},
        {.is_mgs_install_enabled = false,
         .is_user_session = false,
         .bundle_id = kWebBundleId1,
         .manifest_url = kUpdateManifestUrl1,
         .result_type = IwaInstallerResult::Type::kSuccess}}));

}  // namespace internal

constexpr char kUpdateManifestUrlApp1[] =
    "https://example.com/manifest_app1.json";
constexpr char kUpdateManifestUrlApp2[] =
    "https://example.com/manifest_app2.json";

constexpr char kUpdateManifestValueApp1[] = R"(
    {"versions":
      [{"version": "1.0.0","src": "https://example.com/web_bundle_app1.swbn"}]})";
constexpr char kUpdateManifestValueApp2[] = R"(
    {"versions":
    [{"version": "1.0.0","src": "https://example.com/web_bundle_app2.swbn"}]})";

class IsolatedWebAppPolicyManagerTestBase : public WebAppTest {
 public:
  explicit IsolatedWebAppPolicyManagerTestBase(
      bool is_mgs_session_install_enabled,
      bool is_user_session,
      base::test::TaskEnvironment::TimeSource time_source =
          base::test::TaskEnvironment::TimeSource::DEFAULT)
      : WebAppTest(WebAppTest::WithTestUrlLoaderFactory(), time_source),
        is_mgs_session_install_enabled_(is_mgs_session_install_enabled),
        is_user_session_(is_user_session) {
    std::vector<base::test::FeatureRef> enabled_features = {
        features::kIsolatedWebApps};
    if (is_mgs_session_install_enabled_) {
      enabled_features.push_back(
          features::kIsolatedWebAppManagedGuestSessionInstall);
    }

    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/
        std::move(enabled_features),
        /*disabled_features=*/{});
  }

  void SetUpServedIwas() {
    web_app::TestSignedWebBundle swbn_app1 =
        web_app::TestSignedWebBundleBuilder::BuildDefault();
    web_app::TestSignedWebBundle swbn_app2 =
        web_app::TestSignedWebBundleBuilder::BuildDefault(
            TestSignedWebBundleBuilder::BuildOptions().AddKeyPair(
                web_package::test::Ed25519KeyPair::CreateRandom()));

    lazy_app1_id_ = swbn_app1.id;
    lazy_app2_id_ = swbn_app2.id;

    IwaTestServerConfigurator configurator;
    configurator.AddUpdateManifest("manifest_app1.json",
                                   kUpdateManifestValueApp1);
    configurator.AddSignedWebBundle("web_bundle_app1.swbn",
                                    std::move(swbn_app1));
    configurator.AddUpdateManifest("manifest_app2.json",
                                   kUpdateManifestValueApp2);
    configurator.AddSignedWebBundle("web_bundle_app2.swbn",
                                    std::move(swbn_app2));
    configurator.ConfigureURLLoader(GURL("https://example.com/"),
                                    profile_url_loader_factory(),
                                    fake_web_contents_manager());
  }

  void SetUp() override {
    WebAppTest::SetUp();
    SetCommandScheduler();
    test::AwaitStartWebAppProviderAndSubsystems(profile());
    SetUpServedIwas();

    if (!is_user_session_) {
      test_managed_guest_session_ =
          std::make_unique<profiles::testing::ScopedTestManagedGuestSession>();
    }

#if BUILDFLAG(ENABLE_NACL)
    // Uninstalling an IWA will clear PNACL cache, which needs this delegate
    // set.
    nacl_browser_delegate_ = std::make_unique<ScopedNaClBrowserDelegate>(
        profile_manager().profile_manager());
#endif  // BUILDFLAG(ENABLE_NACL)
  }

  virtual void SetCommandScheduler() = 0;

  sync_preferences::TestingPrefServiceSyncable* pref_service() {
    return profile()->GetTestingPrefService();
  }

  WebAppProvider& provider() { return *WebAppProvider::GetForTest(profile()); }
  FakeWebContentsManager& fake_web_contents_manager() {
    return static_cast<FakeWebContentsManager&>(
        provider().web_contents_manager());
  }

  void AssertAppInstalled(const web_package::SignedWebBundleId& swbn_id) {
    const WebApp* web_app = fake_provider().registrar_unsafe().GetAppById(
        IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(swbn_id).app_id());
    ASSERT_THAT(web_app, testing::NotNull()) << "The app in not installed :(";
  }

  bool IsManagedGuestSessionInstallEnabled() {
    return is_mgs_session_install_enabled_;
  }

  const web_package::SignedWebBundleId& get_app1_id() { return *lazy_app1_id_; }
  const web_package::SignedWebBundleId& get_app2_id() { return *lazy_app2_id_; }

 private:
  const bool is_mgs_session_install_enabled_;
  const bool is_user_session_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<profiles::testing::ScopedTestManagedGuestSession>
      test_managed_guest_session_;
  data_decoder::test::InProcessDataDecoder data_decoder_;
#if BUILDFLAG(ENABLE_NACL)
  std::unique_ptr<ScopedNaClBrowserDelegate> nacl_browser_delegate_;
#endif  // BUILDFLAG(ENABLE_NACL)

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
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(get_app1_id());

  WebAppTestInstallObserver install_observer(profile());
  install_observer.BeginListening({url_info.app_id()});

  PolicyGenerator policy_generator;
  policy_generator.AddForceInstalledIwa(url_info.web_bundle_id(),
                                        GURL(kUpdateManifestUrlApp1));

  profile()->GetPrefs()->Set(prefs::kIsolatedWebAppInstallForceList,
                             policy_generator.Generate());

  EXPECT_EQ(install_observer.Wait(), url_info.app_id());
  task_environment()->RunUntilIdle();

  const WebApp* web_app =
      fake_provider().registrar_unsafe().GetAppById(url_info.app_id());
  ASSERT_THAT(web_app, NotNull());
  EXPECT_THAT(web_app->GetSources(),
              Eq(WebAppManagementTypes({WebAppManagement::Type::kIwaPolicy})));
}

TEST_F(IsolatedWebAppPolicyManagerTest,
       AppSourceAddedWhenPreviouslyUserInstalled) {
  auto url_info =
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(get_app1_id());
  AddDummyIsolatedAppToRegistry(
      profile(), url_info.origin().GetURL(), "iwa",
      IsolationData::Builder(
          IwaStorageOwnedBundle("some_folder", /*dev_mode=*/false),
          base::Version("1.0.0"))
          .Build(),
      webapps::WebappInstallSource::IWA_GRAPHICAL_INSTALLER);
  {
    const WebApp* web_app =
        fake_provider().registrar_unsafe().GetAppById(url_info.app_id());
    ASSERT_THAT(web_app, NotNull());
    EXPECT_THAT(
        web_app->GetSources(),
        Eq(WebAppManagementTypes({WebAppManagement::Type::kIwaUserInstalled})));
  }

  PolicyGenerator policy_generator;
  policy_generator.AddForceInstalledIwa(url_info.web_bundle_id(),
                                        GURL(kUpdateManifestUrlApp1));
  profile()->GetPrefs()->Set(prefs::kIsolatedWebAppInstallForceList,
                             policy_generator.Generate());

  task_environment()->RunUntilIdle();
  {
    const WebApp* web_app =
        fake_provider().registrar_unsafe().GetAppById(url_info.app_id());
    ASSERT_THAT(web_app, NotNull());
    EXPECT_THAT(
        web_app->GetSources(),
        Eq(WebAppManagementTypes({WebAppManagement::Type::kIwaUserInstalled,
                                  WebAppManagement::Type::kIwaPolicy})));
  }

  auto debug_log = provider().iwa_update_manager().AsDebugValue();
  EXPECT_THAT(
      debug_log.GetDict()
          .FindDict("task_queue")
          ->FindList("update_discovery_log"),
      Pointee(UnorderedElementsAre(Property(
          "GetDict", &base::Value::GetDict,
          AllOf(DictionaryHasValue("app_id", base::Value(url_info.app_id())),
                DictionaryHasValue(
                    "result", base::Value(base::ToString(
                                  IsolatedWebAppUpdateDiscoveryTask::Success::
                                      kNoUpdateFound))))))))
      << debug_log;
}

TEST_F(IsolatedWebAppPolicyManagerTest,
       AppInstalledWhenPreviouslyDevInstalled) {
  auto url_info =
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(get_app1_id());
  AddDummyIsolatedAppToRegistry(
      profile(), url_info.origin().GetURL(), "iwa",
      IsolationData::Builder(
          IwaStorageOwnedBundle("some_folder", /*dev_mode=*/true),
          base::Version("1.0.0"))
          .Build(),
      webapps::WebappInstallSource::IWA_DEV_UI);

  WebAppTestUninstallObserver uninstall_observer(profile());
  uninstall_observer.BeginListening({url_info.app_id()});
  WebAppTestInstallObserver install_observer(profile());
  install_observer.BeginListening({url_info.app_id()});

  PolicyGenerator policy_generator;
  policy_generator.AddForceInstalledIwa(url_info.web_bundle_id(),
                                        GURL(kUpdateManifestUrlApp1));
  profile()->GetPrefs()->Set(prefs::kIsolatedWebAppInstallForceList,
                             policy_generator.Generate());

  // Dev-mode apps should be fully uninstalled before they can be
  // force-installed.
  EXPECT_EQ(uninstall_observer.Wait(), url_info.app_id());
  EXPECT_EQ(install_observer.Wait(), url_info.app_id());
  task_environment()->RunUntilIdle();

  const WebApp* web_app =
      fake_provider().registrar_unsafe().GetAppById(url_info.app_id());
  ASSERT_THAT(web_app, NotNull());
  EXPECT_THAT(web_app->GetSources(),
              Eq(WebAppManagementTypes({WebAppManagement::Type::kIwaPolicy})));
}

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
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(get_app1_id());

  PolicyGenerator policy_generator;
  policy_generator.AddForceInstalledIwa(url_info.web_bundle_id(),
                                        GURL(kUpdateManifestUrlApp1));
  profile()->GetPrefs()->Set(prefs::kIsolatedWebAppInstallForceList,
                             policy_generator.Generate());

  provider().command_manager().AwaitAllCommandsCompleteForTesting();
  task_environment()->RunUntilIdle();

  const WebApp* web_app =
      fake_provider().registrar_unsafe().GetAppById(url_info.app_id());
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
    fake_provider().SetScheduler(std::move(scheduler));
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
    PolicyGenerator policy_generator;
    policy_generator.AddForceInstalledIwa(get_app1_id(),
                                          GURL(kUpdateManifestUrlApp1));
    profile()->GetPrefs()->Set(prefs::kIsolatedWebAppInstallForceList,
                               policy_generator.Generate());
  }

  task_environment()->RunUntilIdle();

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

  task_environment()->RunUntilIdle();

  // The third policy update. This one must be processed.
  {
    PolicyGenerator policy_generator;
    policy_generator.AddForceInstalledIwa(get_app1_id(),
                                          GURL(kUpdateManifestUrlApp1));
    policy_generator.AddForceInstalledIwa(get_app2_id(),
                                          GURL(kUpdateManifestUrlApp2));
    profile()->GetPrefs()->Set(prefs::kIsolatedWebAppInstallForceList,
                               policy_generator.Generate());
  }

  // Finish the installation of the app1 from the first policy update.
  EXPECT_THAT(get_command_scheduler()->FinishWithError(), Eq(get_app1_id()));
  task_environment()->RunUntilIdle();

  // The second policy update is ignored as it was replaced by the third one.

  // Processing the third policy update.
  std::vector<web_package::SignedWebBundleId> ids;

  // Finish app1 from the third policy update.
  ids.push_back(get_command_scheduler()->FinishWithError());
  task_environment()->RunUntilIdle();

  // Finish app2 from the third policy update.
  ids.push_back(get_command_scheduler()->FinishWithError());
  task_environment()->RunUntilIdle();

  EXPECT_THAT(ids, UnorderedElementsAre(get_app1_id(), get_app2_id()));
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
      UninstallJob::Callback callback,
      const base::Location& location) override {
    tried_to_uninstall_ = true;
    EXPECT_TRUE(base::Contains(expected_apps_to_remove_, app_id));
    EXPECT_EQ(management_type, WebAppManagement::Type::kIwaPolicy);
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

 private:
  base::flat_set<webapps::AppId> expected_apps_to_remove_;
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
    PolicyGenerator policy_generator_2_apps;
    policy_generator_2_apps.AddForceInstalledIwa(get_app1_id(),
                                                 GURL(kUpdateManifestUrlApp1));
    policy_generator_2_apps.AddForceInstalledIwa(get_app2_id(),
                                                 GURL(kUpdateManifestUrlApp2));

    profile()->GetPrefs()->Set(prefs::kIsolatedWebAppInstallForceList,
                               policy_generator_2_apps.Generate());

    task_environment()->RunUntilIdle();

    AssertAppInstalled(get_app1_id());
    AssertAppInstalled(get_app2_id());
  }

  // Now generate a policy with 1 app and expect an attempt to
  // remove the other app.
  {
    PolicyGenerator policy_generator_1_app;
    policy_generator_1_app.AddForceInstalledIwa(get_app1_id(),
                                                GURL(kUpdateManifestUrlApp1));

    const webapps::AppId app2_id =
        IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(get_app2_id())
            .app_id();
    get_command_scheduler()->AddExpectedToUninstallApp(app2_id);
    EXPECT_EQ(get_command_scheduler()->GetNumberOfAppsRemainingToUninstall(),
              1U);

    profile()->GetPrefs()->Set(prefs::kIsolatedWebAppInstallForceList,
                               policy_generator_1_app.Generate());

    task_environment()->RunUntilIdle();

    EXPECT_EQ(get_command_scheduler()->GetNumberOfAppsRemainingToUninstall(),
              0U);
  }
}

TEST_F(IsolatedWebAppPolicyManagerUninstallTest, BothAppUninstalled) {
  // Force install 2 apps.
  {
    PolicyGenerator policy_generator;
    policy_generator.AddForceInstalledIwa(get_app1_id(),
                                          GURL(kUpdateManifestUrlApp1));
    policy_generator.AddForceInstalledIwa(get_app2_id(),
                                          GURL(kUpdateManifestUrlApp2));

    profile()->GetPrefs()->Set(prefs::kIsolatedWebAppInstallForceList,
                               policy_generator.Generate());

    task_environment()->RunUntilIdle();

    AssertAppInstalled(get_app1_id());
    AssertAppInstalled(get_app2_id());
  }

  // Set the policy without any app and expect an attempt to uninstall
  // both previously installed apps.
  {
    const webapps::AppId app1_id =
        IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(get_app1_id())
            .app_id();
    const webapps::AppId app2_id =
        IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(get_app2_id())
            .app_id();

    WebAppTestUninstallObserver uninstall_observer(profile());
    uninstall_observer.BeginListening({app1_id, app2_id});

    get_command_scheduler()->AddExpectedToUninstallApp(app1_id);
    get_command_scheduler()->AddExpectedToUninstallApp(app2_id);
    EXPECT_EQ(get_command_scheduler()->GetNumberOfAppsRemainingToUninstall(),
              2U);

    PolicyGenerator empty_policy;
    profile()->GetPrefs()->Set(prefs::kIsolatedWebAppInstallForceList,
                               empty_policy.Generate());

    uninstall_observer.Wait();

    // WebAppTestUninstallObserver already triggers when the app is not fully
    // uninstalled. This causes issues with references to destroyed profiles
    // (see https://crbug.com/41484323#comment7). Wait until the app is actually
    // uninstalled here.
    task_environment()->RunUntilIdle();

    EXPECT_EQ(get_command_scheduler()->GetNumberOfAppsRemainingToUninstall(),
              0U);
  }
}

TEST_F(IsolatedWebAppPolicyManagerUninstallTest,
       UserInstalledAppNotUninstalled) {
  auto url_info =
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(get_app1_id());

  // User-install the app.
  {
    AddDummyIsolatedAppToRegistry(
        profile(), url_info.origin().GetURL(), "iwa",
        IsolationData::Builder(
            IwaStorageOwnedBundle("some_folder", /*dev_mode=*/false),
            base::Version("1.0.0"))
            .Build(),
        webapps::WebappInstallSource::IWA_GRAPHICAL_INSTALLER);

    const WebApp* web_app =
        fake_provider().registrar_unsafe().GetAppById(url_info.app_id());
    ASSERT_THAT(web_app, NotNull());
    EXPECT_THAT(
        web_app->GetSources(),
        Eq(WebAppManagementTypes({WebAppManagement::Type::kIwaUserInstalled})));
  }

  // Force install the app via policy.
  {
    PolicyGenerator policy_generator;
    policy_generator.AddForceInstalledIwa(url_info.web_bundle_id(),
                                          GURL(kUpdateManifestUrlApp1));
    profile()->GetPrefs()->Set(prefs::kIsolatedWebAppInstallForceList,
                               policy_generator.Generate());

    task_environment()->RunUntilIdle();

    const WebApp* web_app =
        fake_provider().registrar_unsafe().GetAppById(url_info.app_id());
    ASSERT_THAT(web_app, NotNull());
    EXPECT_THAT(
        web_app->GetSources(),
        Eq(WebAppManagementTypes({WebAppManagement::Type::kIwaUserInstalled,
                                  WebAppManagement::Type::kIwaPolicy})));
  }

  // Set the policy without any app and expect an attempt to remove the policy
  // install source.
  {
    get_command_scheduler()->AddExpectedToUninstallApp(url_info.app_id());
    EXPECT_EQ(get_command_scheduler()->GetNumberOfAppsRemainingToUninstall(),
              1U);
    WebAppInstallManagerObserverAdapter observer(profile());
    base::test::RepeatingTestFuture<const webapps::AppId&>
        source_removed_future;
    observer.SetWebAppSourceRemovedDelegate(
        source_removed_future.GetCallback());

    PolicyGenerator empty_policy;
    profile()->GetPrefs()->Set(prefs::kIsolatedWebAppInstallForceList,
                               empty_policy.Generate());

    EXPECT_EQ(source_removed_future.Take(), url_info.app_id());
    EXPECT_EQ(get_command_scheduler()->GetNumberOfAppsRemainingToUninstall(),
              0U);

    const WebApp* web_app =
        fake_provider().registrar_unsafe().GetAppById(url_info.app_id());
    ASSERT_THAT(web_app, NotNull());
    EXPECT_THAT(
        web_app->GetSources(),
        Eq(WebAppManagementTypes({WebAppManagement::Type::kIwaUserInstalled})));
  }
}

// There should not be any attempt to uninstall an app if no apps have been
// removed from the apps.
TEST_F(IsolatedWebAppPolicyManagerUninstallTest, NoAppsUninstalled) {
  PolicyGenerator policy_generator;
  policy_generator.AddForceInstalledIwa(get_app1_id(),
                                        GURL(kUpdateManifestUrlApp1));
  profile()->GetPrefs()->Set(prefs::kIsolatedWebAppInstallForceList,
                             policy_generator.Generate());
  task_environment()->RunUntilIdle();

  AssertAppInstalled(get_app1_id());

  policy_generator.AddForceInstalledIwa(get_app2_id(),
                                        GURL(kUpdateManifestUrlApp2));
  profile()->GetPrefs()->Set(prefs::kIsolatedWebAppInstallForceList,
                             policy_generator.Generate());
  task_environment()->RunUntilIdle();

  AssertAppInstalled(get_app1_id());
  AssertAppInstalled(get_app2_id());
  EXPECT_FALSE(get_command_scheduler()->TriedToUninstall());
}

class IsolatedWebAppRetryTest : public IsolatedWebAppPolicyManagerTestBase {
 public:
  IsolatedWebAppRetryTest()
      : IsolatedWebAppPolicyManagerTestBase(
            /*is_mgs_session_install_enabled=*/false,
            /*is_user_session=*/true,
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

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
};

TEST_F(IsolatedWebAppRetryTest, FirstInstallFailsRetrySucceeds) {
  auto url_info =
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(get_app1_id());
  iwa_installer_factory_.SetCommandBehavior(
      url_info.web_bundle_id().id(),
      /*execution_mode=*/
      MockIsolatedWebAppInstallCommandWrapper::ExecutionMode::kSimulateFailure,
      /*execute_immediately=*/true);

  PolicyGenerator policy_generator;
  policy_generator.AddForceInstalledIwa(url_info.web_bundle_id(),
                                        GURL(kUpdateManifestUrlApp1));
  profile()->GetPrefs()->Set(prefs::kIsolatedWebAppInstallForceList,
                             policy_generator.Generate());

  // Run the first attempt to install the isolated web app (which should fail).
  task_environment()->FastForwardBy(base::TimeDelta(base::Seconds(1)));

  ASSERT_EQ(1u, iwa_installer_factory_.GetNumberOfCreatedInstallTasks());
  const WebApp* web_app_t0 =
      fake_provider().registrar_unsafe().GetAppById(url_info.app_id());
  ASSERT_THAT(web_app_t0, IsNull());

  // Fast forward right before the retry should happen --> retry to process the
  // policy is still scheduled, but the isolated web app is not yet installed.
  iwa_installer_factory_.SetCommandBehavior(
      url_info.web_bundle_id().id(),
      /*execution_mode=*/
      MockIsolatedWebAppInstallCommandWrapper::ExecutionMode::kRunCommand,
      /*execute_immediately=*/true);
  task_environment()->FastForwardBy(base::TimeDelta(base::Seconds(58)));

  const WebApp* web_app_t1 =
      fake_provider().registrar_unsafe().GetAppById(url_info.app_id());
  ASSERT_THAT(web_app_t1, IsNull());

  WebAppTestInstallObserver install_observer(profile());
  install_observer.BeginListening({url_info.app_id()});

  // Fast forward another second and the app should be installed.
  task_environment()->FastForwardBy(base::TimeDelta(base::Seconds(1)));

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
      fake_provider().registrar_unsafe().GetAppById(url_info.app_id());
  ASSERT_THAT(web_app_t2, NotNull());
  EXPECT_THAT(web_app_t2->GetSources(),
              Eq(WebAppManagementTypes({WebAppManagement::Type::kIwaPolicy})));
}

TEST_F(IsolatedWebAppRetryTest, RetryTimeStepsCorrect) {
  const std::vector<std::pair<web_package::SignedWebBundleId, GURL>> apps = {
      {get_app1_id(), GURL(kUpdateManifestUrlApp1)},
      {get_app2_id(), GURL(kUpdateManifestUrlApp2)}};
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
  PolicyGenerator policy_generator;
  unsigned int expected_number_install_tasks = 1u;
  for (const auto& [app_id, update_manifest_url] : apps) {
    auto url_info = IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(app_id);
    iwa_installer_factory_.SetCommandBehavior(
        url_info.web_bundle_id().id(),
        /*execution_mode=*/
        MockIsolatedWebAppInstallCommandWrapper::ExecutionMode::
            kSimulateFailure,
        /*execute_immediately=*/true);

    policy_generator.AddForceInstalledIwa(url_info.web_bundle_id(),
                                          update_manifest_url);
    profile()->GetPrefs()->Set(prefs::kIsolatedWebAppInstallForceList,
                               policy_generator.Generate());

    for (size_t i = 0; i < desired_retry_time_steps_in_seconds.size() - 1;
         ++i) {
      const int& current_time_step = desired_retry_time_steps_in_seconds[i];
      const int& next_time_step = desired_retry_time_steps_in_seconds[i + 1];

      // Another (failed) attempt to install the isolated web app
      task_environment()->FastForwardBy(base::TimeDelta(base::Seconds(1)));

      ASSERT_EQ(expected_number_install_tasks,
                iwa_installer_factory_.GetNumberOfCreatedInstallTasks());
      const WebApp* web_app_t0 =
          fake_provider().registrar_unsafe().GetAppById(url_info.app_id());
      ASSERT_THAT(web_app_t0, IsNull());

      // Fast forward right before the retry should happen --> retry to process
      // the policy is still scheduled, but the install task is not yet created.
      task_environment()->FastForwardBy(base::TimeDelta(
          base::Seconds(next_time_step - current_time_step - 2)));

      const WebApp* web_app_t1 =
          fake_provider().registrar_unsafe().GetAppById(url_info.app_id());
      ASSERT_THAT(web_app_t1, IsNull());

      // Fast forward another second and the next retry should happen.
      task_environment()->FastForwardBy(base::TimeDelta(base::Seconds(1)));
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
    task_environment()->FastForwardBy(base::TimeDelta(base::Seconds(18000)));

    EXPECT_EQ(install_observer.Wait(), url_info.app_id());

    const WebApp* web_app =
        fake_provider().registrar_unsafe().GetAppById(url_info.app_id());
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
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(get_app1_id());
  auto url_info_2 =
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(get_app2_id());

  PolicyGenerator policy_generator;
  policy_generator.AddForceInstalledIwa(url_info_1.web_bundle_id(),
                                        GURL(kUpdateManifestUrlApp1));
  policy_generator.AddForceInstalledIwa(url_info_2.web_bundle_id(),
                                        GURL(kUpdateManifestUrlApp2));
  profile()->GetPrefs()->Set(prefs::kIsolatedWebAppInstallForceList,
                             policy_generator.Generate());

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
  task_environment()->FastForwardBy(base::TimeDelta(base::Seconds(1)));

  ASSERT_EQ(2u, iwa_installer_factory_.GetNumberOfCreatedInstallTasks());
  const WebApp* web_app1_t0 =
      fake_provider().registrar_unsafe().GetAppById(url_info_1.app_id());
  ASSERT_THAT(web_app1_t0, IsNull());
  const WebApp* web_app2_t0 =
      fake_provider().registrar_unsafe().GetAppById(url_info_2.app_id());
  ASSERT_THAT(web_app2_t0, IsNull());

  // Forward by 60 seconds. Because the second app was not completed yet, still
  // no retry should be scheduled.
  task_environment()->FastForwardBy(base::TimeDelta(base::Seconds(60)));
  ASSERT_EQ(2u, iwa_installer_factory_.GetNumberOfCreatedInstallTasks());

  ASSERT_TRUE(iwa_installer_factory_.GetLatestCommandWrapper(
      url_info_2.web_bundle_id().id()));
  ASSERT_FALSE(iwa_installer_factory_
                   .GetLatestCommandWrapper(url_info_2.web_bundle_id().id())
                   ->CommandWasScheduled());

  // Complete install task for the second app (which succeeds).
  WebAppTestInstallObserver app2_install_observer(profile());
  app2_install_observer.BeginListening({url_info_2.app_id()});

  task_environment()->GetMainThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &MockIsolatedWebAppInstallCommandWrapper::ScheduleCommand,
          base::Unretained(iwa_installer_factory_.GetLatestCommandWrapper(
              url_info_2.web_bundle_id().id()))));
  task_environment()->FastForwardBy(base::TimeDelta(base::Seconds(1)));

  EXPECT_EQ(app2_install_observer.Wait(), url_info_2.app_id());

  // The retry command for the first app should be successful. The second app
  // doesn't need a retry.
  iwa_installer_factory_.SetCommandBehavior(
      url_info_1.web_bundle_id().id(),
      /*execution_mode=*/
      MockIsolatedWebAppInstallCommandWrapper::ExecutionMode::kRunCommand,
      /*execute_immediately=*/true);
  task_environment()->FastForwardBy(base::TimeDelta(base::Seconds(1)));
  // The retry is scheduled, but the install task for the remaining app is not
  // yet created.
  ASSERT_EQ(2u, iwa_installer_factory_.GetNumberOfCreatedInstallTasks());

  // Forward to right before an additional install task for the first app is
  // scheduled.
  task_environment()->FastForwardBy(base::TimeDelta(base::Seconds(57)));
  ASSERT_EQ(2u, iwa_installer_factory_.GetNumberOfCreatedInstallTasks());

  WebAppTestInstallObserver app1_install_observer(profile());
  app1_install_observer.BeginListening({url_info_1.app_id()});

  // Moving the clock forward will finally install the second app.
  task_environment()->FastForwardBy(base::TimeDelta(base::Seconds(1)));
  ASSERT_EQ(3u, iwa_installer_factory_.GetNumberOfCreatedInstallTasks());

  EXPECT_EQ(app1_install_observer.Wait(), url_info_1.app_id());

  const WebApp* web_app1_t2 =
      fake_provider().registrar_unsafe().GetAppById(url_info_1.app_id());
  ASSERT_THAT(web_app1_t2, NotNull());
  EXPECT_THAT(web_app1_t2->GetSources(),
              Eq(WebAppManagementTypes({WebAppManagement::Type::kIwaPolicy})));
  const WebApp* web_app2_t2 =
      fake_provider().registrar_unsafe().GetAppById(url_info_2.app_id());
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
    fake_provider().SetScheduler(std::move(command_scheduler));
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
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(get_app1_id());
  auto url_info_2 =
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(get_app2_id());
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

  PolicyGenerator policy_generator;
  policy_generator.AddForceInstalledIwa(url_info_1.web_bundle_id(),
                                        GURL(kUpdateManifestUrlApp1));
  policy_generator.AddForceInstalledIwa(url_info_2.web_bundle_id(),
                                        GURL(kUpdateManifestUrlApp2));
  WebAppTestInstallObserver install_observer(profile());
  install_observer.BeginListening({url_info_1.app_id()});
  profile()->GetPrefs()->Set(prefs::kIsolatedWebAppInstallForceList,
                             policy_generator.Generate());
  EXPECT_EQ(install_observer.Wait(), url_info_1.app_id());

  ASSERT_TRUE(command_done_future_.Wait());
  EXPECT_EQ(2u, command_scheduler_->GetNumberOfCalls());
}

TEST_F(CleanupOrphanedBundlesTest, CleanUpNotCalledOnAllTasksSuccess) {
  ASSERT_TRUE(command_done_future_.Wait());
  EXPECT_EQ(1u, command_scheduler_->GetNumberOfCalls());
  command_done_future_.Clear();

  auto url_info_1 =
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(get_app1_id());
  auto url_info_2 =
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(get_app2_id());
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

  PolicyGenerator policy_generator;
  policy_generator.AddForceInstalledIwa(url_info_1.web_bundle_id(),
                                        GURL(kUpdateManifestUrlApp1));
  policy_generator.AddForceInstalledIwa(url_info_2.web_bundle_id(),
                                        GURL(kUpdateManifestUrlApp2));
  profile()->GetPrefs()->Set(prefs::kIsolatedWebAppInstallForceList,
                             policy_generator.Generate());

  const webapps::AppId last_installed_app_id = install_observer.Wait();
  task_environment()->RunUntilIdle();

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
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

 protected:
  int GetSimulatedPendingInstallCount() { return GetParam(); }

  webapps::AppId app_id_;

 private:
  // `IsolatedWebAppPolicyManagerTestBase`:
  void SetCommandScheduler() override {
    // For these tests we are fine with the regular command scheduler.
    const auto url_info_1 = IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
        web_app::TestSignedWebBundleBuilder::BuildDefault().id);
    app_id_ = url_info_1.app_id();

    PolicyGenerator policy_generator;
    policy_generator.AddForceInstalledIwa(url_info_1.web_bundle_id(),
                                          GURL(kUpdateManifestUrlApp1));
    profile()->GetPrefs()->Set(prefs::kIsolatedWebAppInstallForceList,
                               policy_generator.Generate());

    // Set the number of previous crashes on profile creation to simulate a
    // previously crashing device.
    profile()->GetPrefs()->SetInteger(
        prefs::kIsolatedWebAppPendingInitializationCount,
        GetSimulatedPendingInstallCount());
  }
};

TEST_P(IsolatedWebAppInstallEmergencyMechanismTest,
       EmergencyMechanismOnStartup) {
  // If the emergency mechanism is triggered, the install count is increaded by
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
  task_environment()->FastForwardBy(base::Seconds(1));

  // If we already tried twice, we delay the execution to allow for updates.
  if (GetSimulatedPendingInstallCount() > 2) {
    EXPECT_EQ(GetSimulatedPendingInstallCount() + 1,
              profile()->GetPrefs()->GetInteger(
                  prefs::kIsolatedWebAppPendingInitializationCount));
    EXPECT_EQ(0u, provider().registrar_unsafe().GetAppIds().size());

    // Forward until one second before the retry. The pending installation count
    // is still not reset.
    task_environment()->FastForwardBy(base::Hours(4) + base::Minutes(59) +
                                      base::Seconds(58));
    EXPECT_EQ(GetSimulatedPendingInstallCount() + 1,
              profile()->GetPrefs()->GetInteger(
                  prefs::kIsolatedWebAppPendingInitializationCount));
    EXPECT_EQ(0u, provider().registrar_unsafe().GetAppIds().size());

    // Forward by another second, which triggers the retry.
    task_environment()->FastForwardBy(base::Seconds(1));
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

}  // namespace web_app
