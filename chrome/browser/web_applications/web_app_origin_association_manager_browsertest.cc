// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_origin_association_manager.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/commands/update_validated_origin_associations_command.h"
#include "chrome/browser/web_applications/scheduler/update_validated_origin_associations_result.h"
#include "chrome/browser/web_applications/scope_extension_info.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/webapps/services/web_app_origin_association/test/test_web_app_origin_association_fetcher.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

namespace {

const std::string& kWebAppIdentity = "https://foo.com/index";
const std::string& kInvalidFileUrl = "https://a.com";
const std::string& kValidAppUrl = "https://b.com";
const std::string& kValidAndInvalidAppsUrl = "https://c.com/search";
const std::string& kAppWithMultipleMigrationCasesUrl = "https://d.com";

constexpr char kInvalidFileContent[] = "invalid";
constexpr char kValidAppFileContent[] =
    R"({
      "https://foo.com/index": {
      }
    })";
constexpr char kValidAndInvalidAppsFileContent[] =
    R"({
    // 1st app is valid.
      "https://foo.com/index": { "scope": "/search?q=some+text#frag"},
    // 2nd app is invalid since kWebAppIdentity doesn't match.
      "https://bar.com/": {}
    })";
constexpr char kAppWithMultipleMigrationCasesFileContent[] =
    R"({
      "https://foo.com/index_no_migration": {
      },
      "https://foo.com/index_migration_true": {
        "allow_migration": true
      },
      "https://foo.com/index_migration_false": {
        "allow_migration": false
      }
    })";
}  // namespace

namespace web_app {

class WebAppOriginAssociationManagerTest : public WebAppBrowserTestBase {
 public:
  WebAppOriginAssociationManagerTest() {
    manager_ = std::make_unique<WebAppOriginAssociationManager>();
    SetUpFetcher();
    CreateScopeExtensions();
  }

  ~WebAppOriginAssociationManagerTest() override = default;
  void RunTestOnMainThread() override {}
  void TestBody() override {}

  void SetUpFetcher() {
    fetcher_ = std::make_unique<webapps::TestWebAppOriginAssociationFetcher>();
    std::map<url::Origin, std::string> data = {
        {url::Origin::Create(GURL(kInvalidFileUrl)), kInvalidFileContent},
        {url::Origin::Create(GURL(kValidAppUrl)), kValidAppFileContent},
        {url::Origin::Create(GURL(kValidAndInvalidAppsUrl)),
         kValidAndInvalidAppsFileContent},
        {url::Origin::Create(GURL(kAppWithMultipleMigrationCasesUrl)),
         kAppWithMultipleMigrationCasesFileContent},
    };
    fetcher_->SetData(std::move(data));
    manager_->SetFetcherForTest(std::move(fetcher_));
  }

  void CreateScopeExtensions() {
    invalid_file_scope_extension_ = std::make_unique<ScopeExtensionInfo>(
        ScopeExtensionInfo::CreateForOrigin(
            url::Origin::Create(GURL(kInvalidFileUrl))));

    valid_app_scope_extension_ = std::make_unique<ScopeExtensionInfo>(
        ScopeExtensionInfo::CreateForOrigin(
            url::Origin::Create(GURL(kValidAppUrl))));

    valid_and_invalid_app_scope_extension_ =
        std::make_unique<ScopeExtensionInfo>(
            ScopeExtensionInfo::CreateForScope(GURL(kValidAndInvalidAppsUrl)));
  }

  void VerifyValidAndInvalidAppsResult(int expected_callback_count,
                                       base::OnceClosure done_callback,
                                       OriginAssociations result) {
    callback_count_++;
    ASSERT_EQ(result.scope_extensions.size(), 2u);

    auto valid_app_scope_extension =
        ScopeExtensionInfo::CreateForOrigin(valid_app_scope_extension_->origin);
    auto valid_and_invalid_app_scope_extension =
        ScopeExtensionInfo::CreateForScope(
            valid_and_invalid_app_scope_extension_->scope,
            /*has_origin_wildcard*/ valid_and_invalid_app_scope_extension_
                ->has_origin_wildcard);

    EXPECT_TRUE(
        result.scope_extensions.contains(std::move(valid_app_scope_extension)));
    EXPECT_TRUE(result.scope_extensions.contains(
        std::move(valid_and_invalid_app_scope_extension)));

    if (callback_count_ == expected_callback_count) {
      callback_count_ = 0;
      std::move(done_callback).Run();
    }
  }

 protected:
  std::unique_ptr<webapps::TestWebAppOriginAssociationFetcher> fetcher_;
  std::unique_ptr<WebAppOriginAssociationManager> manager_;
  // Number of times the callback function is called.
  int callback_count_ = 0;

  std::unique_ptr<ScopeExtensionInfo> invalid_file_scope_extension_;
  std::unique_ptr<ScopeExtensionInfo> valid_app_scope_extension_;
  std::unique_ptr<ScopeExtensionInfo> valid_and_invalid_app_scope_extension_;
};

IN_PROC_BROWSER_TEST_F(WebAppOriginAssociationManagerTest, NoHandlers) {
  base::test::TestFuture<OriginAssociations> future;
  manager_->GetWebAppOriginAssociations(
      GURL(kWebAppIdentity), OriginAssociations(), future.GetCallback());
  const OriginAssociations result = future.Get<0>();
  ASSERT_TRUE(result.scope_extensions.empty());
  ASSERT_TRUE(result.migration_sources.empty());
}

IN_PROC_BROWSER_TEST_F(WebAppOriginAssociationManagerTest,
                       InvalidAssociationFile) {
  base::test::TestFuture<OriginAssociations> future;
  OriginAssociations origin_associations;
  origin_associations.scope_extensions = {*invalid_file_scope_extension_};
  manager_->GetWebAppOriginAssociations(GURL(kWebAppIdentity),
                                        std::move(origin_associations),
                                        future.GetCallback());
  const OriginAssociations result = future.Get<0>();
  ASSERT_TRUE(result.scope_extensions.empty());
}

IN_PROC_BROWSER_TEST_F(WebAppOriginAssociationManagerTest, OneValidApp) {
  base::test::TestFuture<OriginAssociations> future;
  OriginAssociations origin_associations;
  origin_associations.scope_extensions = {*valid_app_scope_extension_};
  manager_->GetWebAppOriginAssociations(GURL(kWebAppIdentity),
                                        std::move(origin_associations),
                                        future.GetCallback());
  const OriginAssociations result = future.Get<0>();
  ASSERT_TRUE(result.scope_extensions.size() == 1);
  auto scope_extension = std::move(*result.scope_extensions.begin());
  EXPECT_EQ(scope_extension.origin, valid_app_scope_extension_->origin);
  EXPECT_EQ(scope_extension.has_origin_wildcard,
            valid_app_scope_extension_->has_origin_wildcard);
}

IN_PROC_BROWSER_TEST_F(WebAppOriginAssociationManagerTest,
                       ValidAndInvalidApps) {
  base::test::TestFuture<void> future;

  OriginAssociations origin_associations;
  origin_associations.scope_extensions = {
      *valid_app_scope_extension_, *valid_and_invalid_app_scope_extension_};
  callback_count_ = 0;
  manager_->GetWebAppOriginAssociations(
      GURL(kWebAppIdentity), std::move(origin_associations),
      base::BindOnce(
          &WebAppOriginAssociationManagerTest::VerifyValidAndInvalidAppsResult,
          base::Unretained(this), 1, future.GetCallback()));
  EXPECT_TRUE(future.Wait());
}

IN_PROC_BROWSER_TEST_F(WebAppOriginAssociationManagerTest, RunTasks) {
  base::test::TestFuture<void> future;
  OriginAssociations origin_associations;
  origin_associations.scope_extensions = {
      *valid_app_scope_extension_, *valid_and_invalid_app_scope_extension_};

  // Set status as running temporarily to queue up tasks.
  manager_->task_in_progress_ = true;
  int task_count = 6;
  for (int i = 0; i < task_count - 1; i++) {
    manager_->GetWebAppOriginAssociations(
        GURL(kWebAppIdentity), origin_associations,
        base::BindOnce(&WebAppOriginAssociationManagerTest::
                           VerifyValidAndInvalidAppsResult,
                       base::Unretained(this), task_count,
                       future.GetCallback()));
  }
  // Reset to no task in progress to start.
  manager_->task_in_progress_ = false;

  callback_count_ = 0;
  manager_->GetWebAppOriginAssociations(
      GURL(kWebAppIdentity), std::move(origin_associations),
      base::BindOnce(
          &WebAppOriginAssociationManagerTest::VerifyValidAndInvalidAppsResult,
          base::Unretained(this), task_count, future.GetCallback()));
  EXPECT_TRUE(future.Wait());
}

IN_PROC_BROWSER_TEST_F(WebAppOriginAssociationManagerTest,
                       InvalidMigrationSource) {
  base::test::TestFuture<OriginAssociations> future;
  OriginAssociations origin_associations;
  origin_associations.migration_sources.emplace_back(
      webapps::ManifestId(GURL(kInvalidFileUrl)), MigrationBehavior::kSuggest);

  manager_->GetWebAppOriginAssociations(GURL(kWebAppIdentity),
                                        std::move(origin_associations),
                                        future.GetCallback());

  const OriginAssociations result = future.Get<0>();
  ASSERT_TRUE(result.migration_sources.empty());
}

IN_PROC_BROWSER_TEST_F(WebAppOriginAssociationManagerTest,
                       ValidMigrationSource) {
  // WebAppIdentity matching an app with no allow_migration field.
  {
    base::test::TestFuture<OriginAssociations> future;
    OriginAssociations origin_associations;
    origin_associations.migration_sources.emplace_back(
        webapps::ManifestId(GURL(kAppWithMultipleMigrationCasesUrl)),
        MigrationBehavior::kSuggest);
    manager_->GetWebAppOriginAssociations(
        GURL("https://foo.com/index_no_migration"),
        std::move(origin_associations), future.GetCallback());
    const OriginAssociations result = future.Get<0>();
    ASSERT_TRUE(result.migration_sources.empty());
  }

  // WebAppIdentity matching an app with allow_migration: true.
  {
    base::test::TestFuture<OriginAssociations> future;
    OriginAssociations origin_associations;
    origin_associations.migration_sources.emplace_back(
        webapps::ManifestId(GURL(kAppWithMultipleMigrationCasesUrl)),
        MigrationBehavior::kSuggest);
    manager_->GetWebAppOriginAssociations(
        GURL("https://foo.com/index_migration_true"),
        std::move(origin_associations), future.GetCallback());
    const OriginAssociations result = future.Get<0>();
    ASSERT_EQ(result.migration_sources.size(), 1u);
    EXPECT_EQ(result.migration_sources[0].manifest_id().spec(),
              GURL(kAppWithMultipleMigrationCasesUrl).spec());
  }

  // WebAppIdentity matching an app with allow_migration: false.
  {
    base::test::TestFuture<OriginAssociations> future;
    OriginAssociations origin_associations;
    web_app::proto::WebAppMigrationSource migration_source;
    migration_source.set_manifest_id(kAppWithMultipleMigrationCasesUrl);
    migration_source.set_behavior(
        web_app::proto::WEB_APP_MIGRATION_BEHAVIOR_SUGGEST);
    origin_associations.migration_sources.emplace_back(
        webapps::ManifestId(GURL(kAppWithMultipleMigrationCasesUrl)),
        MigrationBehavior::kSuggest);
    manager_->GetWebAppOriginAssociations(
        GURL("https://foo.com/index_migration_false"),
        std::move(origin_associations), future.GetCallback());
    const OriginAssociations result = future.Get<0>();
    ASSERT_TRUE(result.migration_sources.empty());
  }
}

IN_PROC_BROWSER_TEST_F(WebAppOriginAssociationManagerTest,
                       SameOriginMigrationAllowedWithoutFetch) {
  base::test::TestFuture<OriginAssociations> future;
  OriginAssociations origin_associations;
  web_app::proto::WebAppMigrationSource migration_source;
  // Use same origin as kWebAppIdentity ("https://foo.com/index")
  std::string same_origin_manifest_id = "https://foo.com/another_app";
  migration_source.set_manifest_id(same_origin_manifest_id);
  migration_source.set_behavior(
      web_app::proto::WEB_APP_MIGRATION_BEHAVIOR_SUGGEST);
  origin_associations.migration_sources.emplace_back(
      webapps::ManifestId(GURL(same_origin_manifest_id)),
      MigrationBehavior::kSuggest);

  // The fetcher's data does NOT include any entry for foo.com.
  // GetWebAppOriginAssociations should allow this without fetching.
  manager_->GetWebAppOriginAssociations(GURL(kWebAppIdentity),
                                        std::move(origin_associations),
                                        future.GetCallback());

  const OriginAssociations result = future.Get<0>();
  ASSERT_EQ(result.migration_sources.size(), 1u);
  EXPECT_EQ(result.migration_sources[0].manifest_id().spec(),
            same_origin_manifest_id);
}

class WebAppOriginAssociationManagerRevocationTest
    : public WebAppBrowserTestBase {
 public:
  WebAppOriginAssociationManagerRevocationTest() = default;
  ~WebAppOriginAssociationManagerRevocationTest() override = default;

  void SetUpOnMainThread() override {
    WebAppBrowserTestBase::SetUpOnMainThread();

    auto fetcher =
        std::make_unique<webapps::TestWebAppOriginAssociationFetcher>();
    fetcher_ = fetcher.get();
    provider().origin_association_manager().SetFetcherForTest(
        std::move(fetcher));
  }

  void TearDownOnMainThread() override {
    fetcher_ = nullptr;
    WebAppBrowserTestBase::TearDownOnMainThread();
  }

 protected:
  WebAppProvider& provider() { return *WebAppProvider::GetForTest(profile()); }

  webapps::AppId InstallScopeExtendedWebApp(
      const GURL& start_url,
      const ScopeExtensions& scope_extensions) {
    auto info = WebAppInstallInfo::CreateWithStartUrlForTesting(start_url);
    info->title = u"Test App";
    info->scope_extensions = scope_extensions;
    return test::InstallWebApp(profile(), std::move(info));
  }

  webapps::AppId InstallMigrationSourceWebApp(
      const GURL& start_url,
      const std::vector<MigrationSource>& migration_sources) {
    auto info = WebAppInstallInfo::CreateWithStartUrlForTesting(start_url);
    info->title = u"Test App";
    info->migration_sources = migration_sources;
    return test::InstallWebApp(profile(), std::move(info));
  }

  raw_ptr<webapps::TestWebAppOriginAssociationFetcher> fetcher_ = nullptr;
  base::test::ScopedFeatureList scoped_feature_list_{
      blink::features::kWebAppMigrationApi};
};

IN_PROC_BROWSER_TEST_F(WebAppOriginAssociationManagerRevocationTest,
                       ScopeExtensionRevocationDetectedOnLaunch) {
  GURL start_url("https://example.com/");
  url::Origin extended_origin =
      url::Origin::Create(GURL("https://example.org/"));
  ScopeExtensionInfo extension =
      ScopeExtensionInfo::CreateForOrigin(extended_origin);

  // 1. Mock the association file on the extended origin.
  // Key is the target PWA's manifest ID (https://example.com/).
  std::string association_content = R"({
    "https://example.com/" : {
      "scope": "https://example.org/scope"
    }
  })";
  fetcher_->SetData({{extended_origin, association_content}});

  // 2. Install the PWA with the scope extension.
  // During install, it will fetch the mock association and validate it.
  webapps::AppId app_id = InstallScopeExtendedWebApp(start_url, {extension});

  // Verify that it was successfully validated.
  {
    const WebApp* app = provider().registrar_unsafe().GetAppById(app_id);
    ASSERT_TRUE(app);
    EXPECT_FALSE(app->validated_scope_extensions().empty());
    EXPECT_EQ(app->validated_scope_extensions().begin()->scope,
              GURL("https://example.org/scope"));
  }

  // 3. Simulate revocation by clearing the mock fetcher data.
  fetcher_->SetData({});

  // 4. Bypass the 10-day throttling by setting check time to 11 days ago.
  {
    ScopedRegistryUpdate update = provider().sync_bridge_unsafe().BeginUpdate();
    WebApp* app_to_update = update->UpdateApp(app_id);
    app_to_update->SetOriginAssociationLastValidationCheckTime(
        base::Time::Now() - base::Days(2));
  }

  // 5. Launch the web app in a window. This triggers the launch flow
  // (LaunchWebAppCommand), which in turn schedules the revalidation command.
  Browser* app_browser = LaunchWebAppBrowser(app_id);
  ASSERT_TRUE(app_browser);

  // Await the completion of all scheduled commands.
  provider().command_manager().AwaitAllCommandsCompleteForTesting();

  // 6. Verify that validated scope extensions are now empty (revocation
  // successful!).
  const WebApp* app = provider().registrar_unsafe().GetAppById(app_id);
  EXPECT_TRUE(app->validated_scope_extensions().empty());
}

IN_PROC_BROWSER_TEST_F(WebAppOriginAssociationManagerRevocationTest,
                       MigrationRevocationDetectedOnLaunch) {
  GURL start_url("https://example.com/");
  webapps::ManifestId migration_source_id(
      GURL("https://migration.example.com/manifest.json"));
  url::Origin migration_source_origin =
      url::Origin::Create(migration_source_id.value());
  MigrationSource migration_source(migration_source_id,
                                   MigrationBehavior::kForce);

  // 1. Mock the association file on the migration source origin.
  // Key is the target PWA's manifest ID (https://example.com/).
  std::string association_content = R"({
    "https://example.com/" : {
      "allow_migration": true
    }
  })";
  fetcher_->SetData({{migration_source_origin, association_content}});

  // 2. Install PWA with a migration source.
  // During install, it will fetch the mock association and validate it.
  webapps::AppId app_id =
      InstallMigrationSourceWebApp(start_url, {migration_source});

  // Verify that it was successfully validated.
  {
    const WebApp* app = provider().registrar_unsafe().GetAppById(app_id);
    ASSERT_TRUE(app);
    EXPECT_FALSE(app->validated_migration_sources().empty());
    EXPECT_EQ(app->validated_migration_sources().begin()->manifest_id(),
              migration_source_id);
  }

  // 3. Simulate revocation by clearing the mock fetcher data.
  fetcher_->SetData({});

  // 4. Bypass the 10-day throttling by setting check time to 11 days ago.
  {
    ScopedRegistryUpdate update = provider().sync_bridge_unsafe().BeginUpdate();
    WebApp* app_to_update = update->UpdateApp(app_id);
    app_to_update->SetOriginAssociationLastValidationCheckTime(
        base::Time::Now() - base::Days(2));
  }

  // 5. Launch the web app in a window. This triggers the launch flow
  // (LaunchWebAppCommand), which in turn schedules the revalidation command.
  Browser* app_browser = LaunchWebAppBrowser(app_id);
  ASSERT_TRUE(app_browser);

  // Await the completion of all scheduled commands.
  provider().command_manager().AwaitAllCommandsCompleteForTesting();

  // 6. Verify that validated migration sources are now empty (revocation
  // successful!).
  const WebApp* app = provider().registrar_unsafe().GetAppById(app_id);
  EXPECT_TRUE(app->validated_migration_sources().empty());
}

}  // namespace web_app
