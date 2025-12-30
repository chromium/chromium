// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/iwa_permissions_policy_cache.h"

#include <optional>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/location.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/commands/isolated_web_app_apply_update_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/commands/isolated_web_app_prepare_and_store_update_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/install/isolated_web_app_install_source.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/webapps/isolated_web_apps/test_support/signing_keys.h"
#include "components/webapps/isolated_web_apps/types/iwa_origin.h"
#include "components/webapps/isolated_web_apps/types/source.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {

namespace {
using testing::IsEmpty;
using testing::IsNull;
using testing::NotNull;
using testing::Pointee;
using PrepareAndStoreUpdateResult =
    IsolatedWebAppUpdatePrepareAndStoreCommandResult;
using ApplyUpdateResult = IsolatedWebAppApplyUpdateCommandResult;
}  // namespace

class IwaPermissionsPolicyCacheBrowserTest
    : public IsolatedWebAppBrowserTestHarness {
 protected:
  IwaPermissionsPolicyCache* GetCache() {
    return IwaPermissionsPolicyCacheFactory::GetForProfile(profile());
  }

  void UninstallIwa(const webapps::AppId& app_id) {
    base::test::TestFuture<webapps::UninstallResultCode> future;
    provider().scheduler().RemoveUserUninstallableManagements(
        app_id, webapps::WebappUninstallSource::kAppsPage,
        future.GetCallback());
    EXPECT_TRUE(future.Wait());
  }

  PrepareAndStoreUpdateResult PrepareAndStoreUpdate(
      const web_package::SignedWebBundleId& web_bundle_id,
      const base::FilePath& update_bundle_path,
      const IwaVersion& update_version) {
    base::test::TestFuture<PrepareAndStoreUpdateResult> future;
    provider().scheduler().PrepareAndStoreIsolatedWebAppUpdate(
        IsolatedWebAppUpdatePrepareAndStoreCommand::UpdateInfo(
            IwaSourceBundleWithModeAndFileOp(
                update_bundle_path,
                IwaSourceBundleModeAndFileOp::kProdModeCopy),
            update_version),
        IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(web_bundle_id),
        /*optional_keep_alive=*/nullptr,
        /*optional_profile_keep_alive=*/nullptr, future.GetCallback());
    return future.Take();
  }

  ApplyUpdateResult ApplyUpdate(
      const web_package::SignedWebBundleId& web_bundle_id) {
    base::test::TestFuture<ApplyUpdateResult> future;
    provider().scheduler().ApplyPendingIsolatedWebAppUpdate(
        IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(web_bundle_id),
        /*optional_keep_alive=*/nullptr,
        /*optional_profile_keep_alive=*/nullptr, future.GetCallback());
    return future.Take();
  }
};

IN_PROC_BROWSER_TEST_F(IwaPermissionsPolicyCacheBrowserTest,
                       UninstallClearsCache) {
  auto app = IsolatedWebAppBuilder(ManifestBuilder())
                 .BuildBundle(web_app::test::GetDefaultEd25519WebBundleId(),
                              {web_app::test::GetDefaultEd25519KeyPair()});
  const auto url_info = app->InstallChecked(profile());

  IwaPermissionsPolicyCache* cache = GetCache();
  ASSERT_TRUE(cache);

  const std::vector<IwaPermissionsPolicyCache::Entry> policy = {
      IwaPermissionsPolicyCache::Entry("camera", {})};
  const IwaOrigin iwa_origin =
      IwaOrigin::Create(url_info.origin().GetURL()).value();
  cache->SetPolicy(iwa_origin, policy);

  ASSERT_TRUE(cache->GetPolicy(iwa_origin));

  UninstallIwa(url_info.app_id());

  EXPECT_FALSE(cache->GetPolicy(iwa_origin));
}

IN_PROC_BROWSER_TEST_F(IwaPermissionsPolicyCacheBrowserTest,
                       UpdateClearsCache) {
  const auto bundle_id = web_app::test::GetDefaultEd25519WebBundleId();
  const auto key_pair = web_app::test::GetDefaultEd25519KeyPair();

  auto app_v1 = IsolatedWebAppBuilder(ManifestBuilder().SetVersion("1.0.0"))
                    .BuildBundle(bundle_id, {key_pair});
  const auto url_info = app_v1->InstallChecked(profile());

  IwaPermissionsPolicyCache* cache = GetCache();
  ASSERT_TRUE(cache);

  const std::vector<IwaPermissionsPolicyCache::Entry> policy = {
      IwaPermissionsPolicyCache::Entry("camera", {})};
  const IwaOrigin iwa_origin =
      IwaOrigin::Create(url_info.origin().GetURL()).value();
  cache->SetPolicy(iwa_origin, policy);

  ASSERT_TRUE(cache->GetPolicy(iwa_origin));

  const auto app_v2 =
      IsolatedWebAppBuilder(ManifestBuilder().SetVersion("2.0.0"))
          .BuildBundle(bundle_id, {key_pair});

  ASSERT_TRUE(
      PrepareAndStoreUpdate(bundle_id, app_v2->path(), app_v2->version())
          .has_value());

  ASSERT_TRUE(ApplyUpdate(bundle_id).has_value());

  EXPECT_FALSE(cache->GetPolicy(iwa_origin));
}

IN_PROC_BROWSER_TEST_F(IwaPermissionsPolicyCacheBrowserTest,
                       ImmediatelyReturnsIfCached) {
  auto app = IsolatedWebAppBuilder(ManifestBuilder())
                 .BuildBundle(web_app::test::GetDefaultEd25519WebBundleId(),
                              {web_app::test::GetDefaultEd25519KeyPair()});
  const auto url_info = app->InstallChecked(profile());

  IwaPermissionsPolicyCache* cache = GetCache();
  ASSERT_TRUE(cache);

  const IwaOrigin iwa_origin =
      IwaOrigin::Create(url_info.origin().GetURL()).value();
  // Set fake policy not in line with the manifest (by default, manifest
  // contains also cross-origin-isolated policy).
  cache->SetPolicy(iwa_origin, {});

  ASSERT_TRUE(cache->GetPolicy(iwa_origin));

  base::test::TestFuture<bool> future;
  GetCache()->ObtainManifestAndCache(iwa_origin, future.GetCallback());

  ASSERT_TRUE(future.Get());
  // Since cache is populated and nothing invalidated it, manifest should not be
  // re-fetched and function should immediately query a response.
  // Hence, fake policy still should be here.
  EXPECT_THAT(GetCache()->GetPolicy(iwa_origin), Pointee(IsEmpty()));
}

IN_PROC_BROWSER_TEST_F(IwaPermissionsPolicyCacheBrowserTest,
                       SendsRequestIfNotCached) {
  auto app = IsolatedWebAppBuilder(ManifestBuilder())
                 .BuildBundle(web_app::test::GetDefaultEd25519WebBundleId(),
                              {web_app::test::GetDefaultEd25519KeyPair()});
  const auto url_info = app->InstallChecked(profile());

  IwaPermissionsPolicyCache* cache = GetCache();
  ASSERT_TRUE(cache);

  const IwaOrigin iwa_origin =
      IwaOrigin::Create(url_info.origin().GetURL()).value();
  ASSERT_FALSE(cache->GetPolicy(iwa_origin));

  base::test::TestFuture<bool> future;
  GetCache()->ObtainManifestAndCache(iwa_origin, future.GetCallback());

  ASSERT_TRUE(future.Get());
  EXPECT_THAT(GetCache()->GetPolicy(iwa_origin), NotNull());
}

IN_PROC_BROWSER_TEST_F(IwaPermissionsPolicyCacheBrowserTest,
                       ObtainManifestForNonexistentApp) {
  auto web_bundle_id =
      web_package::SignedWebBundleId::CreateRandomForProxyMode();
  IwaOrigin iwa_origin(web_bundle_id);

  base::test::TestFuture<bool> future;
  GetCache()->ObtainManifestAndCache(iwa_origin, future.GetCallback());

  ASSERT_TRUE(future.Get());
  EXPECT_THAT(GetCache()->GetPolicy(iwa_origin), IsNull());
}

}  // namespace web_app
