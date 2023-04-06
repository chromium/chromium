// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/fuzzing/in_process_fuzz_test.h"
#include "content/public/browser/browser_thread.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

// This is an example use of the InProcessFuzzTest framework.
// It fetches arbitrary HTML from an HTTPS server. It's not really
// intended to be an effective fuzzer, but just to show an example
// of how this framework can be used.

class HtmlInProcessFuzzTest : virtual public InProcessFuzzTest {
 public:
  HtmlInProcessFuzzTest()
      : https_test_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  void SetUpOnMainThread() override {
    InProcessFuzzTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    https_test_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
    https_test_server_.RegisterRequestHandler(base::BindRepeating(
        &HtmlInProcessFuzzTest::HandleHTTPRequest, base::Unretained(this)));
    ASSERT_TRUE(https_test_server_.Start());
  }
  int Fuzz(const uint8_t* data, size_t size) override;
  std::unique_ptr<net::test_server::HttpResponse> HandleHTTPRequest(
      const net::test_server::HttpRequest& request) const;

  net::EmbeddedTestServer https_test_server_;
  std::string current_fuzz_case_;
};

REGISTER_IN_PROCESS_FUZZER(HtmlInProcessFuzzTest)

std::unique_ptr<net::test_server::HttpResponse>
HtmlInProcessFuzzTest::HandleHTTPRequest(
    const net::test_server::HttpRequest& request) const {
  std::unique_ptr<net::test_server::BasicHttpResponse> response;
  response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_content_type("text/html");
  response->set_content(current_fuzz_case_);
  response->set_code(net::HTTP_OK);
  return response;
}

int HtmlInProcessFuzzTest::Fuzz(const uint8_t* data, size_t size) {
  std::string html_string(reinterpret_cast<const char*>(data), size);
  current_fuzz_case_ = html_string;
  GURL test_url = https_test_server_.GetURL("/test.html");
  base::IgnoreResult(ui_test_utils::NavigateToURL(browser(), test_url));
  return 0;
}
