// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/nacl/pnacl_header_test.h"

#include <utility>

#include "base/bind.h"
#include "base/path_service.h"
#include "base/test/scoped_path_override.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/nacl/nacl_browsertest_util.h"
#include "content/public/browser/web_contents.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/url_request/url_request.h"

using net::test_server::BasicHttpResponse;
using net::test_server::HttpRequest;
using net::test_server::HttpResponse;

/*
void TestDispatcherHostDelegate::RequestBeginning(
    net::URLRequest* request,
    content::ResourceContext* resource_context,
    content::AppCacheService* appcache_service,
    content::ResourceType resource_type,
    std::vector<std::unique_ptr<content::ResourceThrottle>>* throttles) {
  // This checks the same condition as the one for PNaCl in
  // AppendComponentUpdaterThrottles.
  if (resource_type == content::ResourceType::kObject) {
    const net::HttpRequestHeaders& headers = request->extra_request_headers();
    std::string accept_headers;
    if (headers.GetHeader("Accept", &accept_headers)) {
      if (accept_headers.find("application/x-pnacl") != std::string::npos)
        found_pnacl_header_ = true;
    }
  }
}
*/

PnaclHeaderTest::PnaclHeaderTest() : noncors_loads_(0), cors_loads_(0) {}

PnaclHeaderTest::~PnaclHeaderTest() {}

void PnaclHeaderTest::StartServer() {
  // For most requests, just serve files, but register a special test handler
  // that watches for the .pexe fetch also.
  base::FilePath test_data_dir;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir));
  embedded_test_server()->RegisterRequestHandler(
      base::Bind(&PnaclHeaderTest::WatchForPexeFetch, base::Unretained(this)));
  embedded_test_server()->ServeFilesFromDirectory(test_data_dir);
  ASSERT_TRUE(embedded_test_server()->Start());
}

void PnaclHeaderTest::RunLoadTest(const std::string& url,
                                  int expected_noncors,
                                  int expected_cors) {
  // content::ResourceDispatcherHost::Get()->SetDelegate(&test_delegate_);
  StartServer();
  LoadTestMessageHandler handler;
  content::JavascriptTestObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents(),
      &handler);

  // Make sure this is able to do a pexe fetch, even without access
  // to the PNaCl component files (make DIR_PNACL_COMPONENT empty).
  // The pexe fetch that is done with special headers must be able to
  // start before the component files are on disk. This is because it
  // is the pexe fetch that helps trigger an on-demand installation
  // which installs the files to disk (if that hasn't already happened
  // in the background).
  base::ScopedPathOverride component_dir(chrome::DIR_PNACL_COMPONENT);

  ui_test_utils::NavigateToURL(browser(), embedded_test_server()->GetURL(url));

  // Wait until the NMF and pexe are also loaded, not just the HTML.
  // Do this by waiting till the LoadTestMessageHandler responds.
  EXPECT_TRUE(observer.Run()) << handler.error_message();

  // Now check the expectations.
  EXPECT_TRUE(handler.test_passed()) << "Test failed.";
  EXPECT_EQ(expected_noncors, noncors_loads_);
  EXPECT_EQ(expected_cors, cors_loads_);

  // content::ResourceDispatcherHost::Get()->SetDelegate(NULL);
}

std::unique_ptr<HttpResponse> PnaclHeaderTest::WatchForPexeFetch(
    const HttpRequest& request) {
  // Avoid favicon.ico warning by giving it a dummy icon.
  if (request.relative_url.find("favicon.ico") != std::string::npos) {
    std::unique_ptr<BasicHttpResponse> http_response(new BasicHttpResponse());
    http_response->set_code(net::HTTP_OK);
    http_response->set_content("");
    http_response->set_content_type("application/octet-stream");
    return std::move(http_response);
  }

  // Skip other non-pexe files and let ServeFilesFromDirectory handle it.
  GURL absolute_url = embedded_test_server()->GetURL(request.relative_url);
  if (absolute_url.path().find(".pexe") == std::string::npos)
    return std::unique_ptr<HttpResponse>();

  // For pexe files, check for the special Accept header,
  // along with the expected ResourceType of the URL request.
  EXPECT_NE(0U, request.headers.count("Accept"));
  auto it = request.headers.find("Accept");
  EXPECT_NE(std::string::npos, it->second.find("application/x-pnacl"));
  EXPECT_NE(std::string::npos, it->second.find("*/*"));
  // EXPECT_TRUE(test_delegate_.found_pnacl_header());

  // Also make sure that other headers like CORS-related headers
  // are preserved when injecting the special Accept header.
  if (absolute_url.path().find("cors") == std::string::npos) {
    EXPECT_EQ(0U, request.headers.count("Origin"));
    noncors_loads_ += 1;
  } else {
    EXPECT_EQ(1U, request.headers.count("Origin"));
    cors_loads_ += 1;
  }

  // After checking the header, just return a 404. We don't need to actually
  // compile and stopping with a 404 is faster.
  std::unique_ptr<BasicHttpResponse> http_response(new BasicHttpResponse());
  http_response->set_code(net::HTTP_NOT_FOUND);
  http_response->set_content("PEXE ... not found");
  http_response->set_content_type("application/octet-stream");
  return std::move(http_response);
}

// Flaky: http://crbug.com/711289
IN_PROC_BROWSER_TEST_F(PnaclHeaderTest, DISABLED_TestHasPnaclHeader) {
  // Load 2 pexes, one same origin and one cross orgin.
  RunLoadTest("/nacl/pnacl_request_header/pnacl_request_header.html", 1, 1);
}
