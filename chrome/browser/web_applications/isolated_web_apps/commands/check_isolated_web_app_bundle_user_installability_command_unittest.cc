// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/commands/check_isolated_web_app_bundle_user_installability_command.h"

#include <memory>
#include <optional>
#include <string>

#include "base/files/file_util.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/signed_web_bundle_metadata.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/fake_chrome_iwa_runtime_data_provider.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/common/chrome_features.h"
#include "components/webapps/isolated_web_apps/types/source.h"
#include "components/webapps/isolated_web_apps/types/storage_location.h"
#include "content/public/common/content_features.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {
namespace {

using base::test::HasValue;

class CheckIsolatedWebAppBundleUserInstallabilityCommandTest
    : public WebAppTest {
 protected:
  void SetUp() override {
    resetter_ =
        ChromeIwaRuntimeDataProvider::SetInstanceForTesting(&data_provider_);
    WebAppTest::SetUp();
    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  std::unique_ptr<BundledIsolatedWebApp> CreateApp(
      const std::string& version,
      bool add_to_user_install_allowlist,
      bool add_to_blocklist) {
    base::FilePath bundle_path =
        IwaStorageOwnedBundle{"bundle-" + version, /*dev_mode=*/false}.GetPath(
            profile()->GetPath());
    EXPECT_TRUE(base::CreateDirectory(bundle_path.DirName()));

    std::unique_ptr<BundledIsolatedWebApp> app =
        IsolatedWebAppBuilder(ManifestBuilder().SetVersion(version))
            .BuildBundle(bundle_path, key_pair_);
    app->TrustSigningKey();
    app->FakeInstallPageState(profile());

    data_provider_.Update([&](auto& update) {
      if (add_to_user_install_allowlist) {
        update.AddToUserInstallAllowlist(
            app->web_bundle_id(),
            ChromeIwaRuntimeDataProvider::UserInstallAllowlistItemData(
                /*enterprise_name=*/"fancy comp"));
      }

      if (add_to_blocklist) {
        update.AddToBlocklist(app->web_bundle_id());
      }
    });

    return app;
  }

  base::expected<SignedWebBundleMetadata, std::string> GetBundleMetadata(
      const BundledIsolatedWebApp& app) {
    auto url_info =
        IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(app.web_bundle_id());

    base::test::TestFuture<base::expected<SignedWebBundleMetadata, std::string>>
        metadata_future;
    SignedWebBundleMetadata::Create(profile(), &fake_provider(), url_info,
                                    IwaSourceBundleProdMode(app.path()),
                                    metadata_future.GetCallback());
    return metadata_future.Take();
  }

  void ScheduleCommand(
      const SignedWebBundleMetadata& bundle_metadata,
      base::OnceCallback<void(IsolatedInstallabilityCheckResult,
                              std::optional<IwaVersion>)> callback) {
    fake_provider().scheduler().CheckIsolatedWebAppBundleUserInstallability(
        bundle_metadata, std::move(callback));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kIsolatedWebApps};
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  web_package::test::KeyPair key_pair_ =
      web_package::test::Ed25519KeyPair::CreateRandom();
  FakeIwaRuntimeDataProvider data_provider_;
  std::optional<base::AutoReset<ChromeIwaRuntimeDataProvider*>> resetter_;
};

TEST_F(CheckIsolatedWebAppBundleUserInstallabilityCommandTest,
       SucceedsWhenAppNotInRegistrar) {
  std::unique_ptr<BundledIsolatedWebApp> app =
      CreateApp("7.7.7", /*add_to_user_install_allowlist=*/true,
                /*add_to_blocklist=*/false);
  ASSERT_OK_AND_ASSIGN(SignedWebBundleMetadata metadata,
                       GetBundleMetadata(*app));

  base::test::TestFuture<IsolatedInstallabilityCheckResult,
                         std::optional<IwaVersion>>
      command_future;
  ScheduleCommand(metadata, command_future.GetCallback());
  IsolatedInstallabilityCheckResult result = command_future.Get<0>();
  std::optional<IwaVersion> installed_version = command_future.Get<1>();

  EXPECT_EQ(result, IsolatedInstallabilityCheckResult::kInstallable);
  EXPECT_FALSE(installed_version.has_value());
}

TEST_F(CheckIsolatedWebAppBundleUserInstallabilityCommandTest,
       SucceedsWhenInstalledAppLowerVersion) {
  std::unique_ptr<BundledIsolatedWebApp> current_app =
      CreateApp("7.7.6", /*add_to_user_install_allowlist=*/true,
                /*add_to_blocklist=*/false);
  ASSERT_THAT(current_app->Install(profile()), HasValue());

  std::unique_ptr<BundledIsolatedWebApp> app =
      CreateApp("7.7.7", /*add_to_user_install_allowlist=*/true,
                /*add_to_blocklist=*/false);
  ASSERT_OK_AND_ASSIGN(SignedWebBundleMetadata metadata,
                       GetBundleMetadata(*app));

  base::test::TestFuture<IsolatedInstallabilityCheckResult,
                         std::optional<IwaVersion>>
      command_future;
  ScheduleCommand(metadata, command_future.GetCallback());
  IsolatedInstallabilityCheckResult result = command_future.Get<0>();
  std::optional<IwaVersion> installed_version = command_future.Get<1>();

  EXPECT_EQ(result, IsolatedInstallabilityCheckResult::kUpdatable);
  EXPECT_EQ(installed_version, *IwaVersion::Create("7.7.6"));
}

TEST_F(CheckIsolatedWebAppBundleUserInstallabilityCommandTest,
       FailsWhenInstalledAppSameVersion) {
  std::unique_ptr<BundledIsolatedWebApp> app =
      CreateApp("7.7.7", /*add_to_user_install_allowlist=*/true,
                /*add_to_blocklist=*/false);
  ASSERT_THAT(app->Install(profile()), HasValue());
  ASSERT_OK_AND_ASSIGN(SignedWebBundleMetadata metadata,
                       GetBundleMetadata(*app));

  base::test::TestFuture<IsolatedInstallabilityCheckResult,
                         std::optional<IwaVersion>>
      command_future;
  ScheduleCommand(metadata, command_future.GetCallback());
  IsolatedInstallabilityCheckResult result = command_future.Get<0>();
  std::optional<IwaVersion> installed_version = command_future.Get<1>();

  EXPECT_EQ(result, IsolatedInstallabilityCheckResult::kOutdated);
  EXPECT_EQ(installed_version, *IwaVersion::Create("7.7.7"));
}

TEST_F(CheckIsolatedWebAppBundleUserInstallabilityCommandTest,
       FailsWhenInstalledAppHigherVersion) {
  std::unique_ptr<BundledIsolatedWebApp> current_app =
      CreateApp("7.7.8", /*add_to_user_install_allowlist=*/true,
                /*add_to_blocklist=*/false);
  ASSERT_THAT(current_app->Install(profile()), HasValue());

  std::unique_ptr<BundledIsolatedWebApp> app =
      CreateApp("7.7.7", /*add_to_user_install_allowlist=*/true,
                /*add_to_blocklist=*/false);
  ASSERT_OK_AND_ASSIGN(SignedWebBundleMetadata metadata,
                       GetBundleMetadata(*app));

  base::test::TestFuture<IsolatedInstallabilityCheckResult,
                         std::optional<IwaVersion>>
      command_future;
  ScheduleCommand(metadata, command_future.GetCallback());
  IsolatedInstallabilityCheckResult result = command_future.Get<0>();
  std::optional<IwaVersion> installed_version = command_future.Get<1>();

  EXPECT_EQ(result, IsolatedInstallabilityCheckResult::kOutdated);
  EXPECT_EQ(installed_version, *IwaVersion::Create("7.7.8"));
}

TEST_F(CheckIsolatedWebAppBundleUserInstallabilityCommandTest,
       FailsWhenAppBlocklisted) {
  std::unique_ptr<BundledIsolatedWebApp> app =
      CreateApp("7.7.8", /*add_to_user_install_allowlist=*/false,
                /*add_to_blocklist=*/true);

  ASSERT_OK_AND_ASSIGN(SignedWebBundleMetadata metadata,
                       GetBundleMetadata(*app));

  base::test::TestFuture<IsolatedInstallabilityCheckResult,
                         std::optional<IwaVersion>>
      command_future;
  ScheduleCommand(metadata, command_future.GetCallback());
  IsolatedInstallabilityCheckResult result = command_future.Get<0>();
  std::optional<IwaVersion> installed_version = command_future.Get<1>();

  ASSERT_EQ(result, IsolatedInstallabilityCheckResult::kBlocklisted);
  ASSERT_FALSE(installed_version.has_value());
}

// Component should never be in such a state, but it is still worth to verify
// that app is not installable even in that case
TEST_F(CheckIsolatedWebAppBundleUserInstallabilityCommandTest,
       FailsWhenAppBlocklistedEvenIfAllowlisted) {
  std::unique_ptr<BundledIsolatedWebApp> app =
      CreateApp("7.7.8", /*add_to_user_install_allowlist=*/true,
                /*add_to_blocklist=*/true);

  ASSERT_OK_AND_ASSIGN(SignedWebBundleMetadata metadata,
                       GetBundleMetadata(*app));

  base::test::TestFuture<IsolatedInstallabilityCheckResult,
                         std::optional<IwaVersion>>
      command_future;
  ScheduleCommand(metadata, command_future.GetCallback());
  IsolatedInstallabilityCheckResult result = command_future.Get<0>();
  std::optional<IwaVersion> installed_version = command_future.Get<1>();

  ASSERT_EQ(result, IsolatedInstallabilityCheckResult::kBlocklisted);
  ASSERT_FALSE(installed_version.has_value());
}

TEST_F(CheckIsolatedWebAppBundleUserInstallabilityCommandTest,
       FailsWhenNotOnUserInstallAllowlist) {
  std::unique_ptr<BundledIsolatedWebApp> app =
      CreateApp("7.7.8", /*add_to_user_install_allowlist=*/false,
                /*add_to_blocklist=*/false);

  ASSERT_OK_AND_ASSIGN(SignedWebBundleMetadata metadata,
                       GetBundleMetadata(*app));

  base::test::TestFuture<IsolatedInstallabilityCheckResult,
                         std::optional<IwaVersion>>
      command_future;
  ScheduleCommand(metadata, command_future.GetCallback());
  IsolatedInstallabilityCheckResult result = command_future.Get<0>();
  std::optional<IwaVersion> installed_version = command_future.Get<1>();

  ASSERT_EQ(result,
            IsolatedInstallabilityCheckResult::kNotOnUserInstallAllowlist);
  ASSERT_FALSE(installed_version.has_value());
}

TEST_F(CheckIsolatedWebAppBundleUserInstallabilityCommandTest,
       FailsWhenAlreadyInstalledAndRemovedFromUserInstallAllowlist) {
  std::unique_ptr<BundledIsolatedWebApp> app =
      CreateApp("7.7.8", /*add_to_user_install_allowlist=*/false,
                /*add_to_blocklist=*/false);
  ASSERT_THAT(app->Install(profile()), HasValue());

  ASSERT_OK_AND_ASSIGN(SignedWebBundleMetadata metadata,
                       GetBundleMetadata(*app));

  base::test::TestFuture<IsolatedInstallabilityCheckResult,
                         std::optional<IwaVersion>>
      command_future;
  ScheduleCommand(metadata, command_future.GetCallback());
  IsolatedInstallabilityCheckResult result = command_future.Get<0>();
  std::optional<IwaVersion> installed_version = command_future.Get<1>();

  ASSERT_EQ(result,
            IsolatedInstallabilityCheckResult::kNotOnUserInstallAllowlist);
  ASSERT_FALSE(installed_version.has_value());
}

class CheckIsolatedWebAppBundleUserInstallabilityCommandDevModeTest
    : public CheckIsolatedWebAppBundleUserInstallabilityCommandTest {
 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kIsolatedWebAppDevMode};
};

TEST_F(CheckIsolatedWebAppBundleUserInstallabilityCommandDevModeTest,
       SucceedsWhenInstalledAppLowerVersion) {
  std::unique_ptr<BundledIsolatedWebApp> current_app =
      CreateApp("7.7.6", /*add_to_user_install_allowlist=*/true,
                /*add_to_blocklist=*/false);
  ASSERT_THAT(current_app->Install(profile()), HasValue());

  std::unique_ptr<BundledIsolatedWebApp> app =
      CreateApp("7.7.7", /*add_to_user_install_allowlist=*/true,
                /*add_to_blocklist=*/false);
  ASSERT_OK_AND_ASSIGN(SignedWebBundleMetadata metadata,
                       GetBundleMetadata(*app));

  base::test::TestFuture<IsolatedInstallabilityCheckResult,
                         std::optional<IwaVersion>>
      command_future;
  ScheduleCommand(metadata, command_future.GetCallback());
  IsolatedInstallabilityCheckResult result = command_future.Get<0>();
  std::optional<IwaVersion> installed_version = command_future.Get<1>();

  EXPECT_EQ(result, IsolatedInstallabilityCheckResult::kUpdatable);
  EXPECT_EQ(installed_version, *IwaVersion::Create("7.7.6"));
}

TEST_F(CheckIsolatedWebAppBundleUserInstallabilityCommandDevModeTest,
       SucceedsWhenInstalledAppSameVersion) {
  std::unique_ptr<BundledIsolatedWebApp> app =
      CreateApp("7.7.7", /*add_to_user_install_allowlist=*/true,
                /*add_to_blocklist=*/false);
  ASSERT_THAT(app->Install(profile()), HasValue());
  ASSERT_OK_AND_ASSIGN(SignedWebBundleMetadata metadata,
                       GetBundleMetadata(*app));

  base::test::TestFuture<IsolatedInstallabilityCheckResult,
                         std::optional<IwaVersion>>
      command_future;
  ScheduleCommand(metadata, command_future.GetCallback());
  IsolatedInstallabilityCheckResult result = command_future.Get<0>();
  std::optional<IwaVersion> installed_version = command_future.Get<1>();

  EXPECT_EQ(result, IsolatedInstallabilityCheckResult::kUpdatable);
  EXPECT_EQ(installed_version, *IwaVersion::Create("7.7.7"));
}

TEST_F(CheckIsolatedWebAppBundleUserInstallabilityCommandDevModeTest,
       FailsWhenInstalledAppHigherVersion) {
  std::unique_ptr<BundledIsolatedWebApp> current_app =
      CreateApp("7.7.8", /*add_to_user_install_allowlist=*/true,
                /*add_to_blocklist=*/false);
  ASSERT_THAT(current_app->Install(profile()), HasValue());

  std::unique_ptr<BundledIsolatedWebApp> app =
      CreateApp("7.7.7", /*add_to_user_install_allowlist=*/true,
                /*add_to_blocklist=*/false);
  ASSERT_OK_AND_ASSIGN(SignedWebBundleMetadata metadata,
                       GetBundleMetadata(*app));

  base::test::TestFuture<IsolatedInstallabilityCheckResult,
                         std::optional<IwaVersion>>
      command_future;
  ScheduleCommand(metadata, command_future.GetCallback());
  IsolatedInstallabilityCheckResult result = command_future.Get<0>();
  std::optional<IwaVersion> installed_version = command_future.Get<1>();

  EXPECT_EQ(result, IsolatedInstallabilityCheckResult::kOutdated);
  EXPECT_EQ(installed_version, *IwaVersion::Create("7.7.8"));
}

}  // namespace
}  // namespace web_app
