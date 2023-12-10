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

class UrlRequestRewriteRulesManagerBrowserTest
    : public content::ContentBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    base::FilePath root_dir;
    base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &root_dir);
    embedded_test_server()->ServeFilesFromDirectory(
        root_dir.AppendASCII("components/test/data/url_rewrite"));
    ASSERT_TRUE(embedded_test_server()->Start());
    ContentBrowserTest::SetUpOnMainThread();
  }

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

  ASSERT_EQ(url_request_rewrite_rules_manager_.GetUpdatersSizeForTesting(), 1u);
  ASSERT_TRUE(url_request_rewrite_rules_manager_.OnRulesUpdated(
      mojom::UrlRequestRewriteRules::New()));
}

IN_PROC_BROWSER_TEST_F(UrlRequestRewriteRulesManagerBrowserTest,
                       RulesUpdatedWithMultipleWebContents) {
  GURL url = embedded_test_server()->GetURL("/single_web_contents.html");

  // Load a URL in two separate contents -- the default, and a second shell.
  // Both contents should be registered with
  // |url_request_rewrite_rules_manager_| and rewrite rules' update will be
  // verified.

  {
    ASSERT_TRUE(url_request_rewrite_rules_manager_.AddWebContents(
        shell()->web_contents()));
    content::TestNavigationObserver navigation_observer(
        shell()->web_contents());
    ASSERT_TRUE(NavigateToURL(shell(), url));
    navigation_observer.Wait();
  }

  {
    content::TestNavigationObserver navigation_observer(url);
    navigation_observer.StartWatchingNewWebContents();
    auto* second_shell = content::Shell::CreateNewWindow(
        shell()->web_contents()->GetBrowserContext(), url, nullptr, {800, 600});
    ASSERT_TRUE(url_request_rewrite_rules_manager_.AddWebContents(
        second_shell->web_contents()));
    navigation_observer.Wait();
  }

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

  ASSERT_TRUE(url_request_rewrite_rules_manager_.OnRulesUpdated(
      mojom::UrlRequestRewriteRules::New()));
}
}  // namespace url_rewrite
