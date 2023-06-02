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
#include "base/version.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_external_install_options.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_policy_constants.h"
#include "chrome/browser/web_applications/isolated_web_apps/update_manifest/update_manifest.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

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
// This manifest contains an invalid `src` URL.
constexpr char kUpdateManifestValue5[] = R"(
    {"versions":
    [{"version": "1.0.0", "src": "chrome-extension://app5.wbn"}]})";
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

class TestIwaInstallCommandWrapper
    : public IsolatedWebAppPolicyManager::IwaInstallCommandWrapper {
 public:
  TestIwaInstallCommandWrapper() = default;
  void Install(
      const IsolatedWebAppLocation& location,
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
  test_managed_guest_session_.reset();
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

}  // namespace web_app
