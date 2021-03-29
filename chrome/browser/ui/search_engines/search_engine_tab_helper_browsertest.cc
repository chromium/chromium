// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search_engines/search_engine_tab_helper.h"

#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/test/browser_test.h"
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
  ~TemplateURLServiceObserver() {}

 private:
  void StopLoop() { runner_->Quit(); }
  base::RunLoop* runner_;
  base::CallbackListSubscription template_url_subscription_;

  DISALLOW_COPY_AND_ASSIGN(TemplateURLServiceObserver);
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
  SearchEngineTabHelperBrowserTest() {}
  ~SearchEngineTabHelperBrowserTest() override {}

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

  DISALLOW_COPY_AND_ASSIGN(SearchEngineTabHelperBrowserTest);
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
  ui_test_utils::NavigateToURL(browser(), url);
  // No new search engines should be added.
  EXPECT_EQ(template_urls, url_service->GetTemplateURLs());
}
