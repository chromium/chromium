// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_update_manager.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_builder.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_location.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_trust_checker.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_policy_constants.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {
namespace {

using base::test::HasValue;
using ::testing::_;
using ::testing::Eq;
using ::testing::Optional;
using ::testing::VariantWith;

constexpr base::StringPiece kUpdateManifestFileName = "update_manifest.json";
constexpr base::StringPiece kBundle304FileName = "bundle304.swbn";
constexpr base::StringPiece kBundle706FileName = "bundle706.swbn";

class IsolatedWebAppUpdateManagerBrowserTest
    : public IsolatedWebAppBrowserTestHarness {
 protected:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        features::kIsolatedWebAppAutomaticUpdates);
    SetTrustedWebBundleIdsForTesting({url_info_.web_bundle_id()});
    SetUpFilesAndServer();

    IsolatedWebAppBrowserTestHarness::SetUp();
  }

  void SetUpFilesAndServer() {
    TestSignedWebBundle bundle304 = TestSignedWebBundleBuilder::BuildDefault(
        TestSignedWebBundleBuilder::BuildOptions()
            .SetAppName("app-3.0.4")
            .SetVersion(base::Version("3.0.4")));
    TestSignedWebBundle bundle706 = TestSignedWebBundleBuilder::BuildDefault(
        TestSignedWebBundleBuilder::BuildOptions()
            .SetAppName("app-7.0.6")
            .SetVersion(base::Version("7.0.6")));

    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    iwa_server_.ServeFilesFromDirectory(temp_dir_.GetPath());
    EXPECT_TRUE(iwa_server_.Start());

    EXPECT_TRUE(base::WriteFile(temp_dir_.GetPath().Append(kBundle304FileName),
                                bundle304.data));
    EXPECT_TRUE(base::WriteFile(temp_dir_.GetPath().Append(kBundle706FileName),
                                bundle706.data));
    EXPECT_TRUE(base::WriteFile(
        temp_dir_.GetPath().Append(kUpdateManifestFileName),
        base::ReplaceStringPlaceholders(
            R"(
              {
                "versions": [
                  {"version": "3.0.4", "src": "$1"},
                  {"version": "7.0.6", "src": "$2"}
                ]
              }
            )",
            {iwa_server_.GetURL(base::StrCat({"/", kBundle304FileName})).spec(),
             iwa_server_.GetURL(base::StrCat({"/", kBundle706FileName}))
                 .spec()},
            /*offsets=*/nullptr)));
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  IsolatedWebAppUrlInfo url_info_ =
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
          *web_package::SignedWebBundleId::Create(kTestEd25519WebBundleId));
  base::ScopedTempDir temp_dir_;
  net::EmbeddedTestServer iwa_server_;
};

IN_PROC_BROWSER_TEST_F(IsolatedWebAppUpdateManagerBrowserTest, Succeeds) {
  profile()->GetPrefs()->SetList(
      prefs::kIsolatedWebAppInstallForceList,
      base::Value::List().Append(
          base::Value::Dict()
              .Set(kPolicyWebBundleIdKey, url_info_.web_bundle_id().id())
              .Set(kPolicyUpdateManifestUrlKey,
                   iwa_server_
                       .GetURL(base::StrCat({"/", kUpdateManifestFileName}))
                       .spec())));

  {
    base::test::TestFuture<base::expected<InstallIsolatedWebAppCommandSuccess,
                                          InstallIsolatedWebAppCommandError>>
        future;
    provider().scheduler().InstallIsolatedWebApp(
        url_info_,
        InstalledBundle{.path = temp_dir_.GetPath().Append(kBundle304FileName)},
        base::Version("3.0.4"), /*optional_keep_alive=*/nullptr,
        /*optional_profile_keep_alive=*/nullptr, future.GetCallback());
    EXPECT_THAT(future.Take(), HasValue());
  }

  WebAppTestManifestUpdatedObserver observer(&provider().install_manager());
  observer.BeginListening({url_info_.app_id()});

  provider().iwa_update_manager().DiscoverUpdatesNowForTesting();
  observer.Wait();

  const WebApp* web_app =
      provider().registrar_unsafe().GetAppById(url_info_.app_id());
  EXPECT_THAT(web_app,
              test::IwaIs(Eq("app-7.0.6"),
                          test::IsolationDataIs(
                              VariantWith<InstalledBundle>(_),
                              Eq(base::Version("7.0.6")),
                              /*controlled_frame_partitions=*/_,
                              /*pending_update_info=*/Eq(absl::nullopt))));
}

}  // namespace
}  // namespace web_app
