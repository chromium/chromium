// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_installer.h"

#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_writer.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_external_install_options.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_test.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/policy_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/test_iwa_installer_factory.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_features.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/web_package/test_support/signed_web_bundles/ed25519_key_pair.h"
#include "components/webapps/isolated_web_apps/test_support/signing_keys.h"
#include "components/webapps/isolated_web_apps/types/iwa_version.h"
#include "components/webapps/isolated_web_apps/types/update_channel.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_paths.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_cache_client.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace web_app {

namespace {

using web_package::SignedWebBundleId;
using web_package::test::Ed25519KeyPair;

constexpr char kVersion1[] = "1.0.0";
constexpr char kVersion2[] = "7.0.6";
constexpr char kVersion3[] = "7.0.8";

const SignedWebBundleId kBundleId = test::GetDefaultEd25519WebBundleId();
const Ed25519KeyPair kKeyPair = test::GetDefaultEd25519KeyPair();
const UpdateChannel kBetaChannel = UpdateChannel::Create("beta").value();

#if BUILDFLAG(IS_CHROMEOS)
constexpr char kCopyBundleToCacheSuccessMetric[] =
    "WebApp.Isolated.CopyBundleToCacheAfterInstallationSuccess";
constexpr char kCopyBundleToCacheErrorMetric[] =
    "WebApp.Isolated.CopyBundleToCacheAfterInstallationError";
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace

enum SessionType {
  kUser = 0,
  kMgs = 1,
};

class IwaInstallerBaseTest : public IsolatedWebAppTest {
 public:
  explicit IwaInstallerBaseTest(SessionType session_type)
      : IsolatedWebAppTest(base::test::TaskEnvironment::TimeSource::DEFAULT),
        session_type_(session_type) {}

  void SetUp() override {
    IsolatedWebAppTest::SetUp();
    test::AwaitStartWebAppProviderAndSubsystems(profile());

#if BUILDFLAG(IS_CHROMEOS)
    if (IsMgs()) {
      test_managed_guest_session_ =
          std::make_unique<profiles::testing::ScopedTestManagedGuestSession>();
    }
#endif  // BUILDFLAG(IS_CHROMEOS)
  }

  // When multiple IWAs are created for the same `bundle_id` with different
  // versions, by default it will override the install page each time. To
  // prevent it, set `update_install_page` to false,
  std::unique_ptr<ScopedBundledIsolatedWebApp> CreateIwaBundle(
      const SignedWebBundleId& bundle_id,
      std::string_view version,
      bool update_install_page = true) {
    CHECK_EQ(SignedWebBundleId::CreateForPublicKey(kKeyPair.public_key),
             bundle_id);
    std::unique_ptr<ScopedBundledIsolatedWebApp> app =
        IsolatedWebAppBuilder(ManifestBuilder().SetVersion(version))
            .BuildBundle(bundle_id, {kKeyPair});
    app->TrustSigningKey();

    if (update_install_page) {
      app->FakeInstallPageState(profile());
    }
    return app;
  }

  // See comment about `update_install_page` in `CreateIwaBundle`.
  void CreateAndPublishIwaBundle(const SignedWebBundleId& bundle_id,
                                 std::string_view version,
                                 bool update_install_page = true,
                                 std::optional<std::vector<UpdateChannel>>
                                     update_channels = std::nullopt) {
    std::unique_ptr<ScopedBundledIsolatedWebApp> app =
        CreateIwaBundle(bundle_id, version, update_install_page);

    test_update_server().AddBundle(std::move(app), update_channels);
  }

  void CreateAndPublishIwaBundle(const SignedWebBundleId& bundle_id,
                                 std::string_view version,
                                 UpdateChannel update_channel,
                                 bool update_install_page = true) {
    CreateAndPublishIwaBundle(bundle_id, version,
                              /*update_install_page=*/update_install_page,
                              std::vector<UpdateChannel>{update_channel});
  }

  void AssertAppInstalledAtVersion(const SignedWebBundleId& web_bundle_id,
                                   std::string_view version) {
    const WebApp* app = provider().registrar_unsafe().GetAppById(
        IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(web_bundle_id)
            .app_id());
    ASSERT_TRUE(app);
    ASSERT_TRUE(app->isolation_data());
    ASSERT_EQ(app->isolation_data()->version().GetString(), version);
  }

  std::unique_ptr<IwaInstaller> CreateIwaInstaller(
      const SignedWebBundleId& bundle_id,
      base::Value::List& log,
      base::test::TestFuture<IwaInstallerResult>& future,
      const std::optional<UpdateChannel>& update_channel = std::nullopt,
      const std::optional<IwaVersion>& pinned_version = std::nullopt) {
    IsolatedWebAppExternalInstallOptions install_options =
        IsolatedWebAppExternalInstallOptions::FromPolicyPrefValue(
            test_update_server().CreateForceInstallPolicyEntry(
                bundle_id, update_channel, pinned_version))
            .value();
    return IwaInstallerFactory::Create(install_options,
                                       IwaInstaller::InstallSourceType::kPolicy,
                                       profile()->GetURLLoaderFactory(), log,
                                       &provider(), future.GetCallback());
  }

  std::unique_ptr<IwaInstaller> CreateIwaInstaller(
      const SignedWebBundleId& bundle_id,
      base::Value::List& log,
      base::test::TestFuture<IwaInstallerResult>& future,
      const IwaVersion& pinned_version) {
    return CreateIwaInstaller(bundle_id, log, future,
                              /*update_channel=*/std::nullopt, pinned_version);
  }

  IwaInstallerResult::Type RunInstallerAndWaitForResult(
      const SignedWebBundleId& bundle_id,
      const std::optional<UpdateChannel>& update_channel = std::nullopt,
      const std::optional<IwaVersion>& pinned_version = std::nullopt) {
    base::test::TestFuture<IwaInstallerResult> future;
    base::Value::List log;
    std::unique_ptr<IwaInstaller> installer = CreateIwaInstaller(
        bundle_id, log, future, update_channel, pinned_version);
    installer->Start();
    return future.Get().type();
  }

  IwaInstallerResult::Type RunInstallerAndWaitForResult(
      const SignedWebBundleId& bundle_id,
      const IwaVersion& pinned_version) {
    return RunInstallerAndWaitForResult(
        bundle_id, /*update_channel=*/std::nullopt, pinned_version);
  }

  bool IsMgs() { return session_type_ == SessionType::kMgs; }

 private:
  SessionType session_type_;
#if BUILDFLAG(IS_CHROMEOS)
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kIsolatedWebAppManagedGuestSessionInstall};
  std::unique_ptr<profiles::testing::ScopedTestManagedGuestSession>
      test_managed_guest_session_;
#endif  // BUILDFLAG(IS_CHROMEOS)
};

class IwaInstallerTest : public IwaInstallerBaseTest,
                         public testing::WithParamInterface<SessionType> {
 public:
  IwaInstallerTest() : IwaInstallerBaseTest(/*session_type=*/GetParam()) {}
};

TEST_P(IwaInstallerTest, SimpleInstall) {
  CreateAndPublishIwaBundle(kBundleId, kVersion1);

  ASSERT_EQ(RunInstallerAndWaitForResult(kBundleId),
            IwaInstallerResult::Type::kSuccess);
  AssertAppInstalledAtVersion(kBundleId, kVersion1);
}

TEST_P(IwaInstallerTest, InstallLatestVersion) {
  CreateAndPublishIwaBundle(kBundleId, kVersion1);
  CreateAndPublishIwaBundle(kBundleId, kVersion3);
  CreateAndPublishIwaBundle(kBundleId, kVersion2,
                            /*update_install_page=*/false);

  ASSERT_EQ(RunInstallerAndWaitForResult(kBundleId),
            IwaInstallerResult::Type::kSuccess);
  AssertAppInstalledAtVersion(kBundleId, kVersion3);
}

TEST_P(IwaInstallerTest, UpdateManifestDownloadFailed) {
  CreateAndPublishIwaBundle(kBundleId, kVersion1);
  test_update_server().SetServedUpdateManifestResponse(
      kBundleId, net::HttpStatusCode::HTTP_NOT_FOUND, /*json_content=*/"");

  EXPECT_EQ(RunInstallerAndWaitForResult(kBundleId),
            IwaInstallerResult::Type::kErrorUpdateManifestDownloadFailed);
}

TEST_P(IwaInstallerTest, UpdateManifestParsingFailed) {
  const std::string kUpdateManifestNotJson = "not json";

  CreateAndPublishIwaBundle(kBundleId, kVersion1);
  test_update_server().SetServedUpdateManifestResponse(
      kBundleId, net::HttpStatusCode::HTTP_OK, kUpdateManifestNotJson);

  EXPECT_EQ(RunInstallerAndWaitForResult(kBundleId),
            IwaInstallerResult::Type::kErrorUpdateManifestParsingFailed);
}

TEST_P(IwaInstallerTest, InvalidUpdateManifestSrcUrl) {
  const base::Value::Dict kUpdateManifestWithInvalidSrcUrl =
      base::Value::Dict().Set(
          "versions", base::Value::List().Append(
                          base::Value::Dict()
                              .Set("version", kVersion1)
                              .Set("src", "chrome-extension://app5.wbn")));

  CreateAndPublishIwaBundle(kBundleId, kVersion1);
  test_update_server().SetServedUpdateManifestResponse(
      kBundleId, net::HttpStatusCode::HTTP_OK,
      *base::WriteJson(kUpdateManifestWithInvalidSrcUrl));

  EXPECT_EQ(RunInstallerAndWaitForResult(kBundleId),
            IwaInstallerResult::Type::kErrorWebBundleUrlCantBeDetermined);
}

TEST_P(IwaInstallerTest, CantDownloadWebBundle) {
  const std::string_view kBundleUrl = "https://example.com/app1.swbn";
  const std::string_view kBundleContent =
      "does-not-matter-because-http-not-found";
  const base::Value::Dict kUpdateManifestWithCustomBundleUrl =
      base::Value::Dict().Set(
          "versions", base::Value::List().Append(base::Value::Dict()
                                                     .Set("version", kVersion1)
                                                     .Set("src", kBundleUrl)));
  url_loader_factory().AddResponse(kBundleUrl, kBundleContent,
                                   net::HttpStatusCode::HTTP_NOT_FOUND);

  test_update_server().SetServedUpdateManifestResponse(
      kBundleId, net::HttpStatusCode::HTTP_OK,
      *base::WriteJson(kUpdateManifestWithCustomBundleUrl));

  EXPECT_EQ(RunInstallerAndWaitForResult(kBundleId),
            IwaInstallerResult::Type::kErrorCantDownloadWebBundle);
}

TEST_P(IwaInstallerTest, CantInstallFromWebBundle) {
  // Set a custom bundle url to the update manifest and set the invalid bundle
  // content as a response to this custom bundle url.
  const std::string_view kBundleUrl = "https://example.com/app1.swbn";
  const std::string_view kBundleContent = "invalid";
  const base::Value::Dict kUpdateManifestWithCustomBundleUrl =
      base::Value::Dict().Set(
          "versions", base::Value::List().Append(base::Value::Dict()
                                                     .Set("version", kVersion1)
                                                     .Set("src", kBundleUrl)));
  url_loader_factory().AddResponse(kBundleUrl, kBundleContent);

  test_update_server().SetServedUpdateManifestResponse(
      kBundleId, net::HttpStatusCode::HTTP_OK,
      *base::WriteJson(kUpdateManifestWithCustomBundleUrl));

  EXPECT_EQ(RunInstallerAndWaitForResult(kBundleId),
            IwaInstallerResult::Type::kErrorCantInstallFromWebBundle);
}

TEST_P(IwaInstallerTest, BetaChannel) {
  CreateAndPublishIwaBundle(kBundleId, kVersion1, kBetaChannel);

  ASSERT_EQ(
      RunInstallerAndWaitForResult(kBundleId, UpdateChannel(kBetaChannel)),
      IwaInstallerResult::Type::kSuccess);
  AssertAppInstalledAtVersion(kBundleId, kVersion1);
}

TEST_P(IwaInstallerTest, InstallBetaChannelWhenRequested) {
  CreateAndPublishIwaBundle(kBundleId, kVersion1, kBetaChannel);
  // Default channel.
  CreateAndPublishIwaBundle(kBundleId, kVersion2,
                            /*update_install_page=*/false);

  ASSERT_EQ(
      RunInstallerAndWaitForResult(kBundleId, UpdateChannel(kBetaChannel)),
      IwaInstallerResult::Type::kSuccess);
  AssertAppInstalledAtVersion(kBundleId, kVersion1);
}

TEST_P(IwaInstallerTest, NoVersionInBetaChannel) {
  // Default channel.
  CreateAndPublishIwaBundle(kBundleId, kVersion1);

  ASSERT_EQ(
      RunInstallerAndWaitForResult(kBundleId, UpdateChannel(kBetaChannel)),
      IwaInstallerResult::Type::kErrorWebBundleUrlCantBeDetermined);
}

TEST_P(IwaInstallerTest, InstallPinnedVersion) {
  CreateAndPublishIwaBundle(kBundleId, kVersion1);
  CreateAndPublishIwaBundle(kBundleId, kVersion2);
  CreateAndPublishIwaBundle(kBundleId, kVersion3,
                            /*update_install_page=*/false);

  ASSERT_EQ(RunInstallerAndWaitForResult(
                kBundleId, /*pinned_version=*/*IwaVersion::Create(kVersion2)),
            IwaInstallerResult::Type::kSuccess);
  AssertAppInstalledAtVersion(kBundleId, kVersion2);
}

TEST_P(IwaInstallerTest, NoPinnedVersionInUpdateManifest) {
  CreateAndPublishIwaBundle(kBundleId, kVersion1);
  CreateAndPublishIwaBundle(kBundleId, kVersion3);

  ASSERT_EQ(RunInstallerAndWaitForResult(
                kBundleId, /*pinned_version=*/*IwaVersion::Create(kVersion2)),
            IwaInstallerResult::Type::kErrorWebBundleUrlCantBeDetermined);
}

TEST_P(IwaInstallerTest, InstallPinnedVersionFromBetaChannel) {
  // Default channel.
  CreateAndPublishIwaBundle(kBundleId, kVersion1);
  CreateAndPublishIwaBundle(kBundleId, kVersion2, kBetaChannel);
  CreateAndPublishIwaBundle(kBundleId, kVersion3, kBetaChannel,
                            /*update_install_page=*/false);

  ASSERT_EQ(RunInstallerAndWaitForResult(
                kBundleId, UpdateChannel(kBetaChannel),
                /*pinned_version=*/*IwaVersion::Create(kVersion2)),
            IwaInstallerResult::Type::kSuccess);
  AssertAppInstalledAtVersion(kBundleId, kVersion2);
}

TEST_P(IwaInstallerTest, PinnedVersionIsAvailableInWrongChannel) {
  // Default channel.
  CreateAndPublishIwaBundle(kBundleId, kVersion1);
  CreateAndPublishIwaBundle(kBundleId, kVersion2);

  ASSERT_EQ(RunInstallerAndWaitForResult(
                kBundleId, UpdateChannel(kBetaChannel),
                /*pinned_version=*/*IwaVersion::Create(kVersion1)),
            IwaInstallerResult::Type::kErrorWebBundleUrlCantBeDetermined);
}

// Checks enabling caching does not break the installation.
TEST_P(IwaInstallerTest, CachingEnabled) {
#if BUILDFLAG(IS_CHROMEOS)
  base::test::ScopedFeatureList scoped_feature_list(
      features::kIsolatedWebAppBundleCache);
#endif  // BUILDFLAG(IS_CHROMEOS)

  CreateAndPublishIwaBundle(kBundleId, kVersion1);

  ASSERT_EQ(RunInstallerAndWaitForResult(kBundleId),
            IwaInstallerResult::Type::kSuccess);
  AssertAppInstalledAtVersion(kBundleId, kVersion1);
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    IwaInstallerTest,
    testing::Values(kUser, kMgs));

#if BUILDFLAG(IS_CHROMEOS)
// IWA cache installation tests for Managed Guest Session (MGS).
class IwaMgsCachingInstallerTest : public IwaInstallerBaseTest {
 public:
  IwaMgsCachingInstallerTest() : IwaInstallerBaseTest(kMgs) {}

  void SetUp() override {
    IwaInstallerBaseTest::SetUp();
    OverrideCacheDir();
  }

  void OverrideCacheDir() {
    ASSERT_TRUE(cache_root_dir_.CreateUniqueTempDir());
    cache_root_dir_override_ = std::make_unique<base::ScopedPathOverride>(
        ash::DIR_DEVICE_LOCAL_ACCOUNT_IWA_CACHE, CacheRootPath());
  }

  void DestroyCacheDir() { cache_root_dir_override_.reset(); }

  base::FilePath GetBundleDirWithVersion(const SignedWebBundleId& bundle_id,
                                         const IwaVersion& version) {
    auto session_cache_dir =
        IwaCacheClient::GetCacheBaseDirectoryForSessionType(
            IwaCacheClient::SessionType::kManagedGuestSession, CacheRootPath());
    return IwaCacheClient::GetCacheDirectoryForBundleWithVersion(
        session_cache_dir, bundle_id, version);
  }

  base::FilePath GetFullBundlePath(const SignedWebBundleId& bundle_id,
                                   const IwaVersion& version) {
    return IwaCacheClient::GetBundleFullName(
        GetBundleDirWithVersion(bundle_id, version));
  }

  void CopyBundleToCache(const web_package::SignedWebBundleId& web_bundle_id,
                         const IwaVersion& version,
                         const base::FilePath& bundle_to_copy) {
    ASSERT_TRUE(
        base::CreateDirectory(GetBundleDirWithVersion(web_bundle_id, version)));
    ASSERT_TRUE(base::CopyFile(bundle_to_copy,
                               GetFullBundlePath(web_bundle_id, version)));
  }

  void ExpectEmptyCopyBundleMetrics() {
    histogram_tester_.ExpectTotalCount(kCopyBundleToCacheSuccessMetric, 0);
    histogram_tester_.ExpectTotalCount(kCopyBundleToCacheErrorMetric, 0);
  }

  void ExpectSuccessCopyBundleMetric() {
    EXPECT_THAT(
        histogram_tester_.GetAllSamples(kCopyBundleToCacheSuccessMetric),
        BucketsAre(base::Bucket(true, 1)));
    histogram_tester_.ExpectTotalCount(kCopyBundleToCacheErrorMetric, 0);
  }

  void ExpectErrorCopyBundleMetric(const CopyBundleToCacheError& error) {
    EXPECT_THAT(
        histogram_tester_.GetAllSamples(kCopyBundleToCacheSuccessMetric),
        BucketsAre(base::Bucket(false, 1)));
    EXPECT_THAT(histogram_tester_.GetAllSamples(kCopyBundleToCacheErrorMetric),
                BucketsAre(base::Bucket(error, 1)));
  }

 protected:
  const base::FilePath& CacheRootPath() { return cache_root_dir_.GetPath(); }

  base::HistogramTester histogram_tester_;
  base::ScopedTempDir cache_root_dir_;
  std::unique_ptr<base::ScopedPathOverride> cache_root_dir_override_;
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kIsolatedWebAppBundleCache};
};

TEST_F(IwaMgsCachingInstallerTest,
       BundleCopiedToCacheAfterSuccessfulInstallation) {
  ExpectEmptyCopyBundleMetrics();
  CreateAndPublishIwaBundle(kBundleId, kVersion1);

  ASSERT_EQ(RunInstallerAndWaitForResult(kBundleId),
            IwaInstallerResult::Type::kSuccess);

  AssertAppInstalledAtVersion(kBundleId, kVersion1);
  // Checks that bundle exists in cache after successful installation.
  EXPECT_TRUE(base::PathExists(
      GetFullBundlePath(kBundleId, *IwaVersion::Create(kVersion1))));
  ExpectSuccessCopyBundleMetric();
}

TEST_F(IwaMgsCachingInstallerTest,
       BundleNotCopiedToCacheAfterFailedInstallation) {
  ExpectEmptyCopyBundleMetrics();
  CreateAndPublishIwaBundle(kBundleId, kVersion1);
  test_update_server().SetServedUpdateManifestResponse(
      kBundleId, net::HttpStatusCode::HTTP_NOT_FOUND, /*json_content=*/"");

  EXPECT_EQ(RunInstallerAndWaitForResult(kBundleId),
            IwaInstallerResult::Type::kErrorUpdateManifestDownloadFailed);

  EXPECT_FALSE(base::PathExists(
      GetFullBundlePath(kBundleId, *IwaVersion::Create(kVersion1))));
  ExpectEmptyCopyBundleMetrics();
}

TEST_F(IwaMgsCachingInstallerTest, FailedToCopyBundleToCache) {
  ExpectEmptyCopyBundleMetrics();
  DestroyCacheDir();
  CreateAndPublishIwaBundle(kBundleId, kVersion1);

  ASSERT_EQ(RunInstallerAndWaitForResult(kBundleId),
            IwaInstallerResult::Type::kSuccess);

  AssertAppInstalledAtVersion(kBundleId, kVersion1);
  ExpectErrorCopyBundleMetric(CopyBundleToCacheError::kFailedToCreateDir);
}

TEST_F(IwaMgsCachingInstallerTest, InstallFromCache) {
  histogram_tester_.ExpectTotalCount("WebApp.Isolated.InstallFromCache", 0);
  // Change the response, so the installation can only happen from the cache.
  std::unique_ptr<ScopedBundledIsolatedWebApp> app =
      CreateIwaBundle(kBundleId, kVersion1);
  test_update_server().SetServedUpdateManifestResponse(
      kBundleId, net::HttpStatusCode::HTTP_NOT_FOUND,
      /*json_content=*/"");

  CopyBundleToCache(app->web_bundle_id(), app->version(), app->path());

  ASSERT_EQ(RunInstallerAndWaitForResult(kBundleId),
            IwaInstallerResult::Type::kSuccess);
  AssertAppInstalledAtVersion(kBundleId, kVersion1);
  EXPECT_THAT(
      histogram_tester_.GetAllSamples("WebApp.Isolated.InstallFromCache"),
      BucketsAre(base::Bucket(true, 1)));
}

TEST_F(IwaMgsCachingInstallerTest, InstallFromCacheFailedRetryFromInternet) {
  histogram_tester_.ExpectTotalCount("WebApp.Isolated.InstallFromCache", 0);
  // Change the response, so the installation can only happen from the cache.
  std::unique_ptr<ScopedBundledIsolatedWebApp> app =
      CreateIwaBundle(kBundleId, kVersion1);
  test_update_server().SetServedUpdateManifestResponse(
      kBundleId, net::HttpStatusCode::HTTP_NOT_FOUND,
      /*json_content=*/"");

  // Installer will try to install the IWA from cache since the cache file
  // exists, but it will fail since it is not a real bundle. Then the
  // installation will happen from the Internet (and fail because we changed the
  // response).
  base::FilePath temp_file;
  base::CreateTemporaryFile(&temp_file);
  CopyBundleToCache(app->web_bundle_id(), app->version(), temp_file);

  EXPECT_EQ(RunInstallerAndWaitForResult(kBundleId),
            IwaInstallerResult::Type::kErrorUpdateManifestDownloadFailed);
  EXPECT_THAT(
      histogram_tester_.GetAllSamples("WebApp.Isolated.InstallFromCache"),
      BucketsAre(base::Bucket(false, 1)));
}

#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace web_app
