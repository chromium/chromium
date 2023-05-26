// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/fuzzing/kombucha_in_process_fuzzer.pb.h"

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/fuzzing/in_process_fuzzer.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/browser/browser_thread.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

// At the moment, this is an example use of the InProcessFuzzer framework
// that uses Kombucha + protos. It's not yet intended to be an effective fuzzer,
// but just to be the skeleton of how this framework can be used.

#define DEFINE_BINARY_PROTO_IN_PROCESS_FUZZER(arg) \
  DEFINE_PROTO_FUZZER_IN_PROCESS_IMPL(true, arg)

#define DEFINE_PROTO_FUZZER_IN_PROCESS_IMPL(use_binary, arg)      \
  static void TestOneProtoInput(arg);                             \
  using FuzzerProtoType =                                         \
      protobuf_mutator::libfuzzer::macro_internal::GetFirstParam< \
          decltype(&TestOneProtoInput)>::type;                    \
  DEFINE_CUSTOM_PROTO_MUTATOR_IMPL(use_binary, FuzzerProtoType)   \
  DEFINE_CUSTOM_PROTO_CROSSOVER_IMPL(use_binary, FuzzerProtoType) \
  DEFINE_POST_PROCESS_PROTO_MUTATION_IMPL(FuzzerProtoType)

class KombuchaInProcessFuzzer
    : virtual public InteractiveBrowserTestT<InProcessFuzzer> {
 public:
  using KombuchaTestCase = chrome::test::fuzzing::kombucha_in_process_fuzzer::
      proto::KombuchaTestcase;
  void SetUpOnMainThread() override;
  int Fuzz(const uint8_t* data, size_t size) override;
  static std::unique_ptr<net::test_server::HttpResponse> HandleHTTPRequest(
      base::WeakPtr<KombuchaInProcessFuzzer> fuzzer_weak,
      const net::test_server::HttpRequest& request);

  KombuchaTestCase current_fuzz_case_;
  base::WeakPtrFactory<KombuchaInProcessFuzzer> weak_ptr_factory_{this};
};

void KombuchaInProcessFuzzer::SetUpOnMainThread() {
  InteractiveBrowserTestT::SetUpOnMainThread();
  host_resolver()->AddRule("*", "127.0.0.1");
  embedded_test_server()->SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
  embedded_test_server()->RegisterRequestHandler(
      base::BindRepeating(&KombuchaInProcessFuzzer::HandleHTTPRequest,
                          weak_ptr_factory_.GetWeakPtr()));
  ASSERT_TRUE(embedded_test_server()->Start());
}

std::unique_ptr<net::test_server::HttpResponse>
KombuchaInProcessFuzzer::HandleHTTPRequest(
    base::WeakPtr<KombuchaInProcessFuzzer> fuzzer_weak,
    const net::test_server::HttpRequest& request) {
  std::unique_ptr<net::test_server::BasicHttpResponse> response;
  response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_content_type("application/x-protobuf");
  KombuchaTestCase testcase;
  // We are running on the embedded test server's thread.
  // We want to ask the fuzzer thread for the latest payload,
  // but there's a risk of UaF if it's being destroyed.
  // We use a weak pointer, but we have to dereference that on the originating
  // thread.
  base::RunLoop run_loop;
  base::RepeatingCallback<void()> get_payload_lambda =
      base::BindLambdaForTesting([&]() {
        KombuchaInProcessFuzzer* fuzzer = fuzzer_weak.get();
        if (fuzzer) {
          testcase = fuzzer->current_fuzz_case_;
        }
        run_loop.Quit();
      });
  content::GetUIThreadTaskRunner()->PostTask(FROM_HERE, get_payload_lambda);
  run_loop.Run();
  response->set_content(testcase.SerializeAsString());
  response->set_code(net::HTTP_OK);
  return response;
}

int KombuchaInProcessFuzzer::Fuzz(const uint8_t* data, size_t size) {
  KombuchaTestCase proto_testcase;
  proto_testcase.ParseFromArray(data, size);
  current_fuzz_case_ = proto_testcase;

  // The following does not make use of data and size in any way.
  // This state is temporary; Fuzz should be updated to use the provided data.
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kPrimaryTabElementId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondaryTabElementId);
  GURL test_url = embedded_test_server()->GetURL("/test.html");
  RunTestSequence(
      InstrumentTab(kPrimaryTabElementId, 0),
      PressButton(kNewTabButtonElementId),
      AddInstrumentedTab(kSecondaryTabElementId, GURL("about:blank")),
      // Only the following step requires the webserver.
      NavigateWebContents(kSecondaryTabElementId, test_url));
  return 0;
}

REGISTER_IN_PROCESS_FUZZER(KombuchaInProcessFuzzer)
