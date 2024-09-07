// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <fuzzer/FuzzedDataProvider.h>
#include <google/protobuf/descriptor.h>
#include <memory>
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/fuzzing/in_process_proto_fuzzer.h"
#include "chrome/test/fuzzing/page_load_in_process_fuzzer.pb.h"
#include "content/public/browser/browser_thread.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_response.h"

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

class PageLoadInProcessFuzzer
    : public InProcessProtoFuzzer<test::fuzzing::page_load_fuzzing::FuzzCase> {
 public:
  using WhichServer = test::fuzzing::page_load_fuzzing::WhichServer;
  PageLoadInProcessFuzzer();

  void SetUpOnMainThread() override;
  int Fuzz(
      const test::fuzzing::page_load_fuzzing::FuzzCase& fuzz_case) override;

 private:
  static std::unique_ptr<net::test_server::HttpResponse> HandleHTTPRequest(
      base::WeakPtr<PageLoadInProcessFuzzer> fuzzer_weak,

      WhichServer which_server,
      const net::test_server::HttpRequest& request);
  std::unique_ptr<net::test_server::BasicHttpResponse> DoHandleHTTPRequest(
      WhichServer which_server,
      const net::test_server::HttpRequest& request);
  std::string SubstituteServersInBody(const std::string& body);
  static void SubstituteServerPattern(std::string* haystack,
                                      const std::string& pattern,
                                      const net::EmbeddedTestServer& server);

 private:
  // To test cross-origin cases, we have four servers listening
  // on different ports.
  net::EmbeddedTestServer http_test_server1_;
  net::EmbeddedTestServer http_test_server2_;
  net::EmbeddedTestServer https_test_server1_;
  net::EmbeddedTestServer https_test_server2_;
  test::fuzzing::page_load_fuzzing::FuzzCase fuzz_case_;
  base::WeakPtrFactory<PageLoadInProcessFuzzer> weak_ptr_factory_{this};
};

REGISTER_TEXT_PROTO_IN_PROCESS_FUZZER(PageLoadInProcessFuzzer)

PageLoadInProcessFuzzer::PageLoadInProcessFuzzer()
    : InProcessProtoFuzzer({
          RunLoopTimeoutBehavior::kDeclareInfiniteLoop,
          base::Seconds(180),
      }),
      http_test_server1_(net::EmbeddedTestServer::TYPE_HTTP),
      http_test_server2_(net::EmbeddedTestServer::TYPE_HTTP),
      https_test_server1_(net::EmbeddedTestServer::TYPE_HTTPS),
      https_test_server2_(net::EmbeddedTestServer::TYPE_HTTPS) {
  https_test_server1_.SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
  https_test_server2_.SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
  http_test_server1_.RegisterRequestHandler(base::BindRepeating(
      &PageLoadInProcessFuzzer::HandleHTTPRequest,
      weak_ptr_factory_.GetWeakPtr(), WhichServer::HTTP_ORIGIN1));
  https_test_server1_.RegisterRequestHandler(base::BindRepeating(
      &PageLoadInProcessFuzzer::HandleHTTPRequest,
      weak_ptr_factory_.GetWeakPtr(), WhichServer::HTTPS_ORIGIN1));
  http_test_server2_.RegisterRequestHandler(base::BindRepeating(
      &PageLoadInProcessFuzzer::HandleHTTPRequest,
      weak_ptr_factory_.GetWeakPtr(), WhichServer::HTTP_ORIGIN2));
  https_test_server2_.RegisterRequestHandler(base::BindRepeating(
      &PageLoadInProcessFuzzer::HandleHTTPRequest,
      weak_ptr_factory_.GetWeakPtr(), WhichServer::HTTPS_ORIGIN2));
}

void PageLoadInProcessFuzzer::SetUpOnMainThread() {
  InProcessFuzzer::SetUpOnMainThread();
  host_resolver()->AddRule("*", "127.0.0.1");
  CHECK(http_test_server1_.Start());
  CHECK(http_test_server2_.Start());
  CHECK(https_test_server1_.Start());
  CHECK(https_test_server2_.Start());
}

std::unique_ptr<net::test_server::HttpResponse>
PageLoadInProcessFuzzer::HandleHTTPRequest(
    base::WeakPtr<PageLoadInProcessFuzzer> fuzzer_weak,
    WhichServer which_server,
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
          response = fuzzer->DoHandleHTTPRequest(which_server, request);
        }
        run_loop.Quit();
      });
  content::GetUIThreadTaskRunner()->PostTask(FROM_HERE, get_payload_lambda);
  run_loop.Run();
  return response;
}

std::unique_ptr<net::test_server::BasicHttpResponse>
PageLoadInProcessFuzzer::DoHandleHTTPRequest(
    WhichServer which_server,
    const net::test_server::HttpRequest& request) {
  // Look through all the network resources given in the fuzz case and build
  // a response if we find one.
  for (const auto& network_resource : fuzz_case_.network_resource()) {
    if (network_resource.which_server() == which_server &&
        request.relative_url.substr(1) == network_resource.path()) {
      std::unique_ptr<net::test_server::BasicHttpResponse> response =
          std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_code(
          static_cast<net::HttpStatusCode>(network_resource.http_status()));
      response->set_content_type(network_resource.content_type());
      for (const auto& header : network_resource.custom_headers()) {
        response->AddCustomHeader(header.key(), header.value());
      }
      response->set_reason(network_resource.reason());
      if (network_resource.has_body()) {
        response->set_content(SubstituteServersInBody(network_resource.body()));
      }
      return response;
    }
  }
  return nullptr;
}

int PageLoadInProcessFuzzer::Fuzz(
    const test::fuzzing::page_load_fuzzing::FuzzCase& fuzz_case) {
  fuzz_case_ = fuzz_case;

  GURL test_url;

  if (fuzz_case_.has_data_uri_navigation()) {
    const auto& data_uri_navigation = fuzz_case_.data_uri_navigation();
    std::string content_type = data_uri_navigation.content_type();
    std::string body = SubstituteServersInBody(data_uri_navigation.body());
    // Request via a data: URI which should be quickest.
    test_url = GURL(base::StrCat({"data:", content_type, ";charset=utf-8,",
                                  base::EscapeQueryParamValue(body, false)}));
  } else {
    // We navigate to the first server resource listed.
    if (fuzz_case_.network_resource_size() < 1) {
      return -1;  // invalid fuzz case.
    }
    const auto& network_resource = fuzz_case_.network_resource(0);
    std::string path = "/" + network_resource.path();
    switch (network_resource.which_server()) {
      case WhichServer::HTTP_ORIGIN1:
        test_url = http_test_server1_.GetURL(path);
        break;
      case WhichServer::HTTP_ORIGIN2:
        test_url = http_test_server2_.GetURL(path);
        break;
      case WhichServer::HTTPS_ORIGIN1:
        test_url = https_test_server1_.GetURL(path);
        break;
      case WhichServer::HTTPS_ORIGIN2:
        test_url = https_test_server2_.GetURL(path);
        break;
      default:
        LOG(FATAL) << "Unexpected proto value for which server";
    }
  }

  base::IgnoreResult(ui_test_utils::NavigateToURL(browser(), test_url));
  return 0;
}

void PageLoadInProcessFuzzer::SubstituteServerPattern(
    std::string* body,
    const std::string& pattern,
    const net::EmbeddedTestServer& server) {
  std::string url = server.GetURL("/").spec();
  url.pop_back();  // remove trailing /
  base::ReplaceSubstringsAfterOffset(body, 0, pattern, url);
}

std::string PageLoadInProcessFuzzer::SubstituteServersInBody(
    const std::string& body) {
  std::string result = body;
  SubstituteServerPattern(&result, "$HTTP_ORIGIN1", http_test_server1_);
  SubstituteServerPattern(&result, "$HTTP_ORIGIN2", http_test_server2_);
  SubstituteServerPattern(&result, "$HTTPS_ORIGIN1", https_test_server1_);
  SubstituteServerPattern(&result, "$HTTPS_ORIGIN2", https_test_server2_);
  return result;
}
