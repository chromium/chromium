// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_policy_manager.h"

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_external_install_options.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_policy_constants.h"
#include "chrome/browser/web_applications/isolation_data.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/login/login_state/login_state.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/startup/browser_init_params.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace web_app {

namespace {

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
constexpr char kUpdateManifestUrl8[] =
    "https://example.com/8/update-manifest-8.json";

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
constexpr char kUpdateManifestValue5[] = R"(
    {"versions":
    [{"version": "1.0.0", "src": "Ooops! Wrong Web Bundle URL!"}]})";
constexpr char kUpdateManifestValue6[] = R"(
    {"versions":
    [{"version": "1.0.0", "src": "https://example.com/app6.swbn"}]})";
constexpr char kUpdateManifestValue7[] = R"(
    {"versions":
    [{"version": "1.0.0", "src": "https://example.com/app7.swbn"}]})";
constexpr char kUpdateManifestValue8[] = R"(
    {"versions":
    [{"version": "1.0.0","src": "https://example.com/app8.swbn"}]})";

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
constexpr base::StringPiece kWebBundleId6 = kWebBundleId1;
constexpr char kWebBundleId7[] =
    "gerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic";
constexpr char kWebBundleId8[] =
    "herugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic";

base::Value CreatePolicyEntry(base::StringPiece web_bundle_id,
                              base::StringPiece update_manifest_url) {
  base::Value policy_entry(base::Value::Type::DICT);
  policy_entry.SetStringKey(web_app::kPolicyWebBundleIdKey, web_bundle_id);
  policy_entry.SetStringKey(web_app::kPolicyUpdateManifestUrlKey,
                            update_manifest_url);
  return policy_entry;
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
  // ID of the App 6 is the same as ID of the App 1.
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
  // The Web Bundle of the App 8 can't be installed.
  const base::Value policy_value_8 =
      CreatePolicyEntry(kWebBundleId8, kUpdateManifestUrl8);
  IsolatedWebAppExternalInstallOptions app_options_8 =
      IsolatedWebAppExternalInstallOptions::FromPolicyPrefValue(policy_value_8)
          .value();

  std::vector<IsolatedWebAppExternalInstallOptions> options;
  options.push_back(std::move(app_options_1));
  options.push_back(std::move(app_options_2));
  options.push_back(std::move(app_options_3));
  options.push_back(std::move(app_options_4));
  options.push_back(std::move(app_options_5));
  options.push_back(std::move(app_options_6));
  options.push_back(std::move(app_options_7));
  options.push_back(std::move(app_options_8));
  return options;
}

void StartManagedGuestSession() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  auto init_params = crosapi::mojom::BrowserInitParams::New();
  init_params->session_type = crosapi::mojom::SessionType::kPublicSession;
  chromeos::BrowserInitParams::SetInitParamsForTests(std::move(init_params));
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::LoginState::Initialize();
  ash::LoginState::Get()->SetLoggedInState(
      ash::LoginState::LOGGED_IN_ACTIVE,
      ash::LoginState::LOGGED_IN_USER_PUBLIC_ACCOUNT);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

void ShutdownManagedGuestSession() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  auto init_params = crosapi::mojom::BrowserInitParams::New();
  chromeos::BrowserInitParams::SetInitParamsForTests(std::move(init_params));
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (ash::LoginState::IsInitialized()) {
    ash::LoginState::Shutdown();
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

class TestIwaInstallCommandWrapper
    : public IsolatedWebAppPolicyManager::IwaInstallCommandWrapper {
 public:
  TestIwaInstallCommandWrapper() = default;
  void Install(
      const IsolationData& isolation_data,
      const IsolatedWebAppUrlInfo& isolation_info,
      WebAppCommandScheduler::InstallIsolatedWebAppCallback callback) override {
    if (isolation_info.web_bundle_id().id() == kWebBundleId1 ||
        isolation_info.web_bundle_id().id() == kWebBundleId2) {
      std::move(callback).Run(InstallIsolatedWebAppCommandSuccess{});
      return;
    }

    std::move(callback).Run(base::unexpected{InstallIsolatedWebAppCommandError{
        .message = std::string{"Install error message"}}});
  }
  ~TestIwaInstallCommandWrapper() override = default;
};

}  // namespace

class IsolatedWebAppPolicyManagerTest : public ::testing::Test {
 public:
  IsolatedWebAppPolicyManagerTest()
      : shared_url_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_factory_)) {}

 protected:
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
    AddJsonResponse(kUpdateManifestUrl8, kUpdateManifestValue8);
    test_factory_.AddResponse("https://example.com/app1.swbn",
                              "Content of app1");
    test_factory_.AddResponse("https://example.com/app2.swbn",
                              "Content of app2");
    test_factory_.AddResponse("https://example.com/app7.swbn", "",
                              net::HttpStatusCode::HTTP_NOT_FOUND);
    test_factory_.AddResponse("https://example.com/app8.swbn",
                              "Content of app8");
    StartManagedGuestSession();
  }

  void TearDown() override {
    ShutdownManagedGuestSession();
    test_factory_.ClearResponses();
  }

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
};

// This test case represents the regular flow of force installing IWA for
// ephemeral session. The install options will cover cases of success as well as
// legitimate failures.
TEST_F(IsolatedWebAppPolicyManagerTest, MgsRegularFlow) {
  auto expected_results =
      std::vector<IsolatedWebAppPolicyManager::EphemeralAppInstallResult>(
          all_install_options_.size());

  expected_results.at(0) =
      IsolatedWebAppPolicyManager::EphemeralAppInstallResult::kSuccess;
  expected_results.at(1) =
      IsolatedWebAppPolicyManager::EphemeralAppInstallResult::kSuccess;
  expected_results.at(2) = IsolatedWebAppPolicyManager::
      EphemeralAppInstallResult::kErrorUpdateManifestDownloadFailed;
  expected_results.at(3) = IsolatedWebAppPolicyManager::
      EphemeralAppInstallResult::kErrorUpdateManifestParsingFailed;
  expected_results.at(4) = IsolatedWebAppPolicyManager::
      EphemeralAppInstallResult::kErrorWebBundleUrlCantBeDetermined;
  expected_results.at(5) = IsolatedWebAppPolicyManager::
      EphemeralAppInstallResult::kErrorCantCreateIwaDirectory;
  expected_results.at(6) = IsolatedWebAppPolicyManager::
      EphemeralAppInstallResult::kErrorCantDownloadWebBundle;
  expected_results.at(7) = IsolatedWebAppPolicyManager::
      EphemeralAppInstallResult::kErrorCantInstallFromWebBundle;
  base::test::TestFuture<
      std::vector<IsolatedWebAppPolicyManager::EphemeralAppInstallResult>>
      future;
  IsolatedWebAppPolicyManager manager(
      dir_.GetPath(), all_install_options_, shared_url_loader_factory_,
      std::make_unique<TestIwaInstallCommandWrapper>(), future.GetCallback());
  manager.InstallEphemeralApps();

  EXPECT_EQ(future.Get(), expected_results);

  const base::FilePath iwa_root_dir = dir_.GetPath().Append(
      IsolatedWebAppPolicyManager::kEphemeralIwaRootDirectory);
  ASSERT_TRUE(base::DirectoryExists(iwa_root_dir));

  // There should be 2 directories that represent successfully installed apps.
  base::FileEnumerator iter(
      iwa_root_dir, /*recursive=*/false,
      base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES);
  int counter = 0;
  while (!iter.Next().empty()) {
    EXPECT_TRUE(iter.GetInfo().IsDirectory());
    ++counter;
  }
  EXPECT_EQ(counter, 2);

  EXPECT_TRUE(base::PathExists(
      iwa_root_dir.Append(kWebBundleId1)
          .Append(IsolatedWebAppPolicyManager::kMainSignedWebBundleFileName)));
  EXPECT_TRUE(base::PathExists(
      iwa_root_dir.Append(kWebBundleId2)
          .Append(IsolatedWebAppPolicyManager::kMainSignedWebBundleFileName)));
}

// If there is no MGS we don't create root directory for the IWAs.
TEST_F(IsolatedWebAppPolicyManagerTest, RegularUserDirectoryForIwaNotCreated) {
  ShutdownManagedGuestSession();
  auto expected_results =
      std::vector<IsolatedWebAppPolicyManager::EphemeralAppInstallResult>(
          all_install_options_.size(),
          IsolatedWebAppPolicyManager::EphemeralAppInstallResult::
              kErrorNotEphemeralSession);
  base::test::TestFuture<
      std::vector<IsolatedWebAppPolicyManager::EphemeralAppInstallResult>>
      future;
  IsolatedWebAppPolicyManager manager(
      dir_.GetPath(), all_install_options_, shared_url_loader_factory_,
      std::make_unique<TestIwaInstallCommandWrapper>(), future.GetCallback());
  manager.InstallEphemeralApps();

  EXPECT_EQ(future.Get(), expected_results);
  EXPECT_FALSE(base::DirectoryExists(dir_.GetPath().Append(
      IsolatedWebAppPolicyManager::kEphemeralIwaRootDirectory)));
}

// Return error if the root directory exists.
TEST_F(IsolatedWebAppPolicyManagerTest, RootDirectoryExists) {
  base::CreateDirectory(dir_.GetPath().Append(
      IsolatedWebAppPolicyManager::kEphemeralIwaRootDirectory));

  auto expected_results =
      std::vector<IsolatedWebAppPolicyManager::EphemeralAppInstallResult>(
          all_install_options_.size(),
          IsolatedWebAppPolicyManager::EphemeralAppInstallResult::
              kErrorCantCreateRootDirectory);

  base::test::TestFuture<
      std::vector<IsolatedWebAppPolicyManager::EphemeralAppInstallResult>>
      future;
  IsolatedWebAppPolicyManager manager(
      dir_.GetPath(), all_install_options_, shared_url_loader_factory_,
      std::make_unique<TestIwaInstallCommandWrapper>(), future.GetCallback());
  manager.InstallEphemeralApps();

  EXPECT_EQ(future.Get(), expected_results);
}

// Empty install list should not lead to unexpected behavior.
TEST_F(IsolatedWebAppPolicyManagerTest, EmptyInstallList) {
  const std::vector<IsolatedWebAppExternalInstallOptions> empty_install_options;

  base::test::TestFuture<
      std::vector<IsolatedWebAppPolicyManager::EphemeralAppInstallResult>>
      future;
  IsolatedWebAppPolicyManager manager(
      dir_.GetPath(), empty_install_options, shared_url_loader_factory_,
      std::make_unique<TestIwaInstallCommandWrapper>(), future.GetCallback());
  manager.InstallEphemeralApps();

  // No apps to install leads to zero install results.
  EXPECT_TRUE(future.Get().empty());
}

TEST(IsolatedWebAppPolicyManagerStaticFunctionsTest,
     ExtractWebBundleURLErrorTest) {
  {
    // Providing a non-dictionary value should not be handled correctly.
    const base::Value string_value("A string value");
    EXPECT_FALSE(string_value.is_dict());
    EXPECT_FALSE(IsolatedWebAppPolicyManager::ExtractWebBundleURL(string_value)
                     .has_value());
  }

  {
    // Empty dictionary should be handled correctly as well.
    base::Value::Dict empty_dict;
    EXPECT_FALSE(IsolatedWebAppPolicyManager::ExtractWebBundleURL(
                     base::Value(std::move(empty_dict)))
                     .has_value());
  }

  {
    // Dictionary contains string instead of list.
    base::Value::Dict dict;
    dict.Set(kUpdateManifestAllVersionsKey,
             "Instead of this string we expect a base::Value::List here");
    EXPECT_FALSE(IsolatedWebAppPolicyManager::ExtractWebBundleURL(
                     base::Value(std::move(dict)))
                     .has_value());
  }

  {
    // Dictionary with empty version records.
    base::Value::List apps;
    base::Value::Dict dict;
    dict.Set(kUpdateManifestAllVersionsKey, std::move(apps));
    EXPECT_FALSE(IsolatedWebAppPolicyManager::ExtractWebBundleURL(
                     base::Value(std::move(dict)))
                     .has_value());
  }

  {
    // Dictionary with empty random strings instead of the version/URL
    // dictionary.
    base::Value::List apps;
    apps.Append("aaa");
    apps.Append("bbb");
    base::Value::Dict dict;
    dict.Set(kUpdateManifestAllVersionsKey, std::move(apps));
    EXPECT_FALSE(IsolatedWebAppPolicyManager::ExtractWebBundleURL(
                     base::Value(std::move(dict)))
                     .has_value());
  }

  {
    // There is no version.
    base::Value::List apps;

    base::Value::Dict no_version_record;
    no_version_record.Set(kUpdateManifestSrcKey,
                          "https://example.com/a/b.json");
    apps.Append(std::move(no_version_record));

    base::Value::Dict dict;
    dict.Set(kUpdateManifestAllVersionsKey, std::move(apps));
    EXPECT_FALSE(IsolatedWebAppPolicyManager::ExtractWebBundleURL(
                     base::Value(std::move(dict)))
                     .has_value());
  }

  {
    // There is no Web bundle URL.
    base::Value::List apps;

    base::Value::Dict no_web_bundle_url;
    no_web_bundle_url.Set(kUpdateManifestVersionKey, "1.0.0");
    apps.Append(std::move(no_web_bundle_url));

    base::Value::Dict dict;
    dict.Set(kUpdateManifestAllVersionsKey, std::move(apps));

    EXPECT_FALSE(IsolatedWebAppPolicyManager::ExtractWebBundleURL(
                     base::Value(std::move(dict)))
                     .has_value());
  }

  {
    // Version is not parseble.
    base::Value::List apps;

    base::Value::Dict invalid_version_record;
    invalid_version_record.Set(kUpdateManifestVersionKey,
                               "It is not a correct version");
    invalid_version_record.Set(kUpdateManifestSrcKey,
                               "https://example.com/a/b.json");
    apps.Append(std::move(invalid_version_record));

    base::Value::Dict dict;
    dict.Set(kUpdateManifestAllVersionsKey, std::move(apps));
    EXPECT_FALSE(IsolatedWebAppPolicyManager::ExtractWebBundleURL(
                     base::Value(std::move(dict)))
                     .has_value());
  }

  {
    // Web bundle URL is not parsable.
    base::Value::List apps;

    base::Value::Dict invalid_web_bundle_url;
    invalid_web_bundle_url.Set(kUpdateManifestVersionKey, "1.0.0");
    invalid_web_bundle_url.Set(kUpdateManifestSrcKey, "It is not a valid URL");
    apps.Append(std::move(invalid_web_bundle_url));

    base::Value::Dict dict;
    dict.Set(kUpdateManifestAllVersionsKey, std::move(apps));

    EXPECT_FALSE(IsolatedWebAppPolicyManager::ExtractWebBundleURL(
                     base::Value(std::move(dict)))
                     .has_value());
  }

  {
    // If at least one version is not parsable return nullptr.
    base::Value::List apps;

    base::Value::Dict ok_app;
    ok_app.Set(kUpdateManifestVersionKey, "1.0.0");
    ok_app.Set(kUpdateManifestSrcKey, "http://example.com/a/b.json");
    apps.Append(std::move(ok_app));

    base::Value::Dict ok_app_1;
    ok_app_1.Set(kUpdateManifestVersionKey, "2.0.0");
    ok_app_1.Set(kUpdateManifestSrcKey, "http://example.com/a/b.json");
    apps.Append(std::move(ok_app_1));

    base::Value::Dict invalid_version_record;
    invalid_version_record.Set(kUpdateManifestVersionKey,
                               "It is not a correct version");
    invalid_version_record.Set(kUpdateManifestSrcKey,
                               "https://example.com/a/b.json");
    apps.Append(std::move(invalid_version_record));

    base::Value::Dict dict;
    dict.Set(kUpdateManifestAllVersionsKey, std::move(apps));

    EXPECT_FALSE(IsolatedWebAppPolicyManager::ExtractWebBundleURL(
                     base::Value(std::move(dict)))
                     .has_value());
  }

  {
    // Unparsable URL of the latest app version leads to return of nullptr.
    base::Value::List apps;

    base::Value::Dict ok_app;
    ok_app.Set(kUpdateManifestVersionKey, "1.0.0");
    ok_app.Set(kUpdateManifestSrcKey, "http://example.com/a/b.json");
    apps.Append(std::move(ok_app));

    base::Value::Dict invalid_web_bundle_url;
    invalid_web_bundle_url.Set(kUpdateManifestVersionKey, "2.0.0");
    invalid_web_bundle_url.Set(kUpdateManifestSrcKey, "It is not a valid URL");
    apps.Append(std::move(invalid_web_bundle_url));

    base::Value::Dict dict;
    dict.Set(kUpdateManifestAllVersionsKey, std::move(apps));

    EXPECT_FALSE(IsolatedWebAppPolicyManager::ExtractWebBundleURL(
                     base::Value(std::move(dict)))
                     .has_value());
  }

  {
    // Two equal versions in the one update manifest are not acceptable.
    base::Value::List apps;

    base::Value::Dict ok_app;
    ok_app.Set(kUpdateManifestVersionKey, "1.0.0");
    ok_app.Set(kUpdateManifestSrcKey, "http://example.com/v100.json");
    apps.Append(std::move(ok_app));

    base::Value::Dict ok_app_1;
    ok_app_1.Set(kUpdateManifestVersionKey, "1.0.0");
    ok_app_1.Set(kUpdateManifestSrcKey, "http://example.com/xyz.json");
    apps.Append(std::move(ok_app_1));

    base::Value::Dict dict;
    dict.Set(kUpdateManifestAllVersionsKey, std::move(apps));

    EXPECT_FALSE(IsolatedWebAppPolicyManager::ExtractWebBundleURL(
                     base::Value(std::move(dict)))
                     .has_value());
  }
}

TEST(IsolatedWebAppPolicyManagerStaticFunctionsTest,
     ExtractWebBundleURLSuccessTest) {
  {
    // One app case.
    base::Value::List apps;

    base::Value::Dict ok_app;
    ok_app.Set(kUpdateManifestVersionKey, "1.0.1");
    ok_app.Set(kUpdateManifestSrcKey, "http://example.com/v101.json");
    apps.Append(std::move(ok_app));

    base::Value::Dict dict;
    dict.Set(kUpdateManifestAllVersionsKey, std::move(apps));

    auto result = IsolatedWebAppPolicyManager::ExtractWebBundleURL(
        base::Value(std::move(dict)));
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "http://example.com/v101.json");
  }

  {
    // Several apps use case.
    base::Value::List apps;

    base::Value::Dict ok_app;
    ok_app.Set(kUpdateManifestVersionKey, "1.0.0");
    ok_app.Set(kUpdateManifestSrcKey, "http://example.com/v100.json");
    apps.Append(std::move(ok_app));

    base::Value::Dict ok_app_1;
    ok_app_1.Set(kUpdateManifestVersionKey, "2.0.0");
    ok_app_1.Set(kUpdateManifestSrcKey, "http://example.com/v200.json");
    apps.Append(std::move(ok_app_1));

    base::Value::Dict dict;
    dict.Set(kUpdateManifestAllVersionsKey, std::move(apps));

    auto result = IsolatedWebAppPolicyManager::ExtractWebBundleURL(
        base::Value(std::move(dict)));
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "http://example.com/v200.json");
  }

  {
    // The invalid URL of the stale app version doesn't affect result.
    base::Value::List apps;

    base::Value::Dict ok_app;
    ok_app.Set(kUpdateManifestVersionKey, "1.0.0");
    ok_app.Set(kUpdateManifestSrcKey, "http://example.com/v100.json");
    apps.Append(std::move(ok_app));

    base::Value::Dict ok_app_1;
    ok_app_1.Set(kUpdateManifestVersionKey, "2.0.0");
    ok_app_1.Set(kUpdateManifestSrcKey, "http://example.com/v200.json");
    apps.Append(std::move(ok_app_1));

    base::Value::Dict invalid_web_bundle_url;
    invalid_web_bundle_url.Set(kUpdateManifestVersionKey, "1.4.0");
    invalid_web_bundle_url.Set(kUpdateManifestSrcKey, "It is not a valid URL");
    apps.Append(std::move(invalid_web_bundle_url));

    base::Value::Dict dict;
    dict.Set(kUpdateManifestAllVersionsKey, std::move(apps));

    auto result = IsolatedWebAppPolicyManager::ExtractWebBundleURL(
        base::Value(std::move(dict)));
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "http://example.com/v200.json");
  }

  {
    // We don't mind the update manifest has other fields.
    base::Value::List apps;

    base::Value::Dict ok_app;
    ok_app.Set(kUpdateManifestVersionKey, "1.0.1");
    ok_app.Set(kUpdateManifestSrcKey, "http://example.com/v101.json");
    ok_app.Set("comment", "This is app v1.0.1");
    apps.Append(std::move(ok_app));

    base::Value::Dict dict;
    dict.Set(kUpdateManifestAllVersionsKey, std::move(apps));

    auto result = IsolatedWebAppPolicyManager::ExtractWebBundleURL(
        base::Value(std::move(dict)));
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "http://example.com/v101.json");
  }
}

}  // namespace web_app
