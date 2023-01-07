// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/test/scoped_feature_list.h"
#include "components/url_rewrite/browser/url_request_rewrite_rules_manager.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

namespace url_rewrite {

using ::testing::IsEmpty;
using ::testing::SizeIs;

// Attaches an inner WebContents to the UrlRequestRewriteRulesManager.
class InnerWebContentsHandler : public content::WebContentsObserver {
 public:
  InnerWebContentsHandler(
      content::WebContents* web_contents,
      UrlRequestRewriteRulesManager* url_request_rewrite_rules_manager)
      : content::WebContentsObserver(web_contents),
        url_request_rewrite_rules_manager_(url_request_rewrite_rules_manager) {}

  void Wait() { run_loop_.Run(); }

 private:
  // content::WebContentsObserver implementation.
  void InnerWebContentsCreated(
      content::WebContents* inner_web_contents) override {
    CHECK(
        url_request_rewrite_rules_manager_->AddWebContents(inner_web_contents));
    run_loop_.Quit();
  }

  base::RunLoop run_loop_{base::RunLoop::Type::kNestableTasksAllowed};
  raw_ptr<UrlRequestRewriteRulesManager> url_request_rewrite_rules_manager_;
};

class UrlRequestRewriteRulesManagerBrowserTest
    : public content::ContentBrowserTest {
 public:
 protected:
  void SetUp() override {
    // Enable portals to emulate creation of inner WebContents.
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{blink::features::kPortals,
                              blink::features::kPortalsCrossOrigin},
        /*disabled_features=*/{});
    ContentBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    base::FilePath root_dir;
    base::PathService::Get(base::DIR_SOURCE_ROOT, &root_dir);
    embedded_test_server()->ServeFilesFromDirectory(
        root_dir.AppendASCII("components/test/data/url_rewrite"));
    ASSERT_TRUE(embedded_test_server()->Start());
    ContentBrowserTest::SetUpOnMainThread();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  UrlRequestRewriteRulesManager url_request_rewrite_rules_manager_;
};

IN_PROC_BROWSER_TEST_F(UrlRequestRewriteRulesManagerBrowserTest,
                       AddRemoveWebContentsSucceeds) {
  ASSERT_TRUE(url_request_rewrite_rules_manager_.AddWebContents(
      shell()->web_contents()));
  ASSERT_FALSE(url_request_rewrite_rules_manager_.AddWebContents(
      shell()->web_contents()));
  ASSERT_TRUE(url_request_rewrite_rules_manager_.RemoveWebContents(
      shell()->web_contents()));
}

IN_PROC_BROWSER_TEST_F(UrlRequestRewriteRulesManagerBrowserTest,
                       RulesUpdatedWithSingleWebContents) {
  ASSERT_TRUE(url_request_rewrite_rules_manager_.AddWebContents(
      shell()->web_contents()));

  // Load a simple HTML page.
  GURL url = embedded_test_server()->GetURL("/single_web_contents.html");
  content::TestNavigationObserver navigation_observer(shell()->web_contents());
  ASSERT_TRUE(NavigateToURL(shell(), url));
  navigation_observer.Wait();

  // Verify there were no inner WebContents created, and updaters size is 1.
  ASSERT_THAT(shell()->web_contents()->GetInnerWebContents(), IsEmpty());
  ASSERT_EQ(url_request_rewrite_rules_manager_.GetUpdatersSizeForTesting(), 1u);
  ASSERT_TRUE(url_request_rewrite_rules_manager_.OnRulesUpdated(
      mojom::UrlRequestRewriteRules::New()));
}

IN_PROC_BROWSER_TEST_F(UrlRequestRewriteRulesManagerBrowserTest,
                       RulesUpdatedWithMultipleWebContents) {
  ASSERT_TRUE(url_request_rewrite_rules_manager_.AddWebContents(
      shell()->web_contents()));

  // Load an HTML page with portal element. This will results in creation of the
  // second inner WebContents that is signaled by the
  // |inner_web_contents_waiter|. As the operation is finished, the 2nd
  // WebContents will be registered with |url_request_rewrite_rules_manager_|
  // and rewrite rules' update will be verified.
  GURL url = embedded_test_server()->GetURL("/multiple_web_contents.html");
  InnerWebContentsHandler inner_web_contents_waiter(
      shell()->web_contents(), &url_request_rewrite_rules_manager_);
  content::TestNavigationObserver navigation_observer(shell()->web_contents());
  ASSERT_TRUE(NavigateToURL(shell(), url));
  navigation_observer.Wait();
  inner_web_contents_waiter.Wait();

  ASSERT_THAT(shell()->web_contents()->GetInnerWebContents(), SizeIs(1));
  ASSERT_EQ(url_request_rewrite_rules_manager_.GetUpdatersSizeForTesting(), 2u);
  ASSERT_TRUE(url_request_rewrite_rules_manager_.OnRulesUpdated(
      mojom::UrlRequestRewriteRules::New()));
}

// Tests that adding a WebContents after a navigation has already occurred
// does not trigger a DCHECK on destruction. This is a regression test for
// https://crbug.com/1152930.
IN_PROC_BROWSER_TEST_F(UrlRequestRewriteRulesManagerBrowserTest,
                       WebContentsAddedAfterNavigation) {
  // Load a simple HTML page.
  GURL url = embedded_test_server()->GetURL("/single_web_contents.html");
  content::TestNavigationObserver navigation_observer(shell()->web_contents());
  ASSERT_TRUE(NavigateToURL(shell(), url));
  navigation_observer.Wait();

  // Add the WebContents to the manager. At this point, an RFH has already been
  // created. Eventually, it will be destroyed.
  ASSERT_TRUE(url_request_rewrite_rules_manager_.AddWebContents(
      shell()->web_contents()));

  // Verify there were no inner WebContents created, and updaters size is 1.
  ASSERT_THAT(shell()->web_contents()->GetInnerWebContents(), IsEmpty());
  ASSERT_EQ(url_request_rewrite_rules_manager_.GetUpdatersSizeForTesting(), 1u);
  ASSERT_TRUE(url_request_rewrite_rules_manager_.OnRulesUpdated(
      mojom::UrlRequestRewriteRules::New()));
}
}  // namespace url_rewrite
