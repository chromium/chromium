// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_origin_association_manager.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/proto/web_app.pb.h"
#include "chrome/browser/web_applications/scope_extension_info.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/webapps/services/web_app_origin_association/test/test_web_app_origin_association_fetcher.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
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
  web_app::proto::WebAppMigrationSource migration_source;
  migration_source.set_manifest_id(kInvalidFileUrl);
  migration_source.set_behavior(
      web_app::proto::WEB_APP_MIGRATION_BEHAVIOR_SUGGEST);
  origin_associations.migration_sources.push_back(std::move(migration_source));

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
    web_app::proto::WebAppMigrationSource migration_source;
    migration_source.set_manifest_id(kAppWithMultipleMigrationCasesUrl);
    migration_source.set_behavior(
        web_app::proto::WEB_APP_MIGRATION_BEHAVIOR_SUGGEST);
    origin_associations.migration_sources.push_back(
        std::move(migration_source));
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
    web_app::proto::WebAppMigrationSource migration_source;
    migration_source.set_manifest_id(kAppWithMultipleMigrationCasesUrl);
    migration_source.set_behavior(
        web_app::proto::WEB_APP_MIGRATION_BEHAVIOR_SUGGEST);
    origin_associations.migration_sources.push_back(
        std::move(migration_source));
    manager_->GetWebAppOriginAssociations(
        GURL("https://foo.com/index_migration_true"),
        std::move(origin_associations), future.GetCallback());
    const OriginAssociations result = future.Get<0>();
    ASSERT_EQ(result.migration_sources.size(), 1u);
    EXPECT_EQ(GURL(result.migration_sources[0].manifest_id()).spec(),
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
    origin_associations.migration_sources.push_back(
        std::move(migration_source));
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
  origin_associations.migration_sources.push_back(std::move(migration_source));

  // The fetcher's data does NOT include any entry for foo.com.
  // GetWebAppOriginAssociations should allow this without fetching.
  manager_->GetWebAppOriginAssociations(GURL(kWebAppIdentity),
                                        std::move(origin_associations),
                                        future.GetCallback());

  const OriginAssociations result = future.Get<0>();
  ASSERT_EQ(result.migration_sources.size(), 1u);
  EXPECT_EQ(GURL(result.migration_sources[0].manifest_id()).spec(),
            same_origin_manifest_id);
}

}  // namespace web_app
