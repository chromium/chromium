// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/overloaded.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_builder.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/install_isolated_web_app_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_location.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/jobs/uninstall/remove_web_app_job.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {
namespace {

class IsolatedWebAppUninstallBrowserTest
    : public IsolatedWebAppBrowserTestHarness,
      public ::testing::WithParamInterface<bool> {
 protected:
  using InstallResult = base::expected<InstallIsolatedWebAppCommandSuccess,
                                       InstallIsolatedWebAppCommandError>;

  void SetUp() override {
    ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());

    src_bundle_path_ = scoped_temp_dir_.GetPath().Append(
        base::FilePath::FromASCII("bundle.swbn"));
    src_location_ =
        is_dev_mode_
            ? IsolatedWebAppLocation(DevModeBundle{.path = src_bundle_path_})
            : IsolatedWebAppLocation(InstalledBundle{.path = src_bundle_path_});

    IsolatedWebAppBrowserTestHarness::SetUp();
  }

  void CreateBundle() {
    base::ScopedAllowBlockingForTesting allow_blocking;
    TestSignedWebBundle bundle = TestSignedWebBundleBuilder::BuildDefault(
        TestSignedWebBundleBuilder::BuildOptions()
            .SetKeyPair(key_pair_)
            .SetAppName("Test App"));
    ASSERT_TRUE(base::WriteFile(src_bundle_path_, bundle.data));
  }

  void Install() {
    base::test::TestFuture<InstallResult> future;
    provider()->scheduler().InstallIsolatedWebApp(
        url_info_, src_location_,
        /*expected_version=*/absl::nullopt,
        /*optional_keep_alive=*/nullptr,
        /*optional_profile_keep_alive=*/nullptr, future.GetCallback());
    ASSERT_TRUE(future.Wait());

    const WebApp* web_app =
        provider()->registrar_unsafe().GetAppById(url_info_.app_id());
    ASSERT_TRUE(web_app);
  }

  void Uninstall() {
    base::RunLoop run_loop;
    auto* browsing_data_remover = profile()->GetBrowsingDataRemover();
    browsing_data_remover->SetWouldCompleteCallbackForTesting(
        base::BindLambdaForTesting([&](base::OnceClosure callback) {
          if (browsing_data_remover->GetPendingTaskCountForTesting() == 1) {
            run_loop.Quit();
          }
          std::move(callback).Run();
        }));

    base::test::TestFuture<webapps::UninstallResultCode> future;
    auto job = std::make_unique<RemoveWebAppJob>(
        webapps::WebappUninstallSource::kAppsPage, *profile(),
        url_info_.app_id());

    provider()->scheduler().UninstallWebApp(
        url_info_.app_id(), webapps::WebappUninstallSource::kAppsPage,
        future.GetCallback());

    auto code = future.Get();
    ASSERT_EQ(code, webapps::UninstallResultCode::kSuccess);
    run_loop.Run();
  }

  WebAppProvider* provider() {
    return WebAppProvider::GetForWebApps(profile());
  }

  bool is_dev_mode_ = GetParam();

  base::ScopedTempDir scoped_temp_dir_;

  web_package::WebBundleSigner::KeyPair key_pair_ =
      web_package::WebBundleSigner::KeyPair(kTestPublicKey, kTestPrivateKey);

  IsolatedWebAppUrlInfo url_info_ =
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
          web_package::SignedWebBundleId::CreateForEd25519PublicKey(
              key_pair_.public_key));

  base::FilePath src_bundle_path_;
  IsolatedWebAppLocation src_location_;
};

IN_PROC_BROWSER_TEST_P(IsolatedWebAppUninstallBrowserTest, Succeeds) {
  ASSERT_NO_FATAL_FAILURE(CreateBundle());
  absl::optional<base::FilePath> path_to_iwa_in_profile;

  // Install an IWA and check that it is in the desired stated.
  ASSERT_NO_FATAL_FAILURE(Install());
  const WebApp* web_app_before =
      provider()->registrar_unsafe().GetAppById(url_info_.app_id());
  ASSERT_TRUE(web_app_before);
  ASSERT_TRUE(web_app_before->isolation_data().has_value());

  absl::visit(base::Overloaded{
                  [&](const InstalledBundle& location) {
                    // Verify that .swbn file was copied to
                    // the profile directory.
                    base::ScopedAllowBlockingForTesting allow_blocking;
                    EXPECT_NE(location.path, src_bundle_path_);
                    EXPECT_THAT(location.path,
                                test::IsInIwaRandomDir(profile()->GetPath()));
                    EXPECT_TRUE(base::PathExists(location.path));
                    path_to_iwa_in_profile = location.path;
                  },
                  [&](const DevModeBundle& location) {
                    // Dev mode bundle should not be copied.
                    EXPECT_EQ(location.path, src_bundle_path_);
                  },
                  [&](const DevModeProxy& location) {}},
              web_app_before->isolation_data()->location);

  // Uninstall the app and check that the copied to profile directory
  // file has been removed.
  ASSERT_NO_FATAL_FAILURE(Uninstall());
  const WebApp* web_app_after =
      provider()->registrar_unsafe().GetAppById(url_info_.app_id());
  ASSERT_FALSE(web_app_after);
  absl::visit(
      base::Overloaded{
          [&](const InstalledBundle&) {
            // Verify that the random directory was removed.
            base::ScopedAllowBlockingForTesting allow_blocking;
            EXPECT_FALSE(base::PathExists(path_to_iwa_in_profile.value()));
            EXPECT_FALSE(
                base::PathExists(path_to_iwa_in_profile.value().DirName()));
          },
          [&](const DevModeBundle&) {
            // Dev mode bundle is not owned by Chrome and should not be removed.
            base::ScopedAllowBlockingForTesting allow_blocking;
            EXPECT_TRUE(base::PathExists(src_bundle_path_));
          },
          [&](const DevModeProxy&) {}},
      src_location_);
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    IsolatedWebAppUninstallBrowserTest,
    ::testing::Bool(),
    [](::testing::TestParamInfo<bool> info) {
      return info.param ? "DevModeBundle" : "InstalledBundle";
    });

}  // namespace
}  // namespace web_app
