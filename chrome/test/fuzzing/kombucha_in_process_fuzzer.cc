// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/fuzzing/in_process_fuzzer.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/browser/browser_thread.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

// At the moment, this is an example use of the InProcessFuzzer framework
// that uses Kombucha. It's not yet intended to be an effective fuzzer,
// but just to be the skeleton of how this framework can be used.

class KombuchaInProcessFuzzer
    : virtual public InteractiveBrowserTestT<InProcessFuzzer> {
 public:
  void SetUp() override { InteractiveBrowserTestT::SetUp(); }

  void TearDownOnMainThread() override {
    InteractiveBrowserTestT::TearDownOnMainThread();
  }

  void SetUpOnMainThread() override;
  int Fuzz(const uint8_t* data, size_t size) override;
  static std::unique_ptr<net::test_server::HttpResponse> HandleHTTPRequest(
      std::string response_body,
      const net::test_server::HttpRequest& request);

  std::string current_fuzz_case_;
};

REGISTER_IN_PROCESS_FUZZER(KombuchaInProcessFuzzer)

void KombuchaInProcessFuzzer::SetUpOnMainThread() {
  InteractiveBrowserTestT::SetUpOnMainThread();
  host_resolver()->AddRule("*", "127.0.0.1");
  embedded_test_server()->SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      &KombuchaInProcessFuzzer::HandleHTTPRequest, current_fuzz_case_));
  ASSERT_TRUE(embedded_test_server()->Start());
}

std::unique_ptr<net::test_server::HttpResponse>
KombuchaInProcessFuzzer::HandleHTTPRequest(
    std::string response_body,
    const net::test_server::HttpRequest& request) {
  std::unique_ptr<net::test_server::BasicHttpResponse> response;
  response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_content_type("text/html");
  response->set_content(response_body);
  response->set_code(net::HTTP_OK);
  return response;
}

int KombuchaInProcessFuzzer::Fuzz(const uint8_t* data, size_t size) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kPrimaryTabElementId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondaryTabElementId);
  std::string html_string(reinterpret_cast<const char*>(data), size);
  current_fuzz_case_ = html_string;
  GURL test_url = embedded_test_server()->GetURL("/test.html");
  RunTestSequence(
      InstrumentTab(kPrimaryTabElementId, 0),
      PressButton(kNewTabButtonElementId),
      AddInstrumentedTab(kSecondaryTabElementId, GURL("about:blank")),
      // Only the following step requires the webserver.
      NavigateWebContents(kSecondaryTabElementId, test_url));
  return 0;
}
