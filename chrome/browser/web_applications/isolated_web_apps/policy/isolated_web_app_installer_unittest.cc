// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_installer.h"

#include "base/files/scoped_temp_dir.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/web_applications/isolated_web_apps/commands/install_isolated_web_app_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_external_install_options.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/policy_test_utils.h"
#include "chrome/common/chrome_features.h"
#include "content/public/common/content_features.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {

namespace {

using testing::_;
using ::testing::Eq;
using ::testing::Invoke;
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
constexpr char kUpdateManifestUrl8[] =
    "https://example.com/1/update-manifest-8.json";
constexpr char kUpdateManifestUrl9[] =
    "https://example.com/1/update-manifest-9.json";

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
constexpr char kUpdateManifestValue8[] = R"(
    {"versions":
      [{"version": "1.0.0", "src": "https://example.com/not-used.swbn"},
      {"version": "7.0.6", "src": "https://example.com/app1.swbn"},
      {"version": "7.0.8", "src": "https://example.com/app8.swbn", "channels":["beta"]}]})";
constexpr char kUpdateManifestValue9[] = R"(
    {"versions":
      [{"version": "1.0.0", "src": "https://example.com/not-used.swbn"},
      {"version": "7.0.6", "src": "https://example.com/app1.swbn"},
      {"version": "7.0.8", "src": "https://example.com/app8.swbn", "channels":["beta"]},
      {"version": "6.0.1", "src": "https://example.com/app9.swbn"}]})";

constexpr char kWebBundleId1[] =
    "aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic";
constexpr char kWebBundleId2[] =
    "berugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic";

#if BUILDFLAG(IS_CHROMEOS)
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
#endif  // BUILDFLAG(IS_CHROMEOS)

constexpr char kWebBundleId8[] =
    "ierugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic";
constexpr char kWebBundleId9[] =
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

void HandleInstallBasedOnId(
    const IsolatedWebAppInstallSource& install_source,
    const IsolatedWebAppUrlInfo& url_info,
    const base::Version& expected_version,
    WebAppCommandScheduler::InstallIsolatedWebAppCallback callback) {
  if (url_info.web_bundle_id().id() == kWebBundleId1 ||
      url_info.web_bundle_id().id() == kWebBundleId2 ||
      url_info.web_bundle_id().id() == kWebBundleId8 ||
      url_info.web_bundle_id().id() == kWebBundleId9) {
    if (url_info.web_bundle_id().id() == kWebBundleId1) {
      EXPECT_EQ(expected_version, base::Version("7.0.6"));
    } else if (url_info.web_bundle_id().id() == kWebBundleId2) {
      EXPECT_EQ(expected_version, base::Version("3.0.0"));
    } else if (url_info.web_bundle_id().id() == kWebBundleId8) {
      EXPECT_EQ(expected_version, base::Version("7.0.8"));
    } else if (url_info.web_bundle_id().id() == kWebBundleId9) {
      EXPECT_EQ(expected_version, base::Version("6.0.1"));
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

struct IwaInstallerTestParam {
  bool is_mgs_install_enabled;
  bool is_cache_enabled = false;
  bool is_user_session;
  std::string bundle_id;
  std::string manifest_url;
  IwaInstallerResult::Type result_type;
  std::optional<std::string> update_channel;
  std::optional<std::string> pinned_version;
};

class IwaInstallerTest
    : public ::testing::TestWithParam<IwaInstallerTestParam> {
 public:
  IwaInstallerTest()
      : shared_url_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_factory_)) {
    std::vector<base::test::FeatureRef> enabled_features = {
        features::kIsolatedWebApps, features::kIsolatedWebAppDevMode};
#if BUILDFLAG(IS_CHROMEOS)
    if (GetParam().is_mgs_install_enabled) {
      enabled_features.push_back(
          features::kIsolatedWebAppManagedGuestSessionInstall);
    }
    if (GetParam().is_cache_enabled) {
      enabled_features.push_back(features::kIsolatedWebAppBundleCache);
    }
#endif  // BUILDFLAG(IS_CHROMEOS)
    scoped_feature_list_.InitWithFeatures(std::move(enabled_features),
                                          /*disabled_features=*/{});
  }

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
    AddJsonResponse(kUpdateManifestUrl9, kUpdateManifestValue9);

    test_factory_.AddResponse("https://example.com/app1.swbn",
                              "Content of app1");
    test_factory_.AddResponse("https://example.com/app2.swbn",
                              "Content of app2");
    test_factory_.AddResponse("https://example.com/app6.swbn",
                              "Content of app6");
    test_factory_.AddResponse("https://example.com/app7.swbn", "",
                              net::HttpStatusCode::HTTP_NOT_FOUND);
    test_factory_.AddResponse("https://example.com/app8.swbn",
                              "Content of app8");
    test_factory_.AddResponse("https://example.com/app9.swbn",
                              "Content of app9");

#if BUILDFLAG(IS_CHROMEOS)
    if (!GetParam().is_user_session) {
      test_managed_guest_session_ =
          std::make_unique<profiles::testing::ScopedTestManagedGuestSession>();
    }
#endif  // BUILDFLAG(IS_CHROMEOS)
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
#if BUILDFLAG(IS_CHROMEOS)
  std::unique_ptr<profiles::testing::ScopedTestManagedGuestSession>
      test_managed_guest_session_;
#endif  // BUILDFLAG(IS_CHROMEOS)
  base::test::ScopedFeatureList scoped_feature_list_;

  IsolatedWebAppExternalInstallOptions install_options_ =
      IsolatedWebAppExternalInstallOptions::FromPolicyPrefValue(
          base::Value(test::CreateForceInstallIwaPolicyEntry(
              /*web_bundle_id=*/GetParam().bundle_id,
              /*update_manifest_url=*/GetParam().manifest_url,
              /*update_channel=*/GetParam().update_channel,
              /*pinned_version=*/GetParam().pinned_version)))
          .value();
};

#if BUILDFLAG(IS_CHROMEOS)
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
  IwaInstaller installer(install_options_,
                         IwaInstaller::InstallSourceType::kPolicy,
                         shared_url_loader_factory_, std::move(install_command),
                         log, future.GetCallback());
  installer.Start();

  EXPECT_THAT(future.Get(),
              Property("type", &IwaInstallerResult::type,
                       Eq(!GetParam().is_user_session &&
                                  !GetParam().is_mgs_install_enabled
                              ? IwaInstallerResult::Type::
                                    kErrorManagedGuestSessionInstallDisabled
                              : GetParam().result_type)));
}
#endif  // BUILDFLAG(IS_CHROMEOS)

TEST_P(IwaInstallerTest, NotMgs) {
#if BUILDFLAG(IS_CHROMEOS)
  test_managed_guest_session_.reset();
#endif  // BUILDFLAG(IS_CHROMEOS)

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
  IwaInstaller installer(install_options_,
                         IwaInstaller::InstallSourceType::kPolicy,
                         shared_url_loader_factory_, std::move(install_command),
                         log, future.GetCallback());
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
#if BUILDFLAG(IS_CHROMEOS)
        {.is_mgs_install_enabled = true,
         .is_cache_enabled = true,
         .is_user_session = true,
         .bundle_id = kWebBundleId1,
         .manifest_url = kUpdateManifestUrl1,
         .result_type = IwaInstallerResult::Type::kSuccess},
        // Same as the first test case, but `is_cache_enabled` is disabled.
        {.is_mgs_install_enabled = true,
         .is_cache_enabled = false,
         .is_user_session = true,
         .bundle_id = kWebBundleId1,
         .manifest_url = kUpdateManifestUrl1,
         .result_type = IwaInstallerResult::Type::kSuccess},
        // Same as the first test case, but inside a managed guest session.
        {.is_mgs_install_enabled = true,
         .is_cache_enabled = true,
         .is_user_session = false,
         .bundle_id = kWebBundleId1,
         .manifest_url = kUpdateManifestUrl1,
         .result_type = IwaInstallerResult::Type::kSuccess},
        // Same as the third test case, but caching is disabled.
        {.is_mgs_install_enabled = true,
         .is_cache_enabled = false,
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
        // Same as the first test case, but with non-default release channel.
        {.is_mgs_install_enabled = true,
         .is_user_session = true,
         .bundle_id = kWebBundleId8,
         .manifest_url = kUpdateManifestUrl8,
         .result_type = IwaInstallerResult::Type::kSuccess,
         .update_channel = "beta"},
        // Release channel is not assigned to any version of the app.
        {.is_mgs_install_enabled = true,
         .is_user_session = true,
         .bundle_id = kWebBundleId8,
         .manifest_url = kUpdateManifestUrl1,
         .result_type =
             IwaInstallerResult::Type::kErrorWebBundleUrlCantBeDetermined,
         .update_channel = "beta"},
        // Successful test case of installing IWA in pinned_version (which
        // is not the latest) Update manifest has multiple entries.
        // Default channel.
        {.is_mgs_install_enabled = true,
         .is_user_session = true,
         .bundle_id = kWebBundleId9,
         .manifest_url = kUpdateManifestUrl9,
         .result_type = IwaInstallerResult::Type::kSuccess,
         .pinned_version = "6.0.1"},
        // Failed to install the IWA at pinned version. Version does not
        // exist in update manifest.
        {.is_mgs_install_enabled = true,
         .is_user_session = true,
         .bundle_id = kWebBundleId9,
         .manifest_url = kUpdateManifestUrl9,
         .result_type =
             IwaInstallerResult::Type::kErrorWebBundleUrlCantBeDetermined,
         .pinned_version = "6.0.0"},
        // Successful pinning to a specified version, on non-default update
        // channel.
        {.is_mgs_install_enabled = true,
         .is_user_session = true,
         .bundle_id = kWebBundleId8,
         .manifest_url = kUpdateManifestUrl9,
         .result_type = IwaInstallerResult::Type::kSuccess,
         .update_channel = "beta",
         .pinned_version = "7.0.8"},
        // Failed to install the IWA at pinned version. Version exists,
        // but on different channel, so it is not found.
        {.is_mgs_install_enabled = true,
         .is_user_session = true,
         .bundle_id = kWebBundleId9,
         .manifest_url = kUpdateManifestUrl9,
         .result_type =
             IwaInstallerResult::Type::kErrorWebBundleUrlCantBeDetermined,
         .update_channel = "beta",
         .pinned_version = "6.0.1"},
#endif  // BUILDFLAG(IS_CHROMEOS)
        {.is_mgs_install_enabled = false,
         .is_user_session = false,
         .bundle_id = kWebBundleId1,
         .manifest_url = kUpdateManifestUrl1,
         .result_type = IwaInstallerResult::Type::kSuccess}}));

}  // namespace web_app
