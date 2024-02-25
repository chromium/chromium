// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/native/test/test_util.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "components/cronet/native/generated/cronet.idl_c.h"
#include "net/base/net_errors.h"
#include "net/cert/mock_cert_verifier.h"

namespace {
// Implementation of PostTaskExecutor methods.
void TestExecutor_Execute(Cronet_ExecutorPtr self,
                          Cronet_RunnablePtr runnable) {
  CHECK(self);
  DVLOG(1) << "Post Task";
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, cronet::test::RunnableWrapper::CreateOnceClosure(runnable));
}

// Test Cert Verifier that successfully verifies any cert from test.example.com.
class TestCertVerifier : public net::MockCertVerifier {
 public:
  TestCertVerifier() = default;
  ~TestCertVerifier() override = default;

  // CertVerifier implementation
  int Verify(const RequestParams& params,
             net::CertVerifyResult* verify_result,
             net::CompletionOnceCallback callback,
             std::unique_ptr<Request>* out_req,
             const net::NetLogWithSource& net_log) override {
    verify_result->Reset();
    if (params.hostname() == "test.example.com") {
      verify_result->verified_cert = params.certificate();
      verify_result->is_issued_by_known_root = true;
      return net::OK;
    }
    return net::MockCertVerifier::Verify(params, verify_result,
                                         std::move(callback), out_req, net_log);
  }
};

}  // namespace

namespace cronet {
namespace test {

Cronet_EnginePtr CreateTestEngine(int quic_server_port) {
  Cronet_EngineParamsPtr engine_params = Cronet_EngineParams_Create();
  Cronet_EngineParams_user_agent_set(engine_params, "test");
  // Add Host Resolver Rules.
  std::string host_resolver_rules = base::StringPrintf(
      "MAP test.example.com 127.0.0.1:%d,"
      "MAP notfound.example.com ^NOTFOUND",
      quic_server_port);
  Cronet_EngineParams_experimental_options_set(
      engine_params,
      base::StringPrintf(
          "{ \"HostResolverRules\": { \"host_resolver_rules\" : \"%s\" } }",
          host_resolver_rules.c_str())
          .c_str());
  // Enable QUIC.
  Cronet_EngineParams_enable_quic_set(engine_params, true);
  // Add QUIC Hint.
  Cronet_QuicHintPtr quic_hint = Cronet_QuicHint_Create();
  Cronet_QuicHint_host_set(quic_hint, "test.example.com");
  Cronet_QuicHint_port_set(quic_hint, 443);
  Cronet_QuicHint_alternate_port_set(quic_hint, 443);
  Cronet_EngineParams_quic_hints_add(engine_params, quic_hint);
  Cronet_QuicHint_Destroy(quic_hint);
  // Create Cronet Engine.
  Cronet_EnginePtr cronet_engine = Cronet_Engine_Create();
  // Set Mock Cert Verifier.
  auto cert_verifier = std::make_unique<TestCertVerifier>();
  Cronet_Engine_SetMockCertVerifierForTesting(cronet_engine,
                                              cert_verifier.release());
  // Start Cronet Engine.
  Cronet_Engine_StartWithParams(cronet_engine, engine_params);
  Cronet_EngineParams_Destroy(engine_params);
  return cronet_engine;
}

Cronet_ExecutorPtr CreateTestExecutor() {
  return Cronet_Executor_CreateWith(TestExecutor_Execute);
}

// static
base::OnceClosure RunnableWrapper::CreateOnceClosure(
    Cronet_RunnablePtr runnable) {
  return base::BindOnce(&RunnableWrapper::Run,
                        std::make_unique<RunnableWrapper>(runnable));
}

}  // namespace test
}  // namespace cronet
