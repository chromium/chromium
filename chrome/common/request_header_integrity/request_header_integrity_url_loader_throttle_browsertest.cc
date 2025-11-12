// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/request_header_integrity/request_header_integrity_url_loader_throttle.h"

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/run_loop.h"
#include "base/strings/string_split.h"
#include "build/branding_buildflags.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/common/request_header_integrity/internal/google_header_names.h"
#endif

#if !defined(LASTCHANGE_YEAR_HEADER_NAME)
#define LASTCHANGE_YEAR_HEADER_NAME "X-Placeholder-2"
#endif

#if !defined(VALIDATE_HEADER_NAME)
#define VALIDATE_HEADER_NAME "X-Placeholder-3"
#endif

#if !defined(COPYRIGHT_HEADER_NAME)
#define COPYRIGHT_HEADER_NAME "X-Placeholder-4"
#endif

class RequestHeaderIntegrityURLLoaderThrottleBrowserTest
    : public InProcessBrowserTest {
 public:
  RequestHeaderIntegrityURLLoaderThrottleBrowserTest()
      : https_server_(net::test_server::EmbeddedTestServer::TYPE_HTTPS) {}

  RequestHeaderIntegrityURLLoaderThrottleBrowserTest(
      const RequestHeaderIntegrityURLLoaderThrottleBrowserTest&) = delete;
  RequestHeaderIntegrityURLLoaderThrottleBrowserTest& operator=(
      const RequestHeaderIntegrityURLLoaderThrottleBrowserTest&) = delete;

  ~RequestHeaderIntegrityURLLoaderThrottleBrowserTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");

    server().RegisterRequestHandler(base::BindRepeating(
        &RequestHeaderIntegrityURLLoaderThrottleBrowserTest::RequestHandler,
        base::Unretained(this)));

    server().SetCertHostnames({"www.google.com", "www.chromium.org"});
    ASSERT_TRUE(server().InitializeAndListen());
    server().StartAcceptingConnections();
  }

 protected:
  const net::EmbeddedTestServer& server() const { return https_server_; }
  net::EmbeddedTestServer& server() { return https_server_; }

  GURL GetGoogleUrl() const { return server().GetURL("www.google.com", "/"); }

  GURL GetGoogleBeaconPageUrl() const {
    return server().GetURL("www.google.com", "/beacon.html");
  }

  GURL GetGoogleBeaconReceiverUrl() const {
    return server().GetURL("www.google.com", "/beacon_receiver.html");
  }

  GURL GetGoogleScriptPageUrl() const {
    return server().GetURL("www.google.com", "/sync_script.html");
  }

  GURL GetGoogleScriptBootstrapUrl() const {
    return server().GetURL("www.google.com", "/bootstrap.js");
  }

  GURL GetGoogleScriptDynamicScriptUrl() const {
    return server().GetURL("www.google.com", "/dynamic_script.js");
  }

  GURL GetChromiumUrl() const {
    return server().GetURL("www.chromium.org", "/");
  }

  GURL GetChromiumToChromiumRedirectUrl() {
    return server().GetURL("www.chromium.org", "/redirectChromiumToChromium");
  }

  GURL GetChromiumToGoogleRedirectUrl() {
    return server().GetURL("www.chromium.org", "/redirectChromiumToGoogle");
  }

  GURL GetGoogleToChromiumRedirectUrl() {
    return server().GetURL("www.google.com", "/redirectGoogleToChromium");
  }

  GURL GetGoogleToGoogleRedirectUrl() {
    return server().GetURL("www.google.com", "/redirectGoogleToGoogle");
  }

  // Returns whether a given `header` has been received for a `url`. If
  // `url` has not been observed, fails with ADD_FAILURE() and returns false.
  bool HasReceivedHeader(const GURL& url, std::string_view header) const {
    auto it = received_headers_.find(url);
    if (it == received_headers_.end()) {
      ADD_FAILURE() << "No navigation for url " << url;
      return false;
    }
    return it->second.contains(header);
  }

  void WaitForRequest(const GURL& url) {
    if (received_headers_.contains(url)) {
      return;
    }
    base::RunLoop loop;
    done_callbacks_.emplace(url, loop.QuitClosure());
    loop.Run();
  }

 private:
  std::unique_ptr<net::test_server::HttpResponse> RequestHandler(
      const net::test_server::HttpRequest& request) {
    std::string host;
    // Retrieve the host name (without port) from the request headers.
    auto request_iter = request.headers.find("Host");
    if (request_iter != request.headers.end()) {
      host = request_iter->second;
    }
    auto components = base::SplitStringOnce(host, ':');
    if (components) {
      host = components->first;
    }

    // Recover the original URL of the request by replacing the host name in
    // request.GetURL() (which is 127.0.0.1) with the host name from the request
    // headers.
    GURL::Replacements replacements;
    replacements.SetHostStr(host);
    GURL original_url = request.GetURL().ReplaceComponents(replacements);

    // Save the request headers for test verification.
    received_headers_[original_url] = request.headers;
    auto callbacks_iter = done_callbacks_.find(original_url);
    if (callbacks_iter != done_callbacks_.end()) {
      std::move(callbacks_iter->second).Run();
    }

    auto http_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    if (request.relative_url == GetGoogleBeaconPageUrl().GetPath()) {
      http_response->set_code(net::HTTP_OK);
      http_response->set_content(R"(
        <script type="text/javascript">
          window.navigator.sendBeacon("beacon_receiver.html");
        </script>
        )");
      http_response->set_content_type("text/html");
    } else if (request.relative_url == GetGoogleScriptPageUrl().GetPath()) {
      http_response->set_code(net::HTTP_OK);
      http_response->set_content(R"(
        <script src='/bootstrap.js' async defer></script>
        )");
      http_response->set_content_type("text/html");
    } else if (request.relative_url ==
               GetGoogleScriptBootstrapUrl().GetPath()) {
      http_response->set_code(net::HTTP_OK);
      http_response->set_content(R"(
        (function() {
          var scriptElement = document.createElement('script');
          scriptElement.type = 'text/javascript';
          scriptElement.async = true;
          scriptElement.charset = 'utf-8';
          scriptElement.src = '/dynamic_script.js';
          var pageScriptElement = document.getElementsByTagName('script')[0];
          pageScriptElement.parentNode.insertBefore(
              scriptElement, pageScriptElement);
        })();
        )");
      http_response->set_content_type("text/javascript");
    } else if (request.relative_url ==
               GetGoogleScriptDynamicScriptUrl().GetPath()) {
      http_response->set_code(net::HTTP_OK);
      http_response->set_content(R"(
        // Placeholder Script
        )");
      http_response->set_content_type("text/javascript");
    } else if (request.relative_url ==
               GetChromiumToChromiumRedirectUrl().GetPath()) {
      http_response->set_code(net::HTTP_FOUND);
      http_response->AddCustomHeader("Location", GetChromiumUrl().spec());
    } else if (request.relative_url ==
               GetChromiumToGoogleRedirectUrl().GetPath()) {
      http_response->set_code(net::HTTP_FOUND);
      http_response->AddCustomHeader("Location", GetGoogleUrl().spec());
    } else if (request.relative_url ==
               GetGoogleToChromiumRedirectUrl().GetPath()) {
      http_response->set_code(net::HTTP_FOUND);
      http_response->AddCustomHeader("Location", GetChromiumUrl().spec());
    } else if (request.relative_url ==
               GetGoogleToGoogleRedirectUrl().GetPath()) {
      http_response->set_code(net::HTTP_FOUND);
      http_response->AddCustomHeader("Location", GetGoogleUrl().spec());
    } else {
      http_response->set_code(net::HTTP_OK);
      http_response->set_content("hello");
      http_response->set_content_type("text/html");
    }
    return http_response;
  }

  // Test server responding to HTTPS requests in this browser test.
  net::EmbeddedTestServer https_server_;

  // Stores the observed HTTP Request headers.
  std::map<GURL, net::test_server::HttpRequest::HeaderMap> received_headers_;

  // For waiting for requests.
  std::map<GURL, base::OnceClosure> done_callbacks_;
};

IN_PROC_BROWSER_TEST_F(RequestHeaderIntegrityURLLoaderThrottleBrowserTest,
                       HeadersAddedForGoogleUrl) {
  GURL google_url = GetGoogleUrl();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), google_url));
  EXPECT_TRUE(HasReceivedHeader(google_url, LASTCHANGE_YEAR_HEADER_NAME));
  EXPECT_TRUE(HasReceivedHeader(google_url, VALIDATE_HEADER_NAME));
  EXPECT_TRUE(HasReceivedHeader(google_url, COPYRIGHT_HEADER_NAME));
}

IN_PROC_BROWSER_TEST_F(RequestHeaderIntegrityURLLoaderThrottleBrowserTest,
                       HeadersNotAddedForChromium) {
  GURL chromium_url = GetChromiumUrl();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), chromium_url));
  EXPECT_FALSE(HasReceivedHeader(chromium_url, LASTCHANGE_YEAR_HEADER_NAME));
  EXPECT_FALSE(HasReceivedHeader(chromium_url, VALIDATE_HEADER_NAME));
  EXPECT_FALSE(HasReceivedHeader(chromium_url, COPYRIGHT_HEADER_NAME));
}

IN_PROC_BROWSER_TEST_F(RequestHeaderIntegrityURLLoaderThrottleBrowserTest,
                       HeadersAddedForBeacon) {
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GetGoogleBeaconPageUrl()));
  GURL beacon_receiver_url = GetGoogleBeaconReceiverUrl();
  WaitForRequest(beacon_receiver_url);
  EXPECT_TRUE(
      HasReceivedHeader(beacon_receiver_url, LASTCHANGE_YEAR_HEADER_NAME));
  EXPECT_TRUE(HasReceivedHeader(beacon_receiver_url, VALIDATE_HEADER_NAME));
  EXPECT_TRUE(HasReceivedHeader(beacon_receiver_url, COPYRIGHT_HEADER_NAME));
}

IN_PROC_BROWSER_TEST_F(RequestHeaderIntegrityURLLoaderThrottleBrowserTest,
                       HeadersAddedForScriptRequest) {
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GetGoogleScriptPageUrl()));
  GURL dynamic_script_url = GetGoogleScriptDynamicScriptUrl();
  WaitForRequest(dynamic_script_url);
  EXPECT_TRUE(
      HasReceivedHeader(dynamic_script_url, LASTCHANGE_YEAR_HEADER_NAME));
  EXPECT_TRUE(HasReceivedHeader(dynamic_script_url, VALIDATE_HEADER_NAME));
  EXPECT_TRUE(HasReceivedHeader(dynamic_script_url, COPYRIGHT_HEADER_NAME));
}

IN_PROC_BROWSER_TEST_F(RequestHeaderIntegrityURLLoaderThrottleBrowserTest,
                       RedirectFromChromiumToChromium) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GetChromiumToChromiumRedirectUrl()));
  GURL target_url = GetChromiumUrl();
  WaitForRequest(target_url);
  EXPECT_FALSE(HasReceivedHeader(target_url, LASTCHANGE_YEAR_HEADER_NAME));
  EXPECT_FALSE(HasReceivedHeader(target_url, VALIDATE_HEADER_NAME));
  EXPECT_FALSE(HasReceivedHeader(target_url, COPYRIGHT_HEADER_NAME));
}

IN_PROC_BROWSER_TEST_F(RequestHeaderIntegrityURLLoaderThrottleBrowserTest,
                       RedirectFromChromiumToGoogle) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GetChromiumToGoogleRedirectUrl()));
  GURL target_url = GetGoogleUrl();
  WaitForRequest(target_url);
  EXPECT_TRUE(HasReceivedHeader(target_url, LASTCHANGE_YEAR_HEADER_NAME));
  EXPECT_TRUE(HasReceivedHeader(target_url, VALIDATE_HEADER_NAME));
  EXPECT_TRUE(HasReceivedHeader(target_url, COPYRIGHT_HEADER_NAME));
}

IN_PROC_BROWSER_TEST_F(RequestHeaderIntegrityURLLoaderThrottleBrowserTest,
                       RedirectFromGoogleToChromium) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GetGoogleToChromiumRedirectUrl()));
  GURL target_url = GetChromiumUrl();
  WaitForRequest(target_url);
  EXPECT_FALSE(HasReceivedHeader(target_url, LASTCHANGE_YEAR_HEADER_NAME));
  EXPECT_FALSE(HasReceivedHeader(target_url, VALIDATE_HEADER_NAME));
  EXPECT_FALSE(HasReceivedHeader(target_url, COPYRIGHT_HEADER_NAME));
}

IN_PROC_BROWSER_TEST_F(RequestHeaderIntegrityURLLoaderThrottleBrowserTest,
                       RedirectFromGoogleToGoogle) {
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GetGoogleToGoogleRedirectUrl()));
  GURL target_url = GetGoogleUrl();
  WaitForRequest(target_url);
  EXPECT_TRUE(HasReceivedHeader(target_url, LASTCHANGE_YEAR_HEADER_NAME));
  EXPECT_TRUE(HasReceivedHeader(target_url, VALIDATE_HEADER_NAME));
  EXPECT_TRUE(HasReceivedHeader(target_url, COPYRIGHT_HEADER_NAME));
}
