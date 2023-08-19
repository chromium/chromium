// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/test/bind.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/fuzzing/in_process_fuzzer.h"
#include "content/public/browser/browser_thread.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

// This is an example use of the InProcessFuzzer framework.
// It fetches arbitrary HTML from an HTTPS server. It's not really
// intended to be an effective fuzzer, but just to show an example
// of how this framework can be used.

class HtmlInProcessFuzzer : virtual public InProcessFuzzer {
 public:
  HtmlInProcessFuzzer()
      : http_test_server_(net::EmbeddedTestServer::TYPE_HTTP) {}

  void SetUpOnMainThread() override;
  int Fuzz(const uint8_t* data, size_t size) override;
  static std::unique_ptr<net::test_server::HttpResponse> HandleHTTPRequest(
      base::WeakPtr<HtmlInProcessFuzzer> fuzzer_weak,
      const net::test_server::HttpRequest& request);

  net::EmbeddedTestServer http_test_server_;
  std::string current_fuzz_case_;
  base::WeakPtrFactory<HtmlInProcessFuzzer> weak_ptr_factory_{this};
};

REGISTER_IN_PROCESS_FUZZER(HtmlInProcessFuzzer)

void HtmlInProcessFuzzer::SetUpOnMainThread() {
  InProcessFuzzer::SetUpOnMainThread();
  host_resolver()->AddRule("*", "127.0.0.1");
  // Previous versions of this fuzzer used HTTPS, but on ClusterFuzz,
  // data_deps are not available and thus the SSL config did not work.
  // For now, use simple HTTP.
  // TODO(crbug.com/1463972)
  // http_test_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
  http_test_server_.RegisterRequestHandler(base::BindRepeating(
      &HtmlInProcessFuzzer::HandleHTTPRequest, weak_ptr_factory_.GetWeakPtr()));
  ASSERT_TRUE(http_test_server_.Start());
}

std::unique_ptr<net::test_server::HttpResponse>
HtmlInProcessFuzzer::HandleHTTPRequest(
    base::WeakPtr<HtmlInProcessFuzzer> fuzzer_weak,
    const net::test_server::HttpRequest& request) {
  std::unique_ptr<net::test_server::BasicHttpResponse> response;
  response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_content_type("text/html");
  std::string response_body = "";
  // We are running on the embedded test server's thread.
  // We want to ask the fuzzer thread for the latest HTML payload,
  // but there's a risk of UaF if it's being destroyed.
  // We use a weak pointer, but we have to dereference that on the originating
  // thread.
  base::RunLoop run_loop;
  base::RepeatingCallback<void()> get_payload_lambda =
      base::BindLambdaForTesting([&]() {
        HtmlInProcessFuzzer* fuzzer = fuzzer_weak.get();
        if (fuzzer) {
          response_body = fuzzer->current_fuzz_case_;
        }
        run_loop.Quit();
      });
  content::GetUIThreadTaskRunner()->PostTask(FROM_HERE, get_payload_lambda);
  run_loop.Run();
  response->set_content(response_body);
  response->set_code(net::HTTP_OK);
  return response;
}

int HtmlInProcessFuzzer::Fuzz(const uint8_t* data, size_t size) {
  std::string html_string(reinterpret_cast<const char*>(data), size);
  current_fuzz_case_ = html_string;
  GURL test_url = http_test_server_.GetURL("/test.html");

  base::RunLoop run_loop;
  base::RepeatingCallback<void()> run_fuzz_case_lambda =
      base::BindLambdaForTesting([&]() {
        base::IgnoreResult(ui_test_utils::NavigateToURL(browser(), test_url));
        run_loop.Quit();
      });
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_fuzz_case_lambda);
  run_loop.Run();
  return 0;
}
