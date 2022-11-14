// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_policy_manager.h"

#include <string>
#include <vector>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_external_install_options.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_policy_constants.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/login/login_state/login_state.h"
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

constexpr char kUpdateManifestValue1[] = R"(
    {"versions":[
      {"version": "1.0.0", "src": "https://example.com/1/p1.swbn"},
      {"version": "7.0.6", "src": "http://example.com/1/p7.wbn"}]
    })";
constexpr char kUpdateManifestValue2[] = R"(
    {"versions":
    [{"version": "3.0.0","src": "https://example.com/2/p3.swbn"}]})";
constexpr char kUpdateManifestValue3[] =
    "This update manifest should return error 404";
constexpr char kUpdateManifestValue4[] = R"(This is not JSON)";

constexpr char kWebBundleId1[] =
    "aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic";
constexpr char kWebBundleId2[] =
    "berugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic";
constexpr char kWebBundleId3[] =
    "cerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic";
constexpr char kWebBundleId4[] =
    "derugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic";

base::Value CreatePolicyEntry(base::StringPiece web_bundle_id,
                              base::StringPiece update_manifest_url) {
  base::Value policy_entry(base::Value::Type::DICT);
  policy_entry.SetStringKey(web_app::kWebBundleIdKey, web_bundle_id);
  policy_entry.SetStringKey(web_app::kUpdateManifestUrlKey,
                            update_manifest_url);
  return policy_entry;
}

std::vector<IsolatedWebAppExternalInstallOptions> GenerateInstallOptions() {
  const base::Value policy_value_1 =
      CreatePolicyEntry(kWebBundleId1, kUpdateManifestUrl1);
  IsolatedWebAppExternalInstallOptions app_options_1 =
      IsolatedWebAppExternalInstallOptions::FromPolicyPrefValue(policy_value_1)
          .value();
  const base::Value policy_value_2 =
      CreatePolicyEntry(kWebBundleId2, kUpdateManifestUrl2);
  IsolatedWebAppExternalInstallOptions app_options_2 =
      IsolatedWebAppExternalInstallOptions::FromPolicyPrefValue(policy_value_2)
          .value();
  const base::Value policy_value_3 =
      CreatePolicyEntry(kWebBundleId3, kUpdateManifestUrl3);
  IsolatedWebAppExternalInstallOptions app_options_3 =
      IsolatedWebAppExternalInstallOptions::FromPolicyPrefValue(policy_value_3)
          .value();
  const base::Value policy_value_4 =
      CreatePolicyEntry(kWebBundleId4, kUpdateManifestUrl4);
  IsolatedWebAppExternalInstallOptions app_options_4 =
      IsolatedWebAppExternalInstallOptions::FromPolicyPrefValue(policy_value_4)
          .value();

  std::vector<IsolatedWebAppExternalInstallOptions> options;
  options.push_back(std::move(app_options_1));
  options.push_back(std::move(app_options_2));
  options.push_back(std::move(app_options_3));
  options.push_back(std::move(app_options_4));
  return options;
}

void StartManagedGuestSession() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  auto init_params = crosapi::mojom::BrowserInitParams::New();
  init_params->session_type = crosapi::mojom::SessionType::kPublicSession;
  chromeos::BrowserInitParams::SetInitParamsForTests(std::move(init_params));
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
  chromeos::LoginState::Initialize();
  chromeos::LoginState::Get()->SetLoggedInState(
      chromeos::LoginState::LOGGED_IN_ACTIVE,
      chromeos::LoginState::LOGGED_IN_USER_PUBLIC_ACCOUNT);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

void ShutdownManagedGuestSession() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  auto init_params = crosapi::mojom::BrowserInitParams::New();
  chromeos::BrowserInitParams::SetInitParamsForTests(std::move(init_params));
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (chromeos::LoginState::IsInitialized())
    chromeos::LoginState::Shutdown();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

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
  base::test::TestFuture<
      std::vector<IsolatedWebAppPolicyManager::EphemeralAppInstallResult>>
      future;
  IsolatedWebAppPolicyManager manager(dir_.GetPath(), all_install_options_,
                                      shared_url_loader_factory_,
                                      future.GetCallback());
  manager.InstallEphemeralApps();

  EXPECT_EQ(future.Get(), expected_results);
  EXPECT_TRUE(base::DirectoryExists(dir_.GetPath().Append(
      IsolatedWebAppPolicyManager::kEphemeralIwaRootDirectory)));
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
  IsolatedWebAppPolicyManager manager(dir_.GetPath(), all_install_options_,
                                      shared_url_loader_factory_,
                                      future.GetCallback());
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
  IsolatedWebAppPolicyManager manager(dir_.GetPath(), all_install_options_,
                                      shared_url_loader_factory_,
                                      future.GetCallback());
  manager.InstallEphemeralApps();

  EXPECT_EQ(future.Get(), expected_results);
}

// Empty install list should not lead to unexpected behavior.
TEST_F(IsolatedWebAppPolicyManagerTest, EmptyInstallList) {
  const std::vector<IsolatedWebAppExternalInstallOptions> empty_install_options;

  base::test::TestFuture<
      std::vector<IsolatedWebAppPolicyManager::EphemeralAppInstallResult>>
      future;
  IsolatedWebAppPolicyManager manager(dir_.GetPath(), empty_install_options,
                                      shared_url_loader_factory_,
                                      future.GetCallback());
  manager.InstallEphemeralApps();

  // No apps to install leads to zero install results.
  EXPECT_TRUE(future.Get().empty());
}

}  // namespace web_app
