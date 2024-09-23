// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_origin_association_manager.h"

#include "base/containers/contains.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/webapps/services/web_app_origin_association/test/test_web_app_origin_association_fetcher.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

const std::string& kWebAppIdentity = "https://foo.com/index";
const std::string& kInvalidFileUrl = "https://a.com";
const std::string& kValidAppUrl = "https://b.com";
const std::string& kValidAndInvalidAppsUrl = "https://c.com";

constexpr char kInvalidFileContent[] = "invalid";
constexpr char kValidAppFileContent[] =
    "{\"web_apps\": ["
    "  {"
    "    \"web_app_identity\": \"https://foo.com/index\""
    "  }"
    "]}";
constexpr char kValidAndInvalidAppsFileContent[] =
    "{\"web_apps\": ["
    // 1st app is valid.
    "  {"
    "    \"web_app_identity\": \"https://foo.com/index\""
    "  },"
    // 2nd app is invalid since kWebAppIdentity doesn't match.
    "  {"
    "    \"web_app_identity\": \"https://bar.com/\""
    "  }"
    "]}";
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
    };
    fetcher_->SetData(std::move(data));
    manager_->SetFetcherForTest(std::move(fetcher_));
  }

  void CreateScopeExtensions() {
    invalid_file_scope_extension_.origin =
        url::Origin::Create(GURL(kInvalidFileUrl));
    valid_app_scope_extension_.origin = url::Origin::Create(GURL(kValidAppUrl));
    valid_and_invalid_app_scope_extension_.origin =
        url::Origin::Create(GURL(kValidAndInvalidAppsUrl));
  }

  void VerifyValidAndInvalidAppsResult(int expected_callback_count,
                                       base::OnceClosure done_callback,
                                       ScopeExtensions result) {
    callback_count_++;
    ASSERT_EQ(result.size(), 2u);

    ScopeExtensionInfo valid_app_scope_extension{
        valid_app_scope_extension_.origin};
    ScopeExtensionInfo valid_and_invalid_app_scope_extension{
        valid_and_invalid_app_scope_extension_.origin,
        valid_and_invalid_app_scope_extension_.has_origin_wildcard};

    EXPECT_TRUE(base::Contains(result, std::move(valid_app_scope_extension)));
    EXPECT_TRUE(base::Contains(
        result, std::move(valid_and_invalid_app_scope_extension)));

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

  ScopeExtensionInfo invalid_file_scope_extension_;
  ScopeExtensionInfo valid_app_scope_extension_;
  ScopeExtensionInfo valid_and_invalid_app_scope_extension_;
};

IN_PROC_BROWSER_TEST_F(WebAppOriginAssociationManagerTest, NoHandlers) {
  base::test::TestFuture<ScopeExtensions> future;
  manager_->GetWebAppOriginAssociations(
      GURL(kWebAppIdentity), ScopeExtensions(), future.GetCallback());
  const ScopeExtensions result = future.Get<0>();
  ASSERT_TRUE(result.empty());
}

IN_PROC_BROWSER_TEST_F(WebAppOriginAssociationManagerTest,
                       InvalidAssociationFile) {
  base::test::TestFuture<ScopeExtensions> future;
  ScopeExtensions scope_extensions{std::move(invalid_file_scope_extension_)};
  manager_->GetWebAppOriginAssociations(
      GURL(kWebAppIdentity), std::move(scope_extensions), future.GetCallback());
  const ScopeExtensions result = future.Get<0>();
  ASSERT_TRUE(result.empty());
}

IN_PROC_BROWSER_TEST_F(WebAppOriginAssociationManagerTest, OneValidApp) {
  base::test::TestFuture<ScopeExtensions> future;
  ScopeExtensions scope_extensions{valid_app_scope_extension_};
  manager_->GetWebAppOriginAssociations(
      GURL(kWebAppIdentity), std::move(scope_extensions), future.GetCallback());
  const ScopeExtensions result = future.Get<0>();
  ASSERT_TRUE(result.size() == 1);
  auto scope_extension = std::move(*result.begin());
  EXPECT_EQ(scope_extension.origin, valid_app_scope_extension_.origin);
  EXPECT_EQ(scope_extension.has_origin_wildcard,
            valid_app_scope_extension_.has_origin_wildcard);
}

IN_PROC_BROWSER_TEST_F(WebAppOriginAssociationManagerTest,
                       ValidAndInvalidApps) {
  base::test::TestFuture<void> future;

  ScopeExtensions scope_extensions{valid_app_scope_extension_,
                                   valid_and_invalid_app_scope_extension_};
  callback_count_ = 0;
  manager_->GetWebAppOriginAssociations(
      GURL(kWebAppIdentity), std::move(scope_extensions),
      base::BindOnce(
          &WebAppOriginAssociationManagerTest::VerifyValidAndInvalidAppsResult,
          base::Unretained(this), 1, future.GetCallback()));
  EXPECT_TRUE(future.Wait());
}

IN_PROC_BROWSER_TEST_F(WebAppOriginAssociationManagerTest, RunTasks) {
  base::test::TestFuture<void> future;
  ScopeExtensions scope_extensions{valid_app_scope_extension_,
                                   valid_and_invalid_app_scope_extension_};

  // Set status as running temporarily to queue up tasks.
  manager_->task_in_progress_ = true;
  int task_count = 6;
  for (int i = 0; i < task_count - 1; i++) {
    manager_->GetWebAppOriginAssociations(
        GURL(kWebAppIdentity), scope_extensions,
        base::BindOnce(&WebAppOriginAssociationManagerTest::
                           VerifyValidAndInvalidAppsResult,
                       base::Unretained(this), task_count,
                       future.GetCallback()));
  }
  // Reset to no task in progress to start.
  manager_->task_in_progress_ = false;

  callback_count_ = 0;
  manager_->GetWebAppOriginAssociations(
      GURL(kWebAppIdentity), std::move(scope_extensions),
      base::BindOnce(
          &WebAppOriginAssociationManagerTest::VerifyValidAndInvalidAppsResult,
          base::Unretained(this), task_count, future.GetCallback()));
  EXPECT_TRUE(future.Wait());
}

}  // namespace web_app
