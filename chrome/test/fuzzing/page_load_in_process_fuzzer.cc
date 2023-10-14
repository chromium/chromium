// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <fuzzer/FuzzedDataProvider.h>
#include <google/protobuf/descriptor.h>
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/fuzzing/in_process_proto_fuzzer.h"
#include "chrome/test/fuzzing/page_load_in_process_fuzzer.pb.h"
#include "content/public/browser/browser_thread.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/libfuzzer/proto/lpm_interface.h"

// A fuzzer which can test the interaction of HTTP response parameters
// and HTML content. This is a large search space and it's unlikely that
// this fuzzer will presently find interesting results, but future
// technologies that can better explore a search space like this may
// successfully do so. Meanwhile, it may be useful to aid reproduction
// of human-crafted test cases.
//
// In the future we might want to extend this fuzzer to:
// * support different HTTPS parameters too
// * support multiple, different, HTTP(S) responses in order to
//   handle iframes or other types of navigation.
//   (We'd need to provide a corpus designed to exercise these).
// * run servers on 3+ different ports to support cross-origin navigations

class PageLoadInProcessFuzzer : public InProcessFuzzer {
 public:
  using FuzzCase = test::fuzzing::page_load_fuzzing::FuzzCase;
  PageLoadInProcessFuzzer();

  void SetUpOnMainThread() override;
  int Fuzz(const uint8_t* data, size_t size) override;

 private:
  static std::unique_ptr<net::test_server::HttpResponse> HandleHTTPRequest(
      base::WeakPtr<PageLoadInProcessFuzzer> fuzzer_weak,
      const net::test_server::HttpRequest& request);

 private:
  net::EmbeddedTestServer http_test_server_;
  net::EmbeddedTestServer https_test_server_;
  std::unique_ptr<net::test_server::BasicHttpResponse> http_response_;
  base::WeakPtrFactory<PageLoadInProcessFuzzer> weak_ptr_factory_{this};
};

REGISTER_TEXT_PROTO_IN_PROCESS_FUZZER(PageLoadInProcessFuzzer)

PageLoadInProcessFuzzer::PageLoadInProcessFuzzer()
    : http_test_server_(net::EmbeddedTestServer::TYPE_HTTP),
      https_test_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
  https_test_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
  http_test_server_.RegisterRequestHandler(
      base::BindRepeating(&PageLoadInProcessFuzzer::HandleHTTPRequest,
                          weak_ptr_factory_.GetWeakPtr()));
  https_test_server_.RegisterRequestHandler(
      base::BindRepeating(&PageLoadInProcessFuzzer::HandleHTTPRequest,
                          weak_ptr_factory_.GetWeakPtr()));
}

void PageLoadInProcessFuzzer::SetUpOnMainThread() {
  InProcessFuzzer::SetUpOnMainThread();
  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(http_test_server_.Start());
  ASSERT_TRUE(https_test_server_.Start());
}

std::unique_ptr<net::test_server::HttpResponse>
PageLoadInProcessFuzzer::HandleHTTPRequest(
    base::WeakPtr<PageLoadInProcessFuzzer> fuzzer_weak,
    const net::test_server::HttpRequest& request) {
  std::unique_ptr<net::test_server::BasicHttpResponse> response;
  // We are running on the embedded test server's thread.
  // We want to ask the fuzzer thread for the fuzz case.
  // We use a weak pointer, but we have to dereference that on the originating
  // thread.
  base::RunLoop run_loop;
  base::RepeatingCallback<void()> get_payload_lambda =
      base::BindLambdaForTesting([&]() {
        PageLoadInProcessFuzzer* fuzzer = fuzzer_weak.get();
        if (fuzzer) {
          response = std::move(fuzzer->http_response_);
        }
        run_loop.Quit();
      });
  content::GetUIThreadTaskRunner()->PostTask(FROM_HERE, get_payload_lambda);
  run_loop.Run();
  return response;
}

int PageLoadInProcessFuzzer::Fuzz(const uint8_t* data, size_t size) {
  FuzzCase fuzz_case;
  fuzz_case.ParseFromArray(data, size);

  GURL test_url;

  std::string content_type = fuzz_case.content_type();
  std::string body = fuzz_case.body();

  if (fuzz_case.has_network_load()) {
    // Load the page over a network. Slow but we can provide lots of options
    // and realism.
    const auto& network_load = fuzz_case.network_load();
    std::string path = network_load.path();
    if (network_load.https()) {
      test_url = https_test_server_.GetURL(path);
    } else {
      test_url = http_test_server_.GetURL(path);
    }

    std::unique_ptr<net::test_server::BasicHttpResponse> response =
        std::make_unique<net::test_server::BasicHttpResponse>();

    // Now figure out what sort of response the server will provide
    response->set_code(
        static_cast<net::HttpStatusCode>(network_load.http_status()));
    response->set_content_type(content_type);
    for (const auto& header : network_load.custom_headers()) {
      response->AddCustomHeader(header.key(), header.value());
    }
    if (network_load.has_reason()) {
      response->set_reason(network_load.reason());
    }
    http_response_ = std::move(response);
  } else {
    // Request via a data: URI which should be quicker.
    test_url = GURL(base::StrCat({"data:", content_type, ";charset=utf-8,",
                                  base::EscapeQueryParamValue(body, false)}));
  }
  base::IgnoreResult(ui_test_utils::NavigateToURL(browser(), test_url));
  return 0;
}
