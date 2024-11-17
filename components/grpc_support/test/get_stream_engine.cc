// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "components/grpc_support/test/get_stream_engine.h"

#include "base/lazy_instance.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_pump_type.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "components/grpc_support/include/bidirectional_stream_c.h"
#include "net/base/host_port_pair.h"
#include "net/base/network_isolation_key.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/dns/mapped_host_resolver.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_network_session.h"
#include "net/http/http_server_properties.h"
#include "net/test/cert_test_util.h"
#include "net/test/quic_simple_test_server.h"
#include "net/test/test_data_directory.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"

namespace grpc_support {
namespace {

// URLRequestContextGetter for BidirectionalStreamTest. This is used instead of
// net::TestURLRequestContextGetter because the URLRequestContext needs to be
// created on the test_io_thread_ for the test, and TestURLRequestContextGetter
// does not allow for lazy instantiation of the URLRequestContext if additional
// setup is required.
class BidirectionalStreamTestURLRequestContextGetter
    : public net::URLRequestContextGetter {
 public:
  explicit BidirectionalStreamTestURLRequestContextGetter(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner)
      : task_runner_(task_runner) {}

  BidirectionalStreamTestURLRequestContextGetter(
      const BidirectionalStreamTestURLRequestContextGetter&) = delete;
  BidirectionalStreamTestURLRequestContextGetter& operator=(
      const BidirectionalStreamTestURLRequestContextGetter&) = delete;

  net::URLRequestContext* GetURLRequestContext() override {
    if (!request_context_) {
      auto context_builder = net::CreateTestURLRequestContextBuilder();
      auto mock_host_resolver = std::make_unique<net::MockHostResolver>();
      auto host_resolver = std::make_unique<net::MappedHostResolver>(
          std::move(mock_host_resolver));
      auto test_cert = net::ImportCertFromFile(net::GetTestCertsDirectory(),
                                               "quic-chain.pem");
      auto mock_cert_verifier = std::make_unique<net::MockCertVerifier>();
      net::CertVerifyResult verify_result;
      verify_result.verified_cert = test_cert;
      verify_result.is_issued_by_known_root = true;
      mock_cert_verifier->AddResultForCert(test_cert, verify_result, net::OK);

      net::HttpNetworkSessionParams params;
      params.enable_quic = true;
      params.enable_http2 = true;

      context_builder->SetCertVerifier(std::move(mock_cert_verifier));
      context_builder->set_host_resolver(std::move(host_resolver));
      context_builder->set_http_network_session_params(params);
      request_context_ = context_builder->Build();
      UpdateHostResolverRules();

      // Need to enable QUIC for the test server.
      net::AlternativeService alternative_service(net::kProtoQUIC, "", 443);
      url::SchemeHostPort quic_hint_server(
          "https", net::QuicSimpleTestServer::GetHost(), 443);
      request_context_->http_server_properties()->SetQuicAlternativeService(
          quic_hint_server, net::NetworkAnonymizationKey(), alternative_service,
          base::Time::Max(), quic::ParsedQuicVersionVector());
    }
    return request_context_.get();
  }

  net::MappedHostResolver* host_resolver() {
    if (!request_context_) {
      return nullptr;
    }
    // This is safe because we set a MappedHostResolver in
    // GetURLRequestContext().
    return static_cast<net::MappedHostResolver*>(
        request_context_->host_resolver());
  }

  scoped_refptr<base::SingleThreadTaskRunner> GetNetworkTaskRunner()
      const override {
    return task_runner_;
  }

  void SetTestServerPort(int port) {
    test_server_port_ = port;
    UpdateHostResolverRules();
  }

 private:
  void UpdateHostResolverRules() {
    if (!host_resolver())
      return;
    host_resolver()->SetRulesFromString(
        base::StringPrintf("MAP notfound.example.com ^NOTFOUND,"
                           "MAP test.example.com 127.0.0.1:%d",
                           test_server_port_));
  }
  ~BidirectionalStreamTestURLRequestContextGetter() override = default;

  int test_server_port_;
  std::unique_ptr<net::URLRequestContext> request_context_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

class TestStreamEngineGetterImpl : public TestStreamEngineGetter {
 public:
  explicit TestStreamEngineGetterImpl(int port)
      : thread_("grpc_support_test_io_thread") {
    base::Thread::Options options;
    options.message_pump_type = base::MessagePumpType::IO;
    bool started = thread_.StartWithOptions(std::move(options));
    CHECK(started);

    request_context_getter_ =
        base::MakeRefCounted<BidirectionalStreamTestURLRequestContextGetter>(
            thread_.task_runner());
    request_context_getter_->SetTestServerPort(port);
    engine_.obj = request_context_getter_.get();
  }

  ~TestStreamEngineGetterImpl() override = default;

  stream_engine* Get() override { return &engine_; }

 private:
  base::Thread thread_;
  scoped_refptr<BidirectionalStreamTestURLRequestContextGetter>
      request_context_getter_;
  stream_engine engine_ = {};
};

}  // namespace

// WARNING: An alternative implementation of Create() exists in
// //components/cronet/native/test/test_stream_engine.cc. They are never both
// linked into the same binary.

// static
std::unique_ptr<TestStreamEngineGetter> TestStreamEngineGetter::Create(
    int port) {
  return std::make_unique<TestStreamEngineGetterImpl>(port);
}

}  // namespace grpc_support
