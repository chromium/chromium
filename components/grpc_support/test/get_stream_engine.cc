// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "components/grpc_support/test/get_stream_engine.h"

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_pump_type.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread.h"
#include "components/grpc_support/include/bidirectional_stream_c.h"
#include "net/base/host_port_pair.h"
#include "net/base/network_isolation_key.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/dns/mapped_host_resolver.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_server_properties.h"
#include "net/test/cert_test_util.h"
#include "net/test/quic_simple_test_server.h"
#include "net/test/test_data_directory.h"
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
  BidirectionalStreamTestURLRequestContextGetter(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner)
      : task_runner_(task_runner) {}

  net::URLRequestContext* GetURLRequestContext() override {
    if (!request_context_) {
      request_context_.reset(
          new net::TestURLRequestContext(true /* delay_initialization */));
      auto mock_host_resolver = std::make_unique<net::MockHostResolver>();
      host_resolver_.reset(
          new net::MappedHostResolver(std::move(mock_host_resolver)));
      UpdateHostResolverRules();
      auto test_cert = net::ImportCertFromFile(net::GetTestCertsDirectory(),
                                               "quic-chain.pem");
      mock_cert_verifier_.reset(new net::MockCertVerifier());
      net::CertVerifyResult verify_result;
      verify_result.verified_cert = test_cert;
      verify_result.is_issued_by_known_root = true;
      mock_cert_verifier_->AddResultForCert(test_cert, verify_result, net::OK);

      auto params = std::make_unique<net::HttpNetworkSession::Params>();
      params->enable_quic = true;
      params->enable_http2 = true;
      request_context_->set_cert_verifier(mock_cert_verifier_.get());
      request_context_->set_host_resolver(host_resolver_.get());
      request_context_->set_http_network_session_params(std::move(params));

      request_context_->Init();

      // Need to enable QUIC for the test server.
      net::AlternativeService alternative_service(net::kProtoQUIC, "", 443);
      url::SchemeHostPort quic_hint_server(
          "https", net::QuicSimpleTestServer::GetHost(), 443);
      request_context_->http_server_properties()->SetQuicAlternativeService(
          quic_hint_server, net::NetworkIsolationKey(), alternative_service,
          base::Time::Max(), quic::ParsedQuicVersionVector());
    }
    return request_context_.get();
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
    if (!host_resolver_)
      return;
    host_resolver_->SetRulesFromString(
        base::StringPrintf("MAP notfound.example.com ~NOTFOUND,"
                           "MAP test.example.com 127.0.0.1:%d",
                           test_server_port_));
  }
  ~BidirectionalStreamTestURLRequestContextGetter() override {}

  int test_server_port_;
  std::unique_ptr<net::MockCertVerifier> mock_cert_verifier_;
  std::unique_ptr<net::MappedHostResolver> host_resolver_;
  std::unique_ptr<net::TestURLRequestContext> request_context_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(BidirectionalStreamTestURLRequestContextGetter);
};

base::LazyInstance<
    scoped_refptr<BidirectionalStreamTestURLRequestContextGetter>>
    ::Leaky g_request_context_getter_ = LAZY_INSTANCE_INITIALIZER;
bool g_initialized_ = false;

}  // namespace

void CreateRequestContextGetterIfNecessary() {
  if (!g_initialized_) {
    g_initialized_ = true;
    static base::Thread* test_io_thread_ =
        new base::Thread("grpc_support_test_io_thread");
    base::Thread::Options options;
    options.message_pump_type = base::MessagePumpType::IO;
    bool started = test_io_thread_->StartWithOptions(options);
    DCHECK(started);

    g_request_context_getter_.Get() =
        new BidirectionalStreamTestURLRequestContextGetter(
            test_io_thread_->task_runner());
  }
}

stream_engine* GetTestStreamEngine(int port) {
  CreateRequestContextGetterIfNecessary();
  g_request_context_getter_.Get()->SetTestServerPort(port);
  static stream_engine engine;
  engine.obj = g_request_context_getter_.Get().get();
  return &engine;
}

void StartTestStreamEngine(int port) {}
void ShutdownTestStreamEngine() {}

}  // namespace grpc_support
