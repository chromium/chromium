// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search_engines/search_engine_tab_helper.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

using net::test_server::BasicHttpResponse;
using net::test_server::HttpRequest;
using net::test_server::HttpResponse;

namespace {

class TemplateURLServiceObserver {
 public:
  TemplateURLServiceObserver(TemplateURLService* service, base::RunLoop* loop)
      : runner_(loop) {
    DCHECK(loop);
    template_url_subscription_ =
        service->RegisterOnLoadedCallback(base::BindOnce(
            &TemplateURLServiceObserver::StopLoop, base::Unretained(this)));
    service->Load();
  }

  TemplateURLServiceObserver(const TemplateURLServiceObserver&) = delete;
  TemplateURLServiceObserver& operator=(const TemplateURLServiceObserver&) =
      delete;

  ~TemplateURLServiceObserver() {}

 private:
  void StopLoop() { runner_->Quit(); }
  raw_ptr<base::RunLoop> runner_;
  base::CallbackListSubscription template_url_subscription_;
};

testing::AssertionResult VerifyTemplateURLServiceLoad(
    TemplateURLService* service) {
  if (service->loaded())
    return testing::AssertionSuccess();
  base::RunLoop runner;
  TemplateURLServiceObserver observer(service, &runner);
  runner.Run();
  if (service->loaded())
    return testing::AssertionSuccess();
  return testing::AssertionFailure() << "TemplateURLService isn't loaded";
}

}  // namespace

class SearchEngineTabHelperBrowserTest : public InProcessBrowserTest {
 public:
  SearchEngineTabHelperBrowserTest() = default;

  SearchEngineTabHelperBrowserTest(const SearchEngineTabHelperBrowserTest&) =
      delete;
  SearchEngineTabHelperBrowserTest& operator=(
      const SearchEngineTabHelperBrowserTest&) = delete;

  ~SearchEngineTabHelperBrowserTest() override = default;

 private:
  std::unique_ptr<HttpResponse> HandleRequest(const GURL& osdd_xml_url,
                                              const HttpRequest& request) {
    std::string html = base::StringPrintf(
        "<html>"
        "<head>"
        "  <link rel='search' type='application/opensearchdescription+xml'"
        "      href='%s'"
        "      title='ExampleSearch'>"
        "</head>"
        "</html>",
        osdd_xml_url.spec().c_str());

    std::unique_ptr<BasicHttpResponse> http_response(new BasicHttpResponse());
    http_response->set_code(net::HTTP_OK);
    http_response->set_content(html);
    http_response->set_content_type("text/html");
    return std::move(http_response);
  }

  // Starts a test server that serves a page pointing to a opensearch descriptor
  // from a file:// url.
  bool StartTestServer() {
    GURL file_url = ui_test_utils::GetTestUrl(
        base::FilePath(),
        base::FilePath().AppendASCII("simple_open_search.xml"));
    embedded_test_server()->RegisterRequestHandler(
        base::BindRepeating(&SearchEngineTabHelperBrowserTest::HandleRequest,
                            base::Unretained(this), file_url));
    return embedded_test_server()->Start();
  }

  void SetUpOnMainThread() override { ASSERT_TRUE(StartTestServer()); }
};

IN_PROC_BROWSER_TEST_F(SearchEngineTabHelperBrowserTest,
                       IgnoreSearchDescriptionsFromFileURLs) {
  TemplateURLService* url_service =
      TemplateURLServiceFactory::GetForProfile(browser()->profile());
  ASSERT_TRUE(url_service);
  VerifyTemplateURLServiceLoad(url_service);
  TemplateURLService::TemplateURLVector template_urls =
      url_service->GetTemplateURLs();
  // Navigate to a page with a search descriptor. Path doesn't matter as the
  // test server always serves the same HTML.
  GURL url(embedded_test_server()->GetURL("/"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  // No new search engines should be added.
  EXPECT_EQ(template_urls, url_service->GetTemplateURLs());
}

class TestSearchEngineTabHelper : public SearchEngineTabHelper {
 public:
  static void CreateForWebContents(content::WebContents* contents) {
    if (FromWebContents(contents))
      return;
    contents->SetUserData(
        UserDataKey(), std::make_unique<TestSearchEngineTabHelper>(contents));
  }
  explicit TestSearchEngineTabHelper(content::WebContents* web_contents)
      : SearchEngineTabHelper(web_contents) {}
  ~TestSearchEngineTabHelper() override = default;

  std::u16string GenerateKeywordFromNavigationEntry(
      content::NavigationEntry* entry) override {
    std::u16string keyword =
        SearchEngineTabHelper::GenerateKeywordFromNavigationEntry(entry);
    if (!keyword.empty())
      return keyword;

    const std::u16string kTestKeyword(u"TestKeyword");
    return kTestKeyword;
  }
};

class SearchEngineTabHelperPrerenderingBrowserTest
    : public InProcessBrowserTest {
 public:
  SearchEngineTabHelperPrerenderingBrowserTest()
      : prerender_helper_(base::BindRepeating(
            &SearchEngineTabHelperPrerenderingBrowserTest::GetWebContents,
            base::Unretained(this))) {}
  ~SearchEngineTabHelperPrerenderingBrowserTest() override = default;

  void SetUp() override {
    prerender_helper_.SetUp(embedded_test_server());
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    ASSERT_TRUE(test_server_handle_ =
                    embedded_test_server()->StartAndReturnHandle());
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  content::test::PrerenderTestHelper* prerender_helper() {
    return &prerender_helper_;
  }

  // Adds a tab with TestSearchEngineTabHelper which is a customized
  // SearchEngineTabHelper.
  void GetNewTabWithTestSearchEngineTabHelper() {
    content::WebContents* preexisting_tab =
        browser()->tab_strip_model()->GetActiveWebContents();
    std::unique_ptr<content::WebContents> owned_web_contents =
        content::WebContents::Create(
            content::WebContents::CreateParams(browser()->profile()));
    ASSERT_TRUE(owned_web_contents.get());

    TestSearchEngineTabHelper::CreateForWebContents(owned_web_contents.get());
    ASSERT_FALSE(owned_web_contents.get()->IsLoading());
    browser()->tab_strip_model()->AppendWebContents(
        std::move(owned_web_contents), true);
    if (preexisting_tab) {
      browser()->tab_strip_model()->CloseWebContentsAt(
          0, TabCloseTypes::CLOSE_NONE);
    }
  }

 protected:
  content::test::PrerenderTestHelper prerender_helper_;
  net::test_server::EmbeddedTestServerHandle test_server_handle_;
};

IN_PROC_BROWSER_TEST_F(SearchEngineTabHelperPrerenderingBrowserTest,
                       GenerateKeywordInPrerendering) {
  GetNewTabWithTestSearchEngineTabHelper();
  TemplateURLService* url_service =
      TemplateURLServiceFactory::GetForProfile(browser()->profile());
  ASSERT_TRUE(url_service);
  VerifyTemplateURLServiceLoad(url_service);
  TemplateURLService::TemplateURLVector template_urls =
      url_service->GetTemplateURLs();

  GURL url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Loads a page in the prerender.
  auto prerender_url = embedded_test_server()->GetURL("/form_search.html");
  int host_id = prerender_helper()->AddPrerender(prerender_url);
  content::test::PrerenderHostObserver host_observer(*GetWebContents(),
                                                     host_id);
  content::RenderFrameHost* render_frame_host =
      prerender_helper()->GetPrerenderedMainFrameHost(host_id);
  EXPECT_EQ(nullptr, content::EvalJs(render_frame_host, "submit_form();"));
  // Since navigation from a prerendering page is disallowed, prerendering is
  // canceled.
  host_observer.WaitForDestroyed();
  // TemplateURLVector's not changed with the prerendering.
  EXPECT_EQ(template_urls, url_service->GetTemplateURLs());

  // Navigates the primary page to the URL.
  prerender_helper()->NavigatePrimaryPage(prerender_url);
  // Makes sure that the page is not from the prerendering.
  EXPECT_FALSE(host_observer.was_activated());
  // Submits a search query.
  content::TestNavigationObserver observer(GetWebContents());
  EXPECT_EQ(nullptr, content::EvalJs(GetWebContents()->GetPrimaryMainFrame(),
                                     "submit_form();"));
  observer.Wait();

  // A new template url is added on the primary page.
  EXPECT_NE(template_urls, url_service->GetTemplateURLs());
}

// TODO(http://crbug.com/1270519): Add a test for prerendering scenarios there
// the OpenSearchDescriptionDocumentHandler mojo calls are deferred.
