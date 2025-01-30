// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_installer.h"

#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/json/json_writer.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_external_install_options.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_test.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/policy_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/test_iwa_installer_factory.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/test_signed_web_bundle_builder.h"
#include "chrome/browser/web_applications/isolated_web_apps/update_manifest/update_manifest.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_features.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/web_package/test_support/signed_web_bundles/ed25519_key_pair.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {

namespace {

using web_package::SignedWebBundleId;
using web_package::test::Ed25519KeyPair;

constexpr char kVersion1[] = "1.0.0";
constexpr char kVersion2[] = "7.0.6";
constexpr char kVersion3[] = "7.0.8";

const UpdateChannel kBetaChannel = UpdateChannel::Create("beta").value();

}  // namespace

enum UserType {
  kUser = 0,
  kMgs = 1,
};

class IwaInstallerTest : public IsolatedWebAppTest,
                         public testing::WithParamInterface<UserType> {
 public:
  IwaInstallerTest()
      : IsolatedWebAppTest(base::test::TaskEnvironment::TimeSource::DEFAULT) {}

  void SetUp() override {
    IsolatedWebAppTest::SetUp();
    test::AwaitStartWebAppProviderAndSubsystems(profile());

#if BUILDFLAG(IS_CHROMEOS)
    if (IsMgs()) {
      test_managed_guest_session_ =
          std::make_unique<profiles::testing::ScopedTestManagedGuestSession>();
      scoped_feature_list_.InitAndEnableFeature(
          features::kIsolatedWebAppManagedGuestSessionInstall);
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
      const std::optional<base::Version>& pinned_version = std::nullopt) {
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
      const base::Version& pinned_version) {
    return CreateIwaInstaller(bundle_id, log, future,
                              /*update_channel=*/std::nullopt, pinned_version);
  }

  IwaInstallerResult::Type RunInstallerAndWaitForResult(
      const SignedWebBundleId& bundle_id,
      const std::optional<UpdateChannel>& update_channel = std::nullopt,
      const std::optional<base::Version>& pinned_version = std::nullopt) {
    base::test::TestFuture<IwaInstallerResult> future;
    base::Value::List log;
    std::unique_ptr<IwaInstaller> installer = CreateIwaInstaller(
        bundle_id, log, future, update_channel, pinned_version);
    installer->Start();
    return future.Get().type();
  }

  IwaInstallerResult::Type RunInstallerAndWaitForResult(
      const SignedWebBundleId& bundle_id,
      const base::Version& pinned_version) {
    return RunInstallerAndWaitForResult(
        bundle_id, /*update_channel=*/std::nullopt, pinned_version);
  }

  bool IsMgs() { return GetParam() == UserType::kMgs; }

  static inline const SignedWebBundleId kBundleId =
      test::GetDefaultEd25519WebBundleId();
  static inline const Ed25519KeyPair kKeyPair =
      test::GetDefaultEd25519KeyPair();

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

#if BUILDFLAG(IS_CHROMEOS)
  std::unique_ptr<profiles::testing::ScopedTestManagedGuestSession>
      test_managed_guest_session_;
#endif  // BUILDFLAG(IS_CHROMEOS)
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
                kBundleId, /*pinned_version=*/base::Version(kVersion2)),
            IwaInstallerResult::Type::kSuccess);
  AssertAppInstalledAtVersion(kBundleId, kVersion2);
}

TEST_P(IwaInstallerTest, NoPinnedVersionInUpdateManifest) {
  CreateAndPublishIwaBundle(kBundleId, kVersion1);
  CreateAndPublishIwaBundle(kBundleId, kVersion3);

  ASSERT_EQ(RunInstallerAndWaitForResult(
                kBundleId, /*pinned_version=*/base::Version(kVersion2)),
            IwaInstallerResult::Type::kErrorWebBundleUrlCantBeDetermined);
}

TEST_P(IwaInstallerTest, InstallPinnedVersionFromBetaChannel) {
  // Default channel.
  CreateAndPublishIwaBundle(kBundleId, kVersion1);
  CreateAndPublishIwaBundle(kBundleId, kVersion2, kBetaChannel);
  CreateAndPublishIwaBundle(kBundleId, kVersion3, kBetaChannel,
                            /*update_install_page=*/false);

  ASSERT_EQ(
      RunInstallerAndWaitForResult(kBundleId, UpdateChannel(kBetaChannel),
                                   /*pinned_version=*/base::Version(kVersion2)),
      IwaInstallerResult::Type::kSuccess);
  AssertAppInstalledAtVersion(kBundleId, kVersion2);
}

TEST_P(IwaInstallerTest, PinnedVersionIsAvailableInWrongChannel) {
  // Default channel.
  CreateAndPublishIwaBundle(kBundleId, kVersion1);
  CreateAndPublishIwaBundle(kBundleId, kVersion2);

  ASSERT_EQ(
      RunInstallerAndWaitForResult(kBundleId, UpdateChannel(kBetaChannel),
                                   /*pinned_version=*/base::Version(kVersion1)),
      IwaInstallerResult::Type::kErrorWebBundleUrlCantBeDetermined);
}

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

}  // namespace web_app
