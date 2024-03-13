// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_policy_manager.h"

#include <memory>
#include <string>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/containers/to_vector.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_install_source.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_source.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_storage_location.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_external_install_options.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_policy_constants.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/iwa_test_server_configurator.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/policy_generator.h"
#include "chrome/browser/web_applications/isolated_web_apps/update_manifest/update_manifest.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/fake_web_contents_manager.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "components/user_manager/user.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {

namespace {

using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::Pair;
using ::testing::Property;

constexpr char kUpdateManifestUrl1[] =
    "https://example.com/1/update-manifest-1.json";
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

base::Value CreatePolicyEntry(base::StringPiece web_bundle_id,
                              base::StringPiece update_manifest_url) {
  base::Value::Dict policy_entry =
      base::Value::Dict()
          .Set(web_app::kPolicyWebBundleIdKey, web_bundle_id)
          .Set(web_app::kPolicyUpdateManifestUrlKey, update_manifest_url);
  return base::Value(std::move(policy_entry));
}

std::vector<IsolatedWebAppExternalInstallOptions> GenerateInstallOptions() {
  // App 1 represents the most general case: the Update Manifest has several
  // records. We should determine the latest version, download the appropreate
  // file and install the app. It is successful case.
  const base::Value policy_value_1 =
      CreatePolicyEntry(kWebBundleId1, kUpdateManifestUrl1);
  IsolatedWebAppExternalInstallOptions app_options_1 =
      IsolatedWebAppExternalInstallOptions::FromPolicyPrefValue(policy_value_1)
          .value();
  // App 2 is similar to App 1 but has only one record in the Update Manifest.
  const base::Value policy_value_2 =
      CreatePolicyEntry(kWebBundleId2, kUpdateManifestUrl2);
  IsolatedWebAppExternalInstallOptions app_options_2 =
      IsolatedWebAppExternalInstallOptions::FromPolicyPrefValue(policy_value_2)
          .value();
  // We can't download Update Manifest for the app 3.
  const base::Value policy_value_3 =
      CreatePolicyEntry(kWebBundleId3, kUpdateManifestUrl3);
  IsolatedWebAppExternalInstallOptions app_options_3 =
      IsolatedWebAppExternalInstallOptions::FromPolicyPrefValue(policy_value_3)
          .value();
  // App 4 represents the case where the Update Manifest if not parceable.
  const base::Value policy_value_4 =
      CreatePolicyEntry(kWebBundleId4, kUpdateManifestUrl4);
  IsolatedWebAppExternalInstallOptions app_options_4 =
      IsolatedWebAppExternalInstallOptions::FromPolicyPrefValue(policy_value_4)
          .value();
  // The Web Bundle URL of the App 5 is not valid.
  const base::Value policy_value_5 =
      CreatePolicyEntry(kWebBundleId5, kUpdateManifestUrl5);
  IsolatedWebAppExternalInstallOptions app_options_5 =
      IsolatedWebAppExternalInstallOptions::FromPolicyPrefValue(policy_value_5)
          .value();
  // The Web Bundle of the App 6 can't be installed.
  const base::Value policy_value_6 =
      CreatePolicyEntry(kWebBundleId6, kUpdateManifestUrl6);
  IsolatedWebAppExternalInstallOptions app_options_6 =
      IsolatedWebAppExternalInstallOptions::FromPolicyPrefValue(policy_value_6)
          .value();
  // The Web Bundle file of the App 7 can't be downloaded.
  const base::Value policy_value_7 =
      CreatePolicyEntry(kWebBundleId7, kUpdateManifestUrl7);
  IsolatedWebAppExternalInstallOptions app_options_7 =
      IsolatedWebAppExternalInstallOptions::FromPolicyPrefValue(policy_value_7)
          .value();

  std::vector<IsolatedWebAppExternalInstallOptions> options;
  options.push_back(std::move(app_options_1));
  options.push_back(std::move(app_options_2));
  options.push_back(std::move(app_options_3));
  options.push_back(std::move(app_options_4));
  options.push_back(std::move(app_options_5));
  options.push_back(std::move(app_options_6));
  options.push_back(std::move(app_options_7));

  return options;
}

class TestIwaInstallCommandWrapper
    : public internal::BulkIwaInstaller::IwaInstallCommandWrapper {
 public:
  TestIwaInstallCommandWrapper() = default;
  void Install(
      const IsolatedWebAppInstallSource& install_source,
      const IsolatedWebAppUrlInfo& url_info,
      const base::Version& expected_version,
      WebAppCommandScheduler::InstallIsolatedWebAppCallback callback) override {
    if (url_info.web_bundle_id().id() == kWebBundleId1 ||
        url_info.web_bundle_id().id() == kWebBundleId2) {
      if (url_info.web_bundle_id().id() == kWebBundleId1) {
        EXPECT_EQ(expected_version, base::Version("7.0.6"));
      } else if (url_info.web_bundle_id().id() == kWebBundleId2) {
        EXPECT_EQ(expected_version, base::Version("3.0.0"));
      }

      std::move(callback).Run(InstallIsolatedWebAppCommandSuccess(
          expected_version,
          IwaStorageOwnedBundle{"random_folder", /*dev_mode=*/false}));
      return;
    }

    std::move(callback).Run(base::unexpected{InstallIsolatedWebAppCommandError{
        .message = std::string{"Install error message"}}});
  }
  ~TestIwaInstallCommandWrapper() override = default;
};

}  // namespace

namespace internal {
class BulkIwaInstallerTest : public ::testing::Test {
 public:
  BulkIwaInstallerTest()
      : shared_url_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_factory_)) {}

 protected:
  using InstallResult = internal::BulkIwaInstallerResult;

  void SetUp() override {
    ASSERT_TRUE(dir_.CreateUniqueTempDir());
    AddJsonResponse(kUpdateManifestUrl1, kUpdateManifestValue1);
    AddJsonResponse(kUpdateManifestUrl2, kUpdateManifestValue2);
    test_factory_.AddResponse(kUpdateManifestUrl3, kUpdateManifestValue3,
                              net::HttpStatusCode::HTTP_NOT_FOUND);
    AddJsonResponse(kUpdateManifestUrl4, kUpdateManifestValue4);
    AddJsonResponse(kUpdateManifestUrl5, kUpdateManifestValue5);
    AddJsonResponse(kUpdateManifestUrl6, kUpdateManifestValue6);
    AddJsonResponse(kUpdateManifestUrl7, kUpdateManifestValue7);
    test_factory_.AddResponse("https://example.com/app1.swbn",
                              "Content of app1");
    test_factory_.AddResponse("https://example.com/app2.swbn",
                              "Content of app2");
    test_factory_.AddResponse("https://example.com/app6.swbn",
                              "Content of app6");
    test_factory_.AddResponse("https://example.com/app7.swbn", "",
                              net::HttpStatusCode::HTTP_NOT_FOUND);

    test_managed_guest_session_ =
        std::make_unique<profiles::testing::ScopedTestManagedGuestSession>();
  }

  void TearDown() override { test_factory_.ClearResponses(); }

  void AddJsonResponse(base::StringPiece url, base::StringPiece content) {
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
  const std::vector<IsolatedWebAppExternalInstallOptions> all_install_options_ =
      GenerateInstallOptions();
  std::unique_ptr<profiles::testing::ScopedTestManagedGuestSession>
      test_managed_guest_session_;
};

// This test case represents the regular flow of force installing IWA for
// ephemeral session. The install options will cover cases of success as well as
// legitimate failures.
TEST_F(BulkIwaInstallerTest, MgsRegularFlow) {
  auto expected_results = base::ToVector<
      std::vector<std::pair<std::string_view, InstallResult::Type>>>(
      {{kWebBundleId1, InstallResult::Type::kSuccess},
       {kWebBundleId2, InstallResult::Type::kSuccess},
       {kWebBundleId3, InstallResult::Type::kErrorUpdateManifestDownloadFailed},
       {kWebBundleId4, InstallResult::Type::kErrorUpdateManifestParsingFailed},
       {kWebBundleId5, InstallResult::Type::kErrorWebBundleUrlCantBeDetermined},
       {kWebBundleId6, InstallResult::Type::kErrorCantInstallFromWebBundle},
       {kWebBundleId7, InstallResult::Type::kErrorCantDownloadWebBundle}},
      [](const auto& item) {
        const auto& [id, type] = item;
        return Pair(Eq(*web_package::SignedWebBundleId::Create(id)),
                    Property("type", &InstallResult::type, Eq(type)));
      });

  base::test::TestFuture<std::vector<internal::BulkIwaInstaller::Result>>
      future;
  BulkIwaInstaller installer(
      dir_.GetPath(), all_install_options_, shared_url_loader_factory_,
      std::make_unique<TestIwaInstallCommandWrapper>(), future.GetCallback());
  installer.InstallEphemeralApps();

  EXPECT_THAT(future.Get(), ElementsAreArray(expected_results));

  const base::FilePath iwa_root_dir = dir_.GetPath().Append(
      internal::BulkIwaInstaller::kEphemeralIwaRootDirectory);
  EXPECT_TRUE(base::IsDirectoryEmpty(iwa_root_dir));
}

// If there is no MGS we don't create root directory for the IWAs.
TEST_F(BulkIwaInstallerTest, RegularUserDirectoryForIwaNotCreated) {
  test_managed_guest_session_.reset();

  auto expected_results = base::ToVector<
      std::vector<std::pair<std::string_view, InstallResult::Type>>>(
      {{kWebBundleId1, InstallResult::Type::kErrorNotEphemeralSession},
       {kWebBundleId2, InstallResult::Type::kErrorNotEphemeralSession},
       {kWebBundleId3, InstallResult::Type::kErrorNotEphemeralSession},
       {kWebBundleId4, InstallResult::Type::kErrorNotEphemeralSession},
       {kWebBundleId5, InstallResult::Type::kErrorNotEphemeralSession},
       {kWebBundleId6, InstallResult::Type::kErrorNotEphemeralSession},
       {kWebBundleId7, InstallResult::Type::kErrorNotEphemeralSession}},
      [](const auto& item) {
        const auto& [id, type] = item;
        return Pair(Eq(*web_package::SignedWebBundleId::Create(id)),
                    Property("type", &InstallResult::type, Eq(type)));
      });

  base::test::TestFuture<std::vector<internal::BulkIwaInstaller::Result>>
      future;
  BulkIwaInstaller installer(
      dir_.GetPath(), all_install_options_, shared_url_loader_factory_,
      std::make_unique<TestIwaInstallCommandWrapper>(), future.GetCallback());
  installer.InstallEphemeralApps();

  EXPECT_THAT(future.Get(), ElementsAreArray(expected_results));
  EXPECT_FALSE(base::DirectoryExists(dir_.GetPath().Append(
      internal::BulkIwaInstaller::kEphemeralIwaRootDirectory)));
}

// Empty install list should not lead to unexpected behavior.
TEST_F(BulkIwaInstallerTest, EmptyInstallList) {
  const std::vector<IsolatedWebAppExternalInstallOptions> empty_install_options;

  base::test::TestFuture<std::vector<internal::BulkIwaInstaller::Result>>
      future;
  BulkIwaInstaller installer(
      dir_.GetPath(), empty_install_options, shared_url_loader_factory_,
      std::make_unique<TestIwaInstallCommandWrapper>(), future.GetCallback());
  installer.InstallEphemeralApps();

  // No apps to install leads to zero install results.
  EXPECT_TRUE(future.Get().empty());
}
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
  IsolatedWebAppPolicyManagerTestBase()
      : WebAppTest(WebAppTest::WithTestUrlLoaderFactory()) {}

  void SetUpServedIwas() {
    web_app::TestSignedWebBundle swbn_app1 =
        web_app::TestSignedWebBundleBuilder::BuildDefault();
    web_app::TestSignedWebBundle swbn_app2 =
        web_app::TestSignedWebBundleBuilder::BuildDefault(
            TestSignedWebBundleBuilder::BuildOptions().SetKeyPair(
                web_package::WebBundleSigner::KeyPair::CreateRandom()));

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

    test_managed_guest_session_ =
        std::make_unique<profiles::testing::ScopedTestManagedGuestSession>();
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

  web_package::SignedWebBundleId get_app1_id() { return lazy_app1_id_.value(); }
  web_package::SignedWebBundleId get_app2_id() { return lazy_app2_id_.value(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<profiles::testing::ScopedTestManagedGuestSession>
      test_managed_guest_session_;
  data_decoder::test::InProcessDataDecoder data_decoder_;

  std::optional<web_package::SignedWebBundleId> lazy_app1_id_;
  std::optional<web_package::SignedWebBundleId> lazy_app2_id_;
};

class IsolatedWebAppPolicyManagerTest
    : public IsolatedWebAppPolicyManagerTestBase {
  void SetCommandScheduler() override {
    // For these tests we are fine with regular command scheduler.
  }
};

TEST_F(IsolatedWebAppPolicyManagerTest, AppInstalled) {
  PolicyGenerator policy_generator;
  policy_generator.AddForceInstalledIwa(get_app1_id(),
                                        GURL(kUpdateManifestUrlApp1));

  profile()->GetPrefs()->Set(prefs::kIsolatedWebAppInstallForceList,
                             policy_generator.Generate());

  task_environment()->RunUntilIdle();

  AssertAppInstalled(get_app1_id());
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
    CHECK(stashed_callback_.is_null());
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
  T* get_command_scheduler() { return scheduler_; }

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

  // Update the policy at the moment when first pilicy update is being
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
  EXPECT_EQ(get_command_scheduler()->FinishWithError(), get_app1_id());
  task_environment()->RunUntilIdle();

  // The second policy update is ignored as it was replaced by the third one.

  // Processing the third policy update.
  EXPECT_EQ(get_command_scheduler()->FinishWithError(), get_app1_id());
  task_environment()->RunUntilIdle();
  // Finish app2 from the third policy update.
  EXPECT_EQ(get_command_scheduler()->FinishWithError(), get_app2_id());
  task_environment()->RunUntilIdle();
}

// This scheduler is intercepting scheduling of the uninstall command,
// verifying if the App ID is expected for removal and returns the
// uninstall error.
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
    EXPECT_EQ(management_type, WebAppManagement::Type::kCommandLine);
    EXPECT_EQ(uninstall_source,
              webapps::WebappUninstallSource::kIwaEnterprisePolicy);
    auto app = expected_apps_to_remove_.find(app_id);
    expected_apps_to_remove_.erase(app);
    std::move(callback).Run(webapps::UninstallResultCode ::kError);
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
    PolicyGenerator empty_policy;

    const webapps::AppId app1_id =
        IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(get_app1_id())
            .app_id();
    const webapps::AppId app2_id =
        IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(get_app2_id())
            .app_id();
    get_command_scheduler()->AddExpectedToUninstallApp(app1_id);
    get_command_scheduler()->AddExpectedToUninstallApp(app2_id);
    EXPECT_EQ(get_command_scheduler()->GetNumberOfAppsRemainingToUninstall(),
              2U);

    profile()->GetPrefs()->Set(prefs::kIsolatedWebAppInstallForceList,
                               empty_policy.Generate());

    task_environment()->RunUntilIdle();

    EXPECT_EQ(get_command_scheduler()->GetNumberOfAppsRemainingToUninstall(),
              0U);
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

}  // namespace web_app
