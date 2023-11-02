// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_origin_association_manager.h"

#include "base/containers/contains.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/webapps/services/web_app_origin_association/test/test_web_app_origin_association_fetcher.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

namespace {

const std::string& kManifestUrl = "https://foo.com/manifest.json";
const std::string& kInvalidFileUrl = "https://a.com";
const std::string& kValidAppUrl = "https://b.com";
const std::string& kValidAndInvalidAppsUrl = "https://c.com";
const std::string& kMultipleValidAppsUrl = "https://d.com";
const std::string& kValidAppWithTooManyPathsUrl = "https://e.com";
const std::string& kValidAppWithDuplicatePathsUrl = "https://f.com";

constexpr char kInvalidFileContent[] = "invalid";
constexpr char kValidAppFileContent[] =
    "{\"web_apps\": ["
    "  {"
    "    \"manifest\": \"https://foo.com/manifest.json\","
    "    \"details\": {"
    "      \"paths\": [\"/*\"],"
    "      \"exclude_paths\": [\"/blog/data\"]"
    "    }"
    "  }"
    "]}";
constexpr char kValidAndInvalidAppsFileContent[] =
    "{\"web_apps\": ["
    // 1st app is valid since manifest url matches
    "  {"
    "    \"manifest\": \"https://foo.com/manifest.json\""
    "  },"
    // 2nd app is invalid since manifest url doesn't match
    "  {"
    "    \"manifest\": \"https://bar.com/manifest.json\","
    "    \"details\": {"
    "      \"paths\": [\"/*\"],"
    "      \"exclude_paths\": [\"/blog/data\"]"
    "    }"
    "  }"
    "]}";
constexpr char kMultipleValidAppsFileContent[] =
    "{\"web_apps\": ["
    // 1st app is valid since manifest url matches
    "  {"
    "    \"manifest\": \"https://foo.com/manifest.json\""
    "  },"
    // 2nd app is also valid
    "  {"
    "    \"manifest\": \"https://foo.com/manifest.json\","
    "    \"details\": {"
    "      \"paths\": [\"/*\"],"
    "      \"exclude_paths\": [\"/blog/data\"]"
    "    }"
    "  }"
    "]}";

constexpr char kValidAppWithTooManyPathsFileContent[] =
    "{\"web_apps\": ["
    "  {"
    "    \"manifest\": \"https://foo.com/manifest.json\","
    "    \"details\": {"
    "      \"paths\": [\"/1\", \"/2\", \"/3\", \"/4\", \"/5\", \"/6\","
    "                  \"/7\", \"/8\", \"/9\", \"/10\", \"/11\"],"
    "      \"exclude_paths\": [\"/1\", \"/2\", \"/3\", \"/4\", \"/5\","
    "                          \"/6\", \"/7\", \"/8\", \"/9\", \"/10\","
    "                          \"/11\"]"
    "    }"
    "  }"
    "]}";

constexpr char kValidAppWithDuplicatePathsFileContent[] =
    "{\"web_apps\": ["
    "  {"
    "    \"manifest\": \"https://foo.com/manifest.json\","
    "    \"details\": {"
    "      \"paths\": [\"/1\", \"/1\", \"/1\"],"
    "      \"exclude_paths\": [\"/2\", \"/2\", \"/2\"]"
    "    }"
    "  }"
    "]}";

}  // namespace

namespace web_app {

class WebAppOriginAssociationManagerTest : public InProcessBrowserTest {
 public:
  WebAppOriginAssociationManagerTest() {
    manager_ = std::make_unique<WebAppOriginAssociationManager>();
    scoped_feature_list_.InitAndEnableFeature(
        blink::features::kWebAppEnableUrlHandlers);
    SetUpFetcher();
    CreateUrlHandlers();
  }

  ~WebAppOriginAssociationManagerTest() override = default;
  void RunTestOnMainThread() override {}
  void TestBody() override {}

  void SetUpFetcher() {
    fetcher_ = std::make_unique<webapps::TestWebAppOriginAssociationFetcher>();
    std::map<url::Origin, std::string> data = {
        {url::Origin::Create(GURL(kInvalidFileUrl)), kInvalidFileContent},
        {url::Origin::Create(GURL(kValidAppUrl)), kValidAppFileContent},
        {url::Origin::Create(GURL(kValidAppWithTooManyPathsUrl)),
         kValidAppWithTooManyPathsFileContent},
        {url::Origin::Create(GURL(kValidAppWithDuplicatePathsUrl)),
         kValidAppWithDuplicatePathsFileContent},
        {url::Origin::Create(GURL(kValidAndInvalidAppsUrl)),
         kValidAndInvalidAppsFileContent},
        {url::Origin::Create(GURL(kMultipleValidAppsUrl)),
         kMultipleValidAppsFileContent},
    };
    fetcher_->SetData(std::move(data));
    manager_->SetFetcherForTest(std::move(fetcher_));
  }

  void CreateUrlHandlers() {
    invalid_file_url_handler_.origin =
        url::Origin::Create(GURL(kInvalidFileUrl));
    valid_app_url_handler_.origin = url::Origin::Create(GURL(kValidAppUrl));
    valid_app_with_too_many_paths_url_handler_.origin =
        url::Origin::Create(GURL(kValidAppWithTooManyPathsUrl));
    valid_app_with_duplicate_paths_url_handler_.origin =
        url::Origin::Create(GURL(kValidAppWithDuplicatePathsUrl));
    valid_and_invalid_app_url_handler_.origin =
        url::Origin::Create(GURL(kValidAndInvalidAppsUrl));
    multiple_valid_apps_url_handler_.origin =
        url::Origin::Create(GURL(kMultipleValidAppsUrl));
  }

  void VerifyValidAndInvalidAppsResult(int expected_callback_count,
                                       base::OnceClosure done_callback,
                                       apps::UrlHandlers result) {
    callback_count_++;
    ASSERT_EQ(result.size(), 2u);

    apps::UrlHandlerInfo valid_app_url_handler(
        valid_app_url_handler_.origin,
        valid_app_url_handler_.has_origin_wildcard, {"/*"}, {"/blog/data"});
    apps::UrlHandlerInfo valid_and_invalid_app_url_handler(
        valid_and_invalid_app_url_handler_.origin,
        valid_and_invalid_app_url_handler_.has_origin_wildcard, {}, {});

    EXPECT_TRUE(base::Contains(result, std::move(valid_app_url_handler)));
    EXPECT_TRUE(
        base::Contains(result, std::move(valid_and_invalid_app_url_handler)));

    if (callback_count_ == expected_callback_count) {
      callback_count_ = 0;
      std::move(done_callback).Run();
    }
  }

 protected:
  std::unique_ptr<webapps::TestWebAppOriginAssociationFetcher> fetcher_;
  std::unique_ptr<WebAppOriginAssociationManager> manager_;
  base::test::ScopedFeatureList scoped_feature_list_;
  // Number of times the callback function is called.
  int callback_count_ = 0;

  apps::UrlHandlerInfo invalid_file_url_handler_;
  apps::UrlHandlerInfo valid_app_url_handler_;
  apps::UrlHandlerInfo valid_app_with_too_many_paths_url_handler_;
  apps::UrlHandlerInfo valid_app_with_duplicate_paths_url_handler_;
  apps::UrlHandlerInfo valid_and_invalid_app_url_handler_;
  apps::UrlHandlerInfo multiple_valid_apps_url_handler_;
};

IN_PROC_BROWSER_TEST_F(WebAppOriginAssociationManagerTest, NoHandlers) {
  base::RunLoop run_loop;
  manager_->GetWebAppOriginAssociations(
      GURL(kManifestUrl), apps::UrlHandlers(),
      base::BindLambdaForTesting([&](apps::UrlHandlers result) {
        ASSERT_TRUE(result.empty());
        run_loop.Quit();
      }));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(WebAppOriginAssociationManagerTest,
                       InvalidAssociationFile) {
  base::RunLoop run_loop;
  apps::UrlHandlers url_handlers{std::move(invalid_file_url_handler_)};
  manager_->GetWebAppOriginAssociations(
      GURL(kManifestUrl), std::move(url_handlers),
      base::BindLambdaForTesting([&](apps::UrlHandlers result) {
        ASSERT_TRUE(result.empty());
        run_loop.Quit();
      }));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(WebAppOriginAssociationManagerTest, OneValidApp) {
  base::RunLoop run_loop;
  apps::UrlHandlers url_handlers{valid_app_url_handler_};
  manager_->GetWebAppOriginAssociations(
      GURL(kManifestUrl), std::move(url_handlers),
      base::BindLambdaForTesting([&](apps::UrlHandlers result) {
        ASSERT_TRUE(result.size() == 1);
        auto url_handler = std::move(result[0]);
        EXPECT_EQ(url_handler.origin, valid_app_url_handler_.origin);
        EXPECT_EQ(url_handler.has_origin_wildcard,
                  valid_app_url_handler_.has_origin_wildcard);

        ASSERT_EQ(1u, url_handler.paths.size());
        EXPECT_EQ(url_handler.paths[0], "/*");

        ASSERT_EQ(1u, url_handler.exclude_paths.size());
        EXPECT_EQ(url_handler.exclude_paths[0], "/blog/data");
        run_loop.Quit();
      }));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(WebAppOriginAssociationManagerTest,
                       OneValidAppWithTooManyPaths) {
  base::RunLoop run_loop;
  apps::UrlHandlers url_handlers{valid_app_with_too_many_paths_url_handler_};
  manager_->GetWebAppOriginAssociations(
      GURL(kManifestUrl), std::move(url_handlers),
      base::BindLambdaForTesting([&](apps::UrlHandlers result) {
        ASSERT_TRUE(result.size() == 1);
        auto url_handler = std::move(result[0]);
        EXPECT_EQ(url_handler.origin,
                  valid_app_with_too_many_paths_url_handler_.origin);
        EXPECT_EQ(
            url_handler.has_origin_wildcard,
            valid_app_with_too_many_paths_url_handler_.has_origin_wildcard);

        ASSERT_EQ(10u, url_handler.paths.size());
        ASSERT_EQ(10u, url_handler.exclude_paths.size());
        run_loop.Quit();
      }));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(WebAppOriginAssociationManagerTest,
                       OneValidAppWithDuplicatePaths) {
  base::RunLoop run_loop;
  apps::UrlHandlers url_handlers{valid_app_with_duplicate_paths_url_handler_};
  manager_->GetWebAppOriginAssociations(
      GURL(kManifestUrl), std::move(url_handlers),
      base::BindLambdaForTesting([&](apps::UrlHandlers result) {
        ASSERT_TRUE(result.size() == 1);
        auto url_handler = std::move(result[0]);
        EXPECT_EQ(url_handler.origin,
                  valid_app_with_duplicate_paths_url_handler_.origin);
        EXPECT_EQ(
            url_handler.has_origin_wildcard,
            valid_app_with_duplicate_paths_url_handler_.has_origin_wildcard);

        // Check that paths and exclude_paths have been deduplicated.
        ASSERT_EQ(1u, url_handler.paths.size());
        EXPECT_EQ(url_handler.paths[0], "/1");
        ASSERT_EQ(1u, url_handler.exclude_paths.size());
        EXPECT_EQ(url_handler.exclude_paths[0], "/2");
        run_loop.Quit();
      }));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(WebAppOriginAssociationManagerTest,
                       ValidAndInvalidApps) {
  base::RunLoop run_loop;

  apps::UrlHandlers url_handlers{valid_app_url_handler_,
                                 valid_and_invalid_app_url_handler_};
  callback_count_ = 0;
  manager_->GetWebAppOriginAssociations(
      GURL(kManifestUrl), std::move(url_handlers),
      base::BindOnce(
          &WebAppOriginAssociationManagerTest::VerifyValidAndInvalidAppsResult,
          base::Unretained(this), 1, run_loop.QuitClosure()));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(WebAppOriginAssociationManagerTest, MultipleValidApps) {
  base::RunLoop run_loop;
  apps::UrlHandlers url_handlers{multiple_valid_apps_url_handler_};
  manager_->GetWebAppOriginAssociations(
      GURL(kManifestUrl), std::move(url_handlers),
      base::BindLambdaForTesting([&](apps::UrlHandlers result) {
        ASSERT_TRUE(result.size() == 1);
        auto url_handler = std::move(result[0]);
        EXPECT_EQ(url_handler.origin, multiple_valid_apps_url_handler_.origin);
        EXPECT_EQ(url_handler.has_origin_wildcard,
                  multiple_valid_apps_url_handler_.has_origin_wildcard);

        ASSERT_TRUE(url_handler.paths.empty());
        ASSERT_TRUE(url_handler.exclude_paths.empty());
        run_loop.Quit();
      }));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(WebAppOriginAssociationManagerTest, RunTasks) {
  base::RunLoop run_loop;
  apps::UrlHandlers url_handlers{valid_app_url_handler_,
                                 valid_and_invalid_app_url_handler_};

  GURL manifest_url(kManifestUrl);
  // Set status as running temporarily to queue up tasks.
  manager_->task_in_progress_ = true;
  int task_count = 6;
  for (int i = 0; i < task_count - 1; i++) {
    manager_->GetWebAppOriginAssociations(
        manifest_url, url_handlers,
        base::BindOnce(&WebAppOriginAssociationManagerTest::
                           VerifyValidAndInvalidAppsResult,
                       base::Unretained(this), task_count,
                       run_loop.QuitClosure()));
  }
  // Reset to no task in progress to start.
  manager_->task_in_progress_ = false;

  callback_count_ = 0;
  manager_->GetWebAppOriginAssociations(
      manifest_url, std::move(url_handlers),
      base::BindOnce(
          &WebAppOriginAssociationManagerTest::VerifyValidAndInvalidAppsResult,
          base::Unretained(this), task_count, run_loop.QuitClosure()));
  run_loop.Run();
}

}  // namespace web_app
