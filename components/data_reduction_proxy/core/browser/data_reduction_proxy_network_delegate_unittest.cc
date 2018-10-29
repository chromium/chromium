// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_network_delegate.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/field_trial.h"
#include "base/numerics/safe_conversions.h"
#include "base/optional.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/safe_sprintf.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_entropy_provider.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_data.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_metrics.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_request_options.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_util.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_features.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_server.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "components/data_reduction_proxy/core/common/lofi_decider.h"
#include "components/data_reduction_proxy/proto/client_config.pb.h"
#include "components/data_reduction_proxy/proto/data_store.pb.h"
#include "components/previews/core/previews_experiments.h"
#include "components/previews/core/previews_features.h"
#include "components/previews/core/test_previews_decider.h"
#include "net/base/host_port_pair.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/proxy_server.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/nqe/effective_connection_type.h"
#include "net/proxy_resolution/proxy_config.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "net/proxy_resolution/proxy_retry_info.h"
#include "net/socket/socket_test_util.h"
#include "net/test/cert_test_util.h"
#include "net/test/gtest_util.h"
#include "net/test/test_data_directory.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "net/url_request/url_request_status.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/test/test_network_quality_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace data_reduction_proxy {
namespace {

using TestNetworkDelegate = net::NetworkDelegateImpl;

const char kOtherProxy[] = "testproxy:17";

const char kTestURL[] = "http://www.google.com/";
const char kSecureTestURL[] = "https://www.google.com/";

const char kOriginalValidOCLHistogramName[] =
    "Net.HttpOriginalContentLengthWithValidOCL";
const char kDifferenceValidOCLHistogramName[] =
    "Net.HttpContentLengthDifferenceWithValidOCL";

// HTTP original content length
const char kOriginalInsecureDirectHistogramName[] =
    "Net.HttpOriginalContentLengthV2.Http.Direct";
const char kOriginalInsecureViaDRPHistogramName[] =
    "Net.HttpOriginalContentLengthV2.Http.ViaDRP";
const char kOriginalInsecureBypassedHistogramName[] =
    "Net.HttpOriginalContentLengthV2.Http.BypassedDRP";
const char kOriginalInsecureOtherHistogramName[] =
    "Net.HttpOriginalContentLengthV2.Http.Other";
// HTTP video original content length
const char kOriginalVideoInsecureDirectHistogramName[] =
    "Net.HttpOriginalContentLengthV2.Http.Direct.Video";
const char kOriginalVideoInsecureViaDRPHistogramName[] =
    "Net.HttpOriginalContentLengthV2.Http.ViaDRP.Video";
const char kOriginalVideoInsecureBypassedHistogramName[] =
    "Net.HttpOriginalContentLengthV2.Http.BypassedDRP.Video";
const char kOriginalVideoInsecureOtherHistogramName[] =
    "Net.HttpOriginalContentLengthV2.Http.Other.Video";
// HTTPS original content length
const char kOriginalSecureDirectHistogramName[] =
    "Net.HttpOriginalContentLengthV2.Https.Direct";
const char kOriginalSecureViaDRPHistogramName[] =
    "Net.HttpOriginalContentLengthV2.Https.ViaDRP";
const char kOriginalSecureBypassedHistogramName[] =
    "Net.HttpOriginalContentLengthV2.Https.BypassedDRP";
const char kOriginalSecureOtherHistogramName[] =
    "Net.HttpOriginalContentLengthV2.Https.Other";
// HTTPS video original content length
const char kOriginalVideoSecureDirectHistogramName[] =
    "Net.HttpOriginalContentLengthV2.Https.Direct.Video";
const char kOriginalVideoSecureViaDRPHistogramName[] =
    "Net.HttpOriginalContentLengthV2.Https.ViaDRP.Video";
const char kOriginalVideoSecureBypassedHistogramName[] =
    "Net.HttpOriginalContentLengthV2.Https.BypassedDRP.Video";
const char kOriginalVideoSecureOtherHistogramName[] =
    "Net.HttpOriginalContentLengthV2.Https.Other.Video";

const char kReceivedHistogramName[] = "Net.HttpContentLength";
const char kReceivedInsecureDirectHistogramName[] =
    "Net.HttpContentLengthV2.Http.Direct";
const char kReceivedInsecureViaDRPHistogramName[] =
    "Net.HttpContentLengthV2.Http.ViaDRP";
const char kReceivedInsecureBypassedHistogramName[] =
    "Net.HttpContentLengthV2.Http.BypassedDRP";
const char kReceivedInsecureOtherHistogramName[] =
    "Net.HttpContentLengthV2.Http.Other";
const char kReceivedSecureDirectHistogramName[] =
    "Net.HttpContentLengthV2.Https.Direct";
const char kReceivedSecureViaDRPHistogramName[] =
    "Net.HttpContentLengthV2.Https.ViaDRP";
const char kReceivedSecureBypassedHistogramName[] =
    "Net.HttpContentLengthV2.Https.BypassedDRP";
const char kReceivedSecureOtherHistogramName[] =
    "Net.HttpContentLengthV2.Https.Other";
const char kReceivedVideoInsecureDirectHistogramName[] =
    "Net.HttpContentLengthV2.Http.Direct.Video";
const char kReceivedVideoInsecureViaDRPHistogramName[] =
    "Net.HttpContentLengthV2.Http.ViaDRP.Video";
const char kReceivedVideoInsecureBypassedHistogramName[] =
    "Net.HttpContentLengthV2.Http.BypassedDRP.Video";
const char kReceivedVideoInsecureOtherHistogramName[] =
    "Net.HttpContentLengthV2.Http.Other.Video";
const char kReceivedVideoSecureDirectHistogramName[] =
    "Net.HttpContentLengthV2.Https.Direct.Video";
const char kReceivedVideoSecureViaDRPHistogramName[] =
    "Net.HttpContentLengthV2.Https.ViaDRP.Video";
const char kReceivedVideoSecureBypassedHistogramName[] =
    "Net.HttpContentLengthV2.Https.BypassedDRP.Video";
const char kReceivedVideoSecureOtherHistogramName[] =
    "Net.HttpContentLengthV2.Https.Other.Video";
const char kFreshnessLifetimeHistogramName[] =
    "Net.HttpContentFreshnessLifetime";
const int64_t kResponseContentLength = 100;
const int64_t kOriginalContentLength = 200;

#if defined(OS_ANDROID)
const Client kClient = Client::CHROME_ANDROID;
#elif defined(OS_IOS)
const Client kClient = Client::CHROME_IOS;
#elif defined(OS_MACOSX)
const Client kClient = Client::CHROME_MAC;
#elif defined(OS_CHROMEOS)
const Client kClient = Client::CHROME_CHROMEOS;
#elif defined(OS_LINUX)
const Client kClient = Client::CHROME_LINUX;
#elif defined(OS_WIN)
const Client kClient = Client::CHROME_WINDOWS;
#elif defined(OS_FREEBSD)
const Client kClient = Client::CHROME_FREEBSD;
#elif defined(OS_OPENBSD)
const Client kClient = Client::CHROME_OPENBSD;
#elif defined(OS_SOLARIS)
const Client kClient = Client::CHROME_SOLARIS;
#elif defined(OS_QNX)
const Client kClient = Client::CHROME_QNX;
#else
const Client kClient = Client::UNKNOWN;
#endif

class TestLoFiDecider : public LoFiDecider {
 public:
  TestLoFiDecider()
      : should_be_client_lofi_(false),
        should_be_client_lofi_auto_reload_(false),
        should_request_lofi_resource_(false),
        ignore_is_using_data_reduction_proxy_check_(false) {}
  ~TestLoFiDecider() override {}

  void SetIsUsingLoFi(bool should_request_lofi_resource) {
    should_request_lofi_resource_ = should_request_lofi_resource;
  }

  void SetIsUsingClientLoFi(bool should_be_client_lofi) {
    should_be_client_lofi_ = should_be_client_lofi;
  }

  void SetIsClientLoFiAutoReload(bool should_be_client_lofi_auto_reload) {
    should_be_client_lofi_auto_reload_ = should_be_client_lofi_auto_reload;
  }

  void MaybeSetAcceptTransformHeader(
      const net::URLRequest& request,
      net::HttpRequestHeaders* headers) const override {
    if (should_request_lofi_resource_) {
      headers->SetHeader(chrome_proxy_accept_transform_header(),
                         empty_image_directive());
    }
  }

  void RemoveAcceptTransformHeader(
      net::HttpRequestHeaders* headers) const override {
    if (ignore_is_using_data_reduction_proxy_check_)
      return;
    headers->RemoveHeader(chrome_proxy_accept_transform_header());
  }

  bool IsClientLoFiImageRequest(const net::URLRequest& request) const override {
    return should_be_client_lofi_;
  }

  bool IsClientLoFiAutoReloadRequest(
      const net::URLRequest& request) const override {
    return should_be_client_lofi_auto_reload_;
  }

  void ignore_is_using_data_reduction_proxy_check() {
    ignore_is_using_data_reduction_proxy_check_ = true;
  }

 private:
  bool should_be_client_lofi_;
  bool should_be_client_lofi_auto_reload_;
  bool should_request_lofi_resource_;
  bool ignore_is_using_data_reduction_proxy_check_;
};

class TestLoFiUIService : public LoFiUIService {
 public:
  TestLoFiUIService() : on_lofi_response_(false) {}
  ~TestLoFiUIService() override {}

  bool DidNotifyLoFiResponse() const { return on_lofi_response_; }

  void OnLoFiReponseReceived(const net::URLRequest& request) override {
    on_lofi_response_ = true;
  }

  void ClearResponse() { on_lofi_response_ = false; }

 private:
  bool on_lofi_response_;
};

class TestResourceTypeProvider : public ResourceTypeProvider {
 public:
  void SetContentType(const net::URLRequest& request) override {}

  ContentType GetContentType(const GURL& url) const override {
    return ResourceTypeProvider::CONTENT_TYPE_UNKNOWN;
  }

  bool IsNonContentInitiatedRequest(
      const net::URLRequest& request) const override {
    return is_non_content_initiated_request_;
  }

  void set_is_non_content_initiated_request(
      bool is_non_content_initiated_request) {
    is_non_content_initiated_request_ = is_non_content_initiated_request;
  }

 private:
  bool is_non_content_initiated_request_ = false;
};

enum ProxyTestConfig { USE_SECURE_PROXY, USE_INSECURE_PROXY, BYPASS_PROXY };

class DataReductionProxyNetworkDelegateTest : public testing::Test {
 public:
  DataReductionProxyNetworkDelegateTest()
      : lofi_decider_(nullptr),
        lofi_ui_service_(nullptr),
        ssl_socket_data_provider_(net::ASYNC, net::OK) {
    ssl_socket_data_provider_.next_proto = net::kProtoHTTP11;
    ssl_socket_data_provider_.ssl_info.cert = net::ImportCertFromFile(
        net::GetTestCertsDirectory(), "unittest.selfsigned.der");
  }

  void Init(ProxyTestConfig proxy_config, bool enable_brotli_globally) {
    net::ProxyServer proxy_server;
    switch (proxy_config) {
      case BYPASS_PROXY:
        proxy_server = net::ProxyServer::Direct();
        break;
      case USE_SECURE_PROXY:
        proxy_server = net::ProxyServer::FromURI(
            "https://origin.net:443", net::ProxyServer::SCHEME_HTTPS);
        break;
      case USE_INSECURE_PROXY:
        proxy_server = net::ProxyServer::FromURI("http://origin.net:80",
                                                 net::ProxyServer::SCHEME_HTTP);
        break;
    }
    context_.reset(new net::TestURLRequestContext(true));
    context_storage_.reset(new net::URLRequestContextStorage(context_.get()));
    proxy_resolution_service_ =
        net::ProxyResolutionService::CreateFixedFromPacResult(
            proxy_server.ToPacString(), TRAFFIC_ANNOTATION_FOR_TESTS);
    context_->set_proxy_resolution_service(proxy_resolution_service_.get());

    mock_socket_factory_.reset(new net::MockClientSocketFactory());

    DataReductionProxyTestContext::Builder builder;
    builder.WithClient(kClient)
        .WithMockClientSocketFactory(mock_socket_factory_.get())
        .WithURLRequestContext(context_.get());

    if (proxy_config != BYPASS_PROXY) {
      builder.WithProxiesForHttp({DataReductionProxyServer(
          proxy_server, ProxyServer::UNSPECIFIED_TYPE)});
    }

    test_context_ = builder.Build();

    context_->set_client_socket_factory(mock_socket_factory_.get());
    test_context_->AttachToURLRequestContext(context_storage_.get());

    std::unique_ptr<TestLoFiDecider> lofi_decider(new TestLoFiDecider());
    lofi_decider_ = lofi_decider.get();
    test_context_->io_data()->set_lofi_decider(std::move(lofi_decider));

    std::unique_ptr<TestLoFiUIService> lofi_ui_service(new TestLoFiUIService());
    lofi_ui_service_ = lofi_ui_service.get();
    test_context_->io_data()->set_lofi_ui_service(std::move(lofi_ui_service));

    context_->set_enable_brotli(enable_brotli_globally);
    context_->Init();

    test_context_->DisableWarmupURLFetch();
    test_context_->EnableDataReductionProxyWithSecureProxyCheckSuccess();
  }

  // Build the sockets by adding appropriate mock data for
  // |effective_connection_types.size()| number of requests. Data for
  // chrome-Proxy-ect header is added to the mock data if |expect_ect_header|
  // is true. |reads_list|, |mock_writes| and |writes_list| should be empty, and
  // are owned by the caller.
  void BuildSocket(const std::string& response_headers,
                   const std::string& response_body,
                   bool expect_ect_header,
                   const std::vector<net::EffectiveConnectionType>&
                       effective_connection_types,
                   std::vector<net::MockRead>* reads_list,
                   std::vector<std::string>* mock_writes,
                   std::vector<net::MockWrite>* writes_list) {
    EXPECT_LT(0u, effective_connection_types.size());
    EXPECT_TRUE(reads_list->empty());
    EXPECT_TRUE(mock_writes->empty());
    EXPECT_TRUE(writes_list->empty());

    for (size_t i = 0; i < effective_connection_types.size(); ++i) {
      reads_list->push_back(net::MockRead(response_headers.c_str()));
      reads_list->push_back(net::MockRead(response_body.c_str()));
    }
    reads_list->push_back(net::MockRead(net::SYNCHRONOUS, net::OK));

    std::string prefix = std::string("GET ")
                             .append(kTestURL)
                             .append(" HTTP/1.1\r\n")
                             .append("Host: ")
                             .append(GURL(kTestURL).host())
                             .append(
                                 "\r\n"
                                 "Proxy-Connection: keep-alive\r\n"
                                 "User-Agent: \r\n"
                                 "Accept-Encoding: gzip, deflate\r\n"
                                 "Accept-Language: en-us,fr\r\n");

    if (io_data()->test_request_options()->GetHeaderValueForTesting().empty()) {
      // Force regeneration of Chrome-Proxy header.
      io_data()->test_request_options()->SetSecureSession("123");
    }

    EXPECT_FALSE(
        io_data()->test_request_options()->GetHeaderValueForTesting().empty());

    std::string suffix =
        std::string("Chrome-Proxy: ") +
        io_data()->test_request_options()->GetHeaderValueForTesting() +
        std::string("\r\n\r\n");

    mock_socket_factory_->AddSSLSocketDataProvider(&ssl_socket_data_provider_);

    for (net::EffectiveConnectionType effective_connection_type :
         effective_connection_types) {
      std::string ect_header;
      if (expect_ect_header) {
        ect_header = "chrome-proxy-ect: " +
                     std::string(net::GetNameForEffectiveConnectionType(
                         effective_connection_type)) +
                     "\r\n";
      }

      std::string mock_write = prefix + ect_header + suffix;
      mock_writes->push_back(mock_write);
      writes_list->push_back(net::MockWrite(mock_writes->back().c_str()));
    }

    EXPECT_FALSE(socket_);
    socket_ = std::make_unique<net::StaticSocketDataProvider>(*reads_list,
                                                              *writes_list);
    mock_socket_factory_->AddSocketDataProvider(socket_.get());
  }

  static void VerifyHeaders(bool expected_data_reduction_proxy_used,
                            bool expected_lofi_used,
                            const net::HttpRequestHeaders& headers) {
    EXPECT_EQ(expected_data_reduction_proxy_used,
              headers.HasHeader(chrome_proxy_header()));
    std::string header_value;
    headers.GetHeader(chrome_proxy_accept_transform_header(), &header_value);
    EXPECT_EQ(expected_data_reduction_proxy_used && expected_lofi_used,
              header_value.find("empty-image") != std::string::npos);
  }

  void VerifyDidNotifyLoFiResponse(bool lofi_response) const {
    EXPECT_EQ(lofi_response, lofi_ui_service_->DidNotifyLoFiResponse());
  }

  void ClearLoFiUIService() { lofi_ui_service_->ClearResponse(); }

  void VerifyDataReductionProxyData(const net::URLRequest& request,
                                    bool data_reduction_proxy_used,
                                    bool lofi_used) {
    DataReductionProxyData* data = DataReductionProxyData::GetData(request);
    if (!data_reduction_proxy_used) {
      EXPECT_FALSE(data);
    } else {
      EXPECT_TRUE(data->used_data_reduction_proxy());
    }
  }

  // Each line in |response_headers| should end with "\r\n" and not '\0', and
  // the last line should have a second "\r\n".
  // An empty |response_headers| is allowed. It works by making this look like
  // an HTTP/0.9 response, since HTTP/0.9 responses don't have headers.
  std::unique_ptr<net::URLRequest> FetchURLRequest(
      const GURL& url,
      net::HttpRequestHeaders* request_headers,
      const std::string& response_headers,
      int64_t response_content_length,
      int load_flags) {
    const std::string response_body(
        base::checked_cast<size_t>(response_content_length), ' ');

    net::MockRead reads[] = {net::MockRead(response_headers.c_str()),
                             net::MockRead(response_body.c_str()),
                             net::MockRead(net::SYNCHRONOUS, net::OK)};
    net::StaticSocketDataProvider socket(reads, base::span<net::MockWrite>());
    mock_socket_factory_->AddSocketDataProvider(&socket);

    net::TestDelegate delegate;
    std::unique_ptr<net::URLRequest> request = context_->CreateRequest(
        url, net::IDLE, &delegate, TRAFFIC_ANNOTATION_FOR_TESTS);
    if (request_headers)
      request->SetExtraRequestHeaders(*request_headers);
    request->SetLoadFlags(request->load_flags() | load_flags);

    request->Start();
    base::RunLoop().RunUntilIdle();
    return request;
  }

  // Reads brotli encoded content to |encoded_brotli_buffer_|.
  void ReadBrotliFile() {
    // Get the path of data directory.
    const size_t kDefaultBufferSize = 4096;
    base::FilePath data_dir;
    base::PathService::Get(base::DIR_SOURCE_ROOT, &data_dir);
    data_dir = data_dir.AppendASCII("net");
    data_dir = data_dir.AppendASCII("data");
    data_dir = data_dir.AppendASCII("filter_unittests");

    // Read data from the encoded file into buffer.
    base::FilePath encoded_file_path;
    encoded_file_path = data_dir.AppendASCII("google.br");
    ASSERT_TRUE(
        base::ReadFileToString(encoded_file_path, &encoded_brotli_buffer_));
    ASSERT_GE(kDefaultBufferSize, encoded_brotli_buffer_.size());
  }

  // Fetches a single URL request, verifies the correctness of Accept-Encoding
  // header, and verifies that the response is cached only if |expect_cached|
  // is set to true. Each line in |response_headers| should end with "\r\n" and
  // not '\0', and the last line should have a second "\r\n". An empty
  // |response_headers| is allowed. It works by making this look like an
  // HTTP/0.9 response, since HTTP/0.9 responses don't have headers.
  void FetchURLRequestAndVerifyBrotli(net::HttpRequestHeaders* request_headers,
                                      const std::string& response_headers,
                                      bool expect_cached,
                                      bool expect_brotli) {
    test_network_quality_tracker()->ReportEffectiveConnectionTypeForTesting(
        net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN);
    base::RunLoop().RunUntilIdle();
    GURL url(kTestURL);

    int response_body_size = 140;
    std::string response_body;
    if (expect_brotli && !expect_cached) {
      response_body = encoded_brotli_buffer_;
      response_body_size = response_body.size();
    } else {
      response_body =
          std::string(base::checked_cast<size_t>(response_body_size), ' ');
    }

    mock_socket_factory_->AddSSLSocketDataProvider(&ssl_socket_data_provider_);

    net::MockRead reads[] = {net::MockRead(response_headers.c_str()),
                             net::MockRead(response_body.c_str()),
                             net::MockRead(net::SYNCHRONOUS, net::OK)};

    if (io_data()->test_request_options()->GetHeaderValueForTesting().empty()) {
      // Force regeneration of Chrome-Proxy header.
      io_data()->test_request_options()->SetSecureSession("123");
    }
    EXPECT_FALSE(
        io_data()->test_request_options()->GetHeaderValueForTesting().empty());

    std::string host = GURL(kTestURL).host();
    std::string prefix_headers = std::string("GET ")
                                     .append(kTestURL)
                                     .append(
                                         " HTTP/1.1\r\n"
                                         "Host: ")
                                     .append(host)
                                     .append(
                                         "\r\n"
                                         "Proxy-Connection: keep-alive\r\n");
    std::string user_agent_header = "User-Agent: \r\n";

    // Set the base Accept-Encoding header value; Brotli may be added to it.
    std::string accept_encoding_header_value;
    bool accept_encoding_header_value_includes_brotli = false;
    bool has_accept_encoding_request_header =
        request_headers &&
        request_headers->HasHeader(net::HttpRequestHeaders::kAcceptEncoding);
    if (has_accept_encoding_request_header) {
      request_headers->GetHeader(net::HttpRequestHeaders::kAcceptEncoding,
                                 &accept_encoding_header_value);
      // Check for if the Accept-Encoding header value already includes Brotli.
      std::set<std::string> accept_encoding_header_entry_set;
      if (net::HttpUtil::ParseAcceptEncoding(
              accept_encoding_header_value,
              &accept_encoding_header_entry_set) &&
          accept_encoding_header_entry_set.find("br") !=
              accept_encoding_header_entry_set.end()) {
        accept_encoding_header_value_includes_brotli = true;
      }
    } else {
      accept_encoding_header_value = "gzip, deflate";
    }

    // Add Brotli to the Accept-Encoding header value if it is expected and not
    // already included. Brotli is expected if the request went to the network
    // (i.e., it was not a cached response), and it is a case where the data
    // reduction proxy network delegate adds Brotli to the header.
    if (expect_brotli && !expect_cached &&
        !accept_encoding_header_value_includes_brotli) {
      if (!accept_encoding_header_value.empty())
        accept_encoding_header_value += ", ";
      accept_encoding_header_value += "br";
      accept_encoding_header_value_includes_brotli = true;
    }

    std::string accept_encoding_header =
        "Accept-Encoding: " + accept_encoding_header_value + "\r\n";

    std::string accept_language_header("Accept-Language: en-us,fr\r\n");
    std::string ect_header = "chrome-proxy-ect: " +
                             std::string(net::GetNameForEffectiveConnectionType(
                                 net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN)) +
                             "\r\n";

    std::string suffix_headers =
        std::string("Chrome-Proxy: ") +
        io_data()->test_request_options()->GetHeaderValueForTesting() +
        std::string("\r\n\r\n");

    // If an Accept-Encoding header was provided, then Accept-Encoding appears
    // before User-Agent; otherwise, it appears after it.
    std::string mock_write;
    if (has_accept_encoding_request_header) {
      mock_write = prefix_headers + accept_encoding_header + user_agent_header +
                   accept_language_header + ect_header + suffix_headers;
    } else {
      mock_write = prefix_headers + user_agent_header + accept_encoding_header +
                   accept_language_header + ect_header + suffix_headers;
    }

    net::MockWrite writes[] = {net::MockWrite(mock_write.c_str())};
    net::StaticSocketDataProvider socket(reads, writes);
    mock_socket_factory_->AddSocketDataProvider(&socket);

    net::TestDelegate delegate;
    std::unique_ptr<net::URLRequest> request = context_->CreateRequest(
        url, net::IDLE, &delegate, TRAFFIC_ANNOTATION_FOR_TESTS);
    if (request_headers)
      request->SetExtraRequestHeaders(*request_headers);

    request->Start();
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(0, request->status().ToNetError());

    if (!expect_cached) {
      EXPECT_EQ(response_body_size,
                request->received_response_content_length());
      VerifyBrotliPresent(request.get(),
                          accept_encoding_header_value_includes_brotli,
                          expect_brotli);
    }
    EXPECT_EQ(expect_cached, request->GetTotalSentBytes() == 0);
    EXPECT_EQ(expect_cached, request->GetTotalReceivedBytes() == 0);
    EXPECT_EQ(expect_cached, request->was_cached());
  }

  void VerifyBrotliPresent(net::URLRequest* request,
                           bool expect_accept_encoding_brotli,
                           bool expect_content_encoding_brotli) {
    net::HttpRequestHeaders request_headers_sent;
    EXPECT_TRUE(request->GetFullRequestHeaders(&request_headers_sent));
    std::string accept_encoding_value;
    EXPECT_TRUE(request_headers_sent.GetHeader("Accept-Encoding",
                                               &accept_encoding_value));

    std::set<std::string> accept_encoding_value_set;
    net::HttpUtil::ParseAcceptEncoding(accept_encoding_value,
                                       &accept_encoding_value_set);
    EXPECT_EQ(expect_accept_encoding_brotli,
              accept_encoding_value_set.find("br") !=
                  accept_encoding_value_set.end());

    std::string content_encoding_value;
    request->GetResponseHeaderByName("Content-Encoding",
                                     &content_encoding_value);
    EXPECT_EQ(expect_content_encoding_brotli, content_encoding_value == "br");
  }

  void FetchURLRequestAndVerifyPageIdDirective(base::Optional<uint64_t> page_id,
                                               bool redirect_once) {
    std::string response_headers =
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: 140\r\n"
        "Via: 1.1 Chrome-Compression-Proxy\r\n"
        "Chrome-Proxy: ofcl=200\r\n"
        "Cache-Control: max-age=1200\r\n"
        "Vary: accept-encoding\r\n\r\n";

    GURL url(kTestURL);

    int response_body_size = 140;
    std::string response_body =
        std::string(base::checked_cast<size_t>(response_body_size), ' ');

    mock_socket_factory_->AddSSLSocketDataProvider(&ssl_socket_data_provider_);

    net::MockRead redirect_reads[] = {
        net::MockRead("HTTP/1.1 302 Redirect\r\n"),
        net::MockRead("Location: http://www.google.com/\r\n"),
        net::MockRead("Content-Length: 0\r\n\r\n"),
        net::MockRead(net::SYNCHRONOUS, net::OK),
        net::MockRead(response_headers.c_str()),
        net::MockRead(response_body.c_str()),
        net::MockRead(net::SYNCHRONOUS, net::OK)};

    net::MockRead reads[] = {net::MockRead(response_headers.c_str()),
                             net::MockRead(response_body.c_str()),
                             net::MockRead(net::SYNCHRONOUS, net::OK)};

    EXPECT_FALSE(
        io_data()->test_request_options()->GetHeaderValueForTesting().empty());

    std::string page_id_value;
    if (page_id) {
      char page_id_buffer[17];
      if (base::strings::SafeSPrintf(page_id_buffer, "%x", page_id.value()) >
          0) {
        page_id_value = std::string("pid=") + page_id_buffer;
      }
    }

    std::string mock_write =
        "GET http://www.google.com/ HTTP/1.1\r\nHost: "
        "www.google.com\r\nProxy-Connection: "
        "keep-alive\r\nUser-Agent: \r\nAccept-Encoding: gzip, "
        "deflate\r\nAccept-Language: en-us,fr\r\n"
        "chrome-proxy-ect: Unknown\r\n"
        "Chrome-Proxy: " +
        io_data()->test_request_options()->GetHeaderValueForTesting() +
        (page_id_value.empty() ? "" : (", " + page_id_value)) + "\r\n\r\n";

    net::MockWrite redirect_writes[] = {net::MockWrite(mock_write.c_str()),
                                        net::MockWrite(mock_write.c_str())};

    net::MockWrite writes[] = {net::MockWrite(mock_write.c_str())};

    std::unique_ptr<net::StaticSocketDataProvider> socket;
    if (!redirect_once) {
      socket = std::make_unique<net::StaticSocketDataProvider>(reads, writes);
    } else {
      socket = std::make_unique<net::StaticSocketDataProvider>(redirect_reads,
                                                               redirect_writes);
    }

    mock_socket_factory_->AddSocketDataProvider(socket.get());

    net::TestDelegate delegate;
    std::unique_ptr<net::URLRequest> request = context_->CreateRequest(
        url, net::IDLE, &delegate, TRAFFIC_ANNOTATION_FOR_TESTS);
    if (!page_id_value.empty()) {
      request->SetLoadFlags(request->load_flags() |
                            net::LOAD_MAIN_FRAME_DEPRECATED);
    }

    request->Start();
    base::RunLoop().RunUntilIdle();
  }

  // Fetches a request while the effective connection type is set to
  // |effective_connection_type|. Verifies that the request headers include the
  // chrome-proxy-ect header only if |expect_ect_header| is true. The response
  // must be served from the cache if |expect_cached| is true.
  void FetchURLRequestAndVerifyECTHeader(
      net::EffectiveConnectionType effective_connection_type,
      bool expect_ect_header,
      bool expect_cached) {
    test_network_quality_tracker()->ReportEffectiveConnectionTypeForTesting(
        effective_connection_type);
    base::RunLoop().RunUntilIdle();

    net::TestDelegate delegate;
    std::unique_ptr<net::URLRequest> request = context_->CreateRequest(
        GURL(kTestURL), net::IDLE, &delegate, TRAFFIC_ANNOTATION_FOR_TESTS);

    request->Start();
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(140, request->received_response_content_length());
    EXPECT_EQ(expect_cached, request->was_cached());
    EXPECT_EQ(expect_cached, request->GetTotalSentBytes() == 0);
    EXPECT_EQ(expect_cached, request->GetTotalReceivedBytes() == 0);

    net::HttpRequestHeaders sent_request_headers;
    EXPECT_NE(expect_cached,
              request->GetFullRequestHeaders(&sent_request_headers));

    if (expect_cached) {
      // Request headers are missing. Return since there is nothing left to
      // check.
      return;
    }

    // Verify that chrome-proxy-ect header is present in the request headers
    // only if |expect_ect_header| is true.
    std::string ect_value;
    EXPECT_EQ(expect_ect_header, sent_request_headers.GetHeader(
                                     chrome_proxy_ect_header(), &ect_value));

    if (!expect_ect_header)
      return;
    EXPECT_EQ(net::GetNameForEffectiveConnectionType(effective_connection_type),
              ect_value);
  }

  void DelegateStageDone(int result) {}

  void NotifyNetworkDelegate(net::URLRequest* request,
                             const net::ProxyInfo& data_reduction_proxy_info,
                             const net::ProxyRetryInfoMap& proxy_retry_info,
                             net::HttpRequestHeaders* headers) {
    network_delegate()->NotifyBeforeURLRequest(
        request,
        base::BindOnce(
            &DataReductionProxyNetworkDelegateTest::DelegateStageDone,
            base::Unretained(this)),
        nullptr);
    network_delegate()->NotifyBeforeStartTransaction(
        request,
        base::BindOnce(
            &DataReductionProxyNetworkDelegateTest::DelegateStageDone,
            base::Unretained(this)),
        headers);
    network_delegate()->NotifyBeforeSendHeaders(
        request, data_reduction_proxy_info, proxy_retry_info, headers);
  }

  void EnableDataUsageReporting() {
    test_context_->pref_service()->SetBoolean(prefs::kDataUsageReportingEnabled,
                                              true);
    // Give the setting notification a chance to propagate.
    base::RunLoop().RunUntilIdle();
  }

  size_t GetOtherHostDataUsage() {
    const auto& data_usage_map = test_context_->data_reduction_proxy_service()
                                     ->compression_stats()
                                     ->DataUsageMapForTesting();
    const auto& it = data_usage_map.find(util::GetSiteBreakdownOtherHostName());
    if (it != data_usage_map.end())
      return it->second->data_used();
    return 0;
  }

  net::MockClientSocketFactory* mock_socket_factory() {
    return mock_socket_factory_.get();
  }

  net::TestURLRequestContext* context() { return context_.get(); }

  net::NetworkDelegate* network_delegate() const {
    return context_->network_delegate();
  }

  TestDataReductionProxyParams* params() const {
    return test_context_->config()->test_params();
  }

  TestDataReductionProxyConfig* config() const {
    return test_context_->config();
  }

  TestDataReductionProxyIOData* io_data() const {
    return test_context_->io_data();
  }

  TestLoFiDecider* lofi_decider() const { return lofi_decider_; }

  network::TestNetworkQualityTracker* test_network_quality_tracker() {
    return test_context_->test_network_quality_tracker();
  }

  net::SSLSocketDataProvider* ssl_socket_data_provider() {
    return &ssl_socket_data_provider_;
  }

  void DisableWarmupURLFetchCallback() {
    test_context_->DisableWarmupURLFetchCallback();
  }

 private:
  base::MessageLoopForIO message_loop_;
  std::unique_ptr<net::MockClientSocketFactory> mock_socket_factory_;
  std::unique_ptr<net::ProxyResolutionService> proxy_resolution_service_;
  std::unique_ptr<net::TestURLRequestContext> context_;
  std::unique_ptr<net::URLRequestContextStorage> context_storage_;

  TestLoFiDecider* lofi_decider_;
  TestLoFiUIService* lofi_ui_service_;
  std::unique_ptr<DataReductionProxyTestContext> test_context_;

  net::SSLSocketDataProvider ssl_socket_data_provider_;

  std::unique_ptr<net::StaticSocketDataProvider> socket_;

  // Encoded Brotli content read from a file. May be empty.
  std::string encoded_brotli_buffer_;
};

TEST_F(DataReductionProxyNetworkDelegateTest, AuthenticationTest) {
  Init(USE_INSECURE_PROXY, false);
  std::unique_ptr<net::URLRequest> fake_request(
      FetchURLRequest(GURL(kTestURL), nullptr, std::string(), 0, 0));

  net::ProxyInfo data_reduction_proxy_info;
  net::ProxyRetryInfoMap proxy_retry_info;
  std::string data_reduction_proxy;
  data_reduction_proxy_info.UseProxyServer(
      params()->proxies_for_http().front().proxy_server());

  net::HttpRequestHeaders headers;
  // Call network delegate methods to ensure that appropriate chrome proxy
  // headers get added/removed.
  network_delegate()->NotifyBeforeStartTransaction(
      fake_request.get(),
      base::BindOnce(&DataReductionProxyNetworkDelegateTest::DelegateStageDone,
                     base::Unretained(this)),
      &headers);
  network_delegate()->NotifyBeforeSendHeaders(fake_request.get(),
                                              data_reduction_proxy_info,
                                              proxy_retry_info, &headers);

  EXPECT_TRUE(headers.HasHeader(chrome_proxy_header()));
  std::string header_value;
  headers.GetHeader(chrome_proxy_header(), &header_value);
  EXPECT_TRUE(header_value.find("ps=") != std::string::npos);
  EXPECT_TRUE(header_value.find("sid=") != std::string::npos);
}

TEST_F(DataReductionProxyNetworkDelegateTest, LoFiTransitions) {
  Init(USE_INSECURE_PROXY, false);

  // Enable Lo-Fi.
  bool is_data_reduction_proxy_enabled[] = {false, true};

  for (size_t i = 0; i < arraysize(is_data_reduction_proxy_enabled); ++i) {
    net::ProxyInfo data_reduction_proxy_info;
    std::string proxy;
    if (is_data_reduction_proxy_enabled[i]) {
      data_reduction_proxy_info.UseProxyServer(
          params()->proxies_for_http().front().proxy_server());
    } else {
      base::TrimString(kOtherProxy, "/", &proxy);
      data_reduction_proxy_info.UseNamedProxy(proxy);
    }

    // Needed as a parameter, but functionality is not tested.
    previews::TestPreviewsDecider test_previews_decider(true);

    {
      // Main frame loaded. Lo-Fi should be used.
      net::HttpRequestHeaders headers;
      net::ProxyRetryInfoMap proxy_retry_info;

      net::TestDelegate delegate;
      std::unique_ptr<net::URLRequest> fake_request = context()->CreateRequest(
          GURL(kTestURL), net::IDLE, &delegate, TRAFFIC_ANNOTATION_FOR_TESTS);
      fake_request->SetLoadFlags(net::LOAD_MAIN_FRAME_DEPRECATED);
      lofi_decider()->SetIsUsingLoFi(true);
      NotifyNetworkDelegate(fake_request.get(), data_reduction_proxy_info,
                            proxy_retry_info, &headers);

      VerifyHeaders(is_data_reduction_proxy_enabled[i], true, headers);
      VerifyDataReductionProxyData(*fake_request,
                                   is_data_reduction_proxy_enabled[i], true);
    }

    {
      // Lo-Fi is already off. Lo-Fi should not be used.
      net::HttpRequestHeaders headers;
      net::ProxyRetryInfoMap proxy_retry_info;
      net::TestDelegate delegate;
      std::unique_ptr<net::URLRequest> fake_request = context()->CreateRequest(
          GURL(kTestURL), net::IDLE, &delegate, TRAFFIC_ANNOTATION_FOR_TESTS);
      lofi_decider()->SetIsUsingLoFi(false);
      NotifyNetworkDelegate(fake_request.get(), data_reduction_proxy_info,
                            proxy_retry_info, &headers);
      VerifyHeaders(is_data_reduction_proxy_enabled[i], false, headers);
      VerifyDataReductionProxyData(*fake_request,
                                   is_data_reduction_proxy_enabled[i], false);
    }

    {
      // Lo-Fi is already on. Lo-Fi should be used.
      net::HttpRequestHeaders headers;
      net::ProxyRetryInfoMap proxy_retry_info;
      net::TestDelegate delegate;
      std::unique_ptr<net::URLRequest> fake_request = context()->CreateRequest(
          GURL(kTestURL), net::IDLE, &delegate, TRAFFIC_ANNOTATION_FOR_TESTS);

      lofi_decider()->SetIsUsingLoFi(true);
      NotifyNetworkDelegate(fake_request.get(), data_reduction_proxy_info,
                            proxy_retry_info, &headers);
      VerifyHeaders(is_data_reduction_proxy_enabled[i], true, headers);
      VerifyDataReductionProxyData(*fake_request,
                                   is_data_reduction_proxy_enabled[i], true);
    }

    {
      // Main frame request with Lo-Fi off. Lo-Fi should not be used.
      // State of Lo-Fi should persist until next page load.
      net::HttpRequestHeaders headers;
      net::ProxyRetryInfoMap proxy_retry_info;
      net::TestDelegate delegate;
      std::unique_ptr<net::URLRequest> fake_request = context()->CreateRequest(
          GURL(kTestURL), net::IDLE, &delegate, TRAFFIC_ANNOTATION_FOR_TESTS);
      fake_request->SetLoadFlags(net::LOAD_MAIN_FRAME_DEPRECATED);
      lofi_decider()->SetIsUsingLoFi(false);
      NotifyNetworkDelegate(fake_request.get(), data_reduction_proxy_info,
                            proxy_retry_info, &headers);
      VerifyHeaders(is_data_reduction_proxy_enabled[i], false, headers);
      VerifyDataReductionProxyData(*fake_request,
                                   is_data_reduction_proxy_enabled[i], false);
    }

    {
      // Lo-Fi is off. Lo-Fi is still not used.
      net::HttpRequestHeaders headers;
      net::ProxyRetryInfoMap proxy_retry_info;
      net::TestDelegate delegate;
      std::unique_ptr<net::URLRequest> fake_request = context()->CreateRequest(
          GURL(kTestURL), net::IDLE, &delegate, TRAFFIC_ANNOTATION_FOR_TESTS);
      lofi_decider()->SetIsUsingLoFi(false);
      NotifyNetworkDelegate(fake_request.get(), data_reduction_proxy_info,
                            proxy_retry_info, &headers);
      VerifyHeaders(is_data_reduction_proxy_enabled[i], false, headers);
      VerifyDataReductionProxyData(*fake_request,
                                   is_data_reduction_proxy_enabled[i], false);
    }

    {
      // Main frame request. Lo-Fi should be used.
      net::HttpRequestHeaders headers;
      net::ProxyRetryInfoMap proxy_retry_info;
      net::TestDelegate delegate;
      std::unique_ptr<net::URLRequest> fake_request = context()->CreateRequest(
          GURL(kTestURL), net::IDLE, &delegate, TRAFFIC_ANNOTATION_FOR_TESTS);
      fake_request->SetLoadFlags(net::LOAD_MAIN_FRAME_DEPRECATED);
      lofi_decider()->SetIsUsingLoFi(true);
      NotifyNetworkDelegate(fake_request.get(), data_reduction_proxy_info,
                            proxy_retry_info, &headers);
      VerifyDataReductionProxyData(*fake_request,
                                   is_data_reduction_proxy_enabled[i], true);
    }
  }
}

TEST_F(DataReductionProxyNetworkDelegateTest, RequestDataConfigurations) {
  Init(USE_INSECURE_PROXY, false);
  const struct {
    bool lofi_on;
    bool used_data_reduction_proxy;
    bool main_frame;
  } tests[] = {
      // Lo-Fi off. Main Frame Request.
      {false, true, true},
      // Data reduction proxy not used. Main Frame Request.
      {false, false, true},
      // Data reduction proxy not used, Lo-Fi should not be used. Main Frame
      // Request.
      {true, false, true},
      // Lo-Fi on. Main Frame Request.
      {true, true, true},
      // Lo-Fi off. Not a Main Frame Request.
      {false, true, false},
      // Data reduction proxy not used. Not a Main Frame Request.
      {false, false, false},
      // Data reduction proxy not used, Lo-Fi should not be used. Not a Main
      // Frame Request.
      {true, false, false},
      // Lo-Fi on. Not a Main Frame Request.
      {true, true, false},
  };

  for (const auto& test : tests) {
    net::ProxyInfo data_reduction_proxy_info;
    if (test.used_data_reduction_proxy) {
      data_reduction_proxy_info.UseProxyServer(
          params()->proxies_for_http().front().proxy_server());
    } else {
      data_reduction_proxy_info.UseNamedProxy("port.of.other.proxy");
    }
    // Main frame loaded. Lo-Fi should be used.
    net::HttpRequestHeaders headers;
    net::ProxyRetryInfoMap proxy_retry_info;

    test_network_quality_tracker()->ReportEffectiveConnectionTypeForTesting(
        net::EFFECTIVE_CONNECTION_TYPE_OFFLINE);
    base::RunLoop().RunUntilIdle();

    std::unique_ptr<net::URLRequest> request =
        context()->CreateRequest(GURL(kTestURL), net::RequestPriority::IDLE,
                                 nullptr, TRAFFIC_ANNOTATION_FOR_TESTS);
    request->SetLoadFlags(test.main_frame ? net::LOAD_MAIN_FRAME_DEPRECATED
                                          : 0);
    lofi_decider()->SetIsUsingLoFi(test.lofi_on);
    io_data()->request_options()->SetSecureSession("fake-session");

    // Call network delegate methods to ensure that appropriate chrome proxy
    // headers get added/removed.
    network_delegate()->NotifyBeforeStartTransaction(
        request.get(),
        base::BindOnce(
            &DataReductionProxyNetworkDelegateTest::DelegateStageDone,
            base::Unretained(this)),
        &headers);
    network_delegate()->NotifyBeforeSendHeaders(
        request.get(), data_reduction_proxy_info, proxy_retry_info, &headers);
    DataReductionProxyData* data = DataReductionProxyData::GetData(*request);
    if (!test.used_data_reduction_proxy) {
      EXPECT_FALSE(data);
    } else {
      EXPECT_TRUE(data);
      EXPECT_EQ(test.main_frame ? net::EFFECTIVE_CONNECTION_TYPE_OFFLINE
                                : net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN,
                data->effective_connection_type());
      EXPECT_TRUE(data->used_data_reduction_proxy());
      EXPECT_EQ(test.main_frame ? GURL(kTestURL) : GURL(), data->request_url());
      EXPECT_EQ(test.main_frame ? "fake-session" : "", data->session_key());
    }
  }
}

TEST_F(DataReductionProxyNetworkDelegateTest,
       RequestDataHoldbackConfigurations) {
  Init(USE_INSECURE_PROXY, false);
  const struct {
    bool data_reduction_proxy_enabled;
    bool used_direct;
  } tests[] = {
      {
          false, true,
      },
      {
          false, false,
      },
      {
          true, false,
      },
      {
          true, true,
      },
  };
  test_network_quality_tracker()->ReportEffectiveConnectionTypeForTesting(
      net::EFFECTIVE_CONNECTION_TYPE_4G);
  base::RunLoop().RunUntilIdle();
  base::FieldTrialList field_trial_list(nullptr);
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "DataCompressionProxyHoldback", "Enabled"));
  for (const auto& test : tests) {
    net::ProxyInfo data_reduction_proxy_info;
    if (test.used_direct)
      data_reduction_proxy_info.UseDirect();
    else
      data_reduction_proxy_info.UseNamedProxy("some.other.proxy");
    config()->UpdateConfigForTesting(test.data_reduction_proxy_enabled, true,
                                     true);
    std::unique_ptr<net::URLRequest> request =
        context()->CreateRequest(GURL(kTestURL), net::RequestPriority::IDLE,
                                 nullptr, TRAFFIC_ANNOTATION_FOR_TESTS);
    request->SetLoadFlags(net::LOAD_MAIN_FRAME_DEPRECATED);
    request->set_method("GET");
    io_data()->request_options()->SetSecureSession("fake-session");
    net::HttpRequestHeaders headers;
    net::ProxyRetryInfoMap proxy_retry_info;
    network_delegate()->NotifyBeforeSendHeaders(
        request.get(), data_reduction_proxy_info, proxy_retry_info, &headers);
    DataReductionProxyData* data = DataReductionProxyData::GetData(*request);
    if (!test.data_reduction_proxy_enabled || !test.used_direct) {
      EXPECT_FALSE(data);
    } else {
      EXPECT_TRUE(data);
      EXPECT_TRUE(data->used_data_reduction_proxy());
      EXPECT_EQ("fake-session", data->session_key());
      EXPECT_EQ(GURL(kTestURL), data->request_url());
      EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_4G,
                data->effective_connection_type());
      EXPECT_TRUE(data->page_id());
    }
  }
}

TEST_F(DataReductionProxyNetworkDelegateTest, RedirectRequestDataCleared) {
  Init(USE_INSECURE_PROXY, false);
  net::ProxyInfo data_reduction_proxy_info;
  data_reduction_proxy_info.UseProxyServer(
      params()->proxies_for_http().front().proxy_server());

  // Main frame loaded. Lo-Fi should be used.
  net::HttpRequestHeaders headers_original;
  net::ProxyRetryInfoMap proxy_retry_info;

  test_network_quality_tracker()->ReportEffectiveConnectionTypeForTesting(
      net::EFFECTIVE_CONNECTION_TYPE_OFFLINE);
  base::RunLoop().RunUntilIdle();

  std::unique_ptr<net::URLRequest> request =
      context()->CreateRequest(GURL(kTestURL), net::RequestPriority::IDLE,
                               nullptr, TRAFFIC_ANNOTATION_FOR_TESTS);
  request->SetLoadFlags(net::LOAD_MAIN_FRAME_DEPRECATED);
  lofi_decider()->SetIsUsingLoFi(true);
  io_data()->request_options()->SetSecureSession("fake-session");

  // Call network delegate methods to ensure that appropriate chrome proxy
  // headers get added/removed.
  network_delegate()->NotifyBeforeStartTransaction(
      request.get(),
      base::BindOnce(&DataReductionProxyNetworkDelegateTest::DelegateStageDone,
                     base::Unretained(this)),
      &headers_original);
  network_delegate()->NotifyBeforeSendHeaders(
      request.get(), data_reduction_proxy_info, proxy_retry_info,
      &headers_original);
  DataReductionProxyData* data = DataReductionProxyData::GetData(*request);
  uint64_t original_page_id = data->page_id().value();
  // Artificially add a RequestInfo for sake of testing that it is persisted.
  data->add_request_info(DataReductionProxyData::RequestInfo(
      DataReductionProxyData::RequestInfo::Protocol::HTTP, false,
      base::TimeDelta(), base::TimeDelta(), base::TimeDelta()));

  EXPECT_TRUE(data);
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_OFFLINE,
            data->effective_connection_type());
  EXPECT_TRUE(data->used_data_reduction_proxy());
  EXPECT_EQ(GURL(kTestURL), data->request_url());
  EXPECT_EQ("fake-session", data->session_key());

  data_reduction_proxy_info.UseNamedProxy("port.of.other.proxy");

  // Simulate a redirect even though the same URL is used. Should clear
  // DataReductionProxyData.
  network_delegate()->NotifyBeforeRedirect(request.get(), GURL(kTestURL));
  data = DataReductionProxyData::GetData(*request);
  EXPECT_FALSE(data && data->used_data_reduction_proxy());
  // page_id and request_info should be persisted across redirects.
  EXPECT_EQ(original_page_id, data->page_id().value());
  EXPECT_EQ(1u, data->request_info().size());

  // Call NotifyBeforeSendHeaders again with different proxy info to check that
  // new data isn't added. Use a new set of headers since the redirected HTTP
  // jobs do not reuse headers from the previous jobs. Also, call network
  // delegate methods to ensure that appropriate chrome proxy headers get
  // added/removed.
  net::HttpRequestHeaders headers_redirect;
  network_delegate()->NotifyBeforeStartTransaction(
      request.get(),
      base::BindOnce(&DataReductionProxyNetworkDelegateTest::DelegateStageDone,
                     base::Unretained(this)),
      &headers_redirect);
  network_delegate()->NotifyBeforeSendHeaders(
      request.get(), data_reduction_proxy_info, proxy_retry_info,
      &headers_redirect);
  data = DataReductionProxyData::GetData(*request);
  EXPECT_FALSE(data);
}

TEST_F(DataReductionProxyNetworkDelegateTest, NetHistograms) {
  Init(USE_INSECURE_PROXY, false);
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {previews::features::kPreviews,
       features::kDataReductionProxyDecidesTransform},
      {});

  base::HistogramTester histogram_tester;

  std::string response_headers =
      "HTTP/1.1 200 OK\r\n"
      "Date: Wed, 28 Nov 2007 09:40:09 GMT\r\n"
      "Expires: Mon, 24 Nov 2014 12:45:26 GMT\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n"
      "Chrome-Proxy: ofcl=" +
      base::Int64ToString(kOriginalContentLength) + "\r\n\r\n";

  std::unique_ptr<net::URLRequest> fake_request(FetchURLRequest(
      GURL(kTestURL), nullptr, response_headers, kResponseContentLength, 0));
  fake_request->SetLoadFlags(fake_request->load_flags() |
                             net::LOAD_MAIN_FRAME_DEPRECATED);

  base::TimeDelta freshness_lifetime =
      fake_request->response_info().headers->GetFreshnessLifetimes(
          fake_request->response_info().response_time).freshness;

  histogram_tester.ExpectUniqueSample(kOriginalValidOCLHistogramName,
                                      kOriginalContentLength, 1);
  histogram_tester.ExpectUniqueSample(kOriginalInsecureViaDRPHistogramName,
                                      kOriginalContentLength, 1);
  histogram_tester.ExpectUniqueSample(
      kDifferenceValidOCLHistogramName,
      kOriginalContentLength - kResponseContentLength, 1);
  histogram_tester.ExpectUniqueSample(kReceivedHistogramName,
                                      kResponseContentLength, 1);
  histogram_tester.ExpectUniqueSample(kReceivedInsecureViaDRPHistogramName,
                                      kResponseContentLength, 1);
  histogram_tester.ExpectTotalCount(kReceivedInsecureDirectHistogramName, 0);
  histogram_tester.ExpectTotalCount(kReceivedSecureDirectHistogramName, 0);
  histogram_tester.ExpectTotalCount(kReceivedVideoInsecureViaDRPHistogramName,
                                    0);
  histogram_tester.ExpectUniqueSample(kReceivedInsecureViaDRPHistogramName,
                                      kResponseContentLength, 1);
  histogram_tester.ExpectUniqueSample(kFreshnessLifetimeHistogramName,
                                      freshness_lifetime.InSeconds(), 1);
}

TEST_F(DataReductionProxyNetworkDelegateTest, NetVideoHistograms) {
  Init(USE_INSECURE_PROXY, false);

  base::HistogramTester histogram_tester;

  // Check video
  std::string video_response_headers =
      "HTTP/1.1 200 OK\r\n"
      "Date: Wed, 28 Nov 2007 09:40:09 GMT\r\n"
      "Expires: Mon, 24 Nov 2014 12:45:26 GMT\r\n"
      "Content-Type: video/mp4\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n"
      "Chrome-Proxy: ofcl=" +
      base::Int64ToString(kOriginalContentLength) + "\r\n\r\n";

  FetchURLRequest(GURL(kTestURL), nullptr, video_response_headers,
                  kResponseContentLength, 0);

  histogram_tester.ExpectUniqueSample(kReceivedInsecureViaDRPHistogramName,
                                      kResponseContentLength, 1);
  histogram_tester.ExpectTotalCount(kReceivedInsecureDirectHistogramName, 0);
  histogram_tester.ExpectTotalCount(kReceivedInsecureBypassedHistogramName, 0);
  histogram_tester.ExpectTotalCount(kReceivedInsecureOtherHistogramName, 0);
  histogram_tester.ExpectTotalCount(kReceivedSecureViaDRPHistogramName, 0);
  histogram_tester.ExpectTotalCount(kReceivedSecureDirectHistogramName, 0);
  histogram_tester.ExpectTotalCount(kReceivedSecureBypassedHistogramName, 0);
  histogram_tester.ExpectTotalCount(kReceivedSecureOtherHistogramName, 0);
  histogram_tester.ExpectUniqueSample(kReceivedVideoInsecureViaDRPHistogramName,
                                      kResponseContentLength, 1);
  histogram_tester.ExpectTotalCount(kReceivedVideoInsecureDirectHistogramName,
                                    0);
  histogram_tester.ExpectTotalCount(kReceivedVideoInsecureBypassedHistogramName,
                                    0);
  histogram_tester.ExpectTotalCount(kReceivedVideoInsecureOtherHistogramName,
                                    0);
  histogram_tester.ExpectTotalCount(kReceivedVideoSecureViaDRPHistogramName, 0);
  histogram_tester.ExpectTotalCount(kReceivedVideoSecureDirectHistogramName, 0);
  histogram_tester.ExpectTotalCount(kReceivedVideoSecureBypassedHistogramName,
                                    0);
  histogram_tester.ExpectTotalCount(kReceivedVideoSecureOtherHistogramName, 0);
}

struct ExpectedHistogram {
  const std::string name;
  int64_t value;
};

bool operator<(const ExpectedHistogram& hist1, const ExpectedHistogram& hist2) {
  return hist1.name < hist2.name;
}

TEST_F(DataReductionProxyNetworkDelegateTest, DetailedNetHistograms) {
  // List of all the histograms we're concerned with.
  // Each test case can define interesting histograms from this list.
  // Any histogram not called out in an invidual test will have an expected
  // count of 0 samples.
  const std::set<std::string> all_new_net_histograms{
      // HTTP received content length
      kReceivedInsecureDirectHistogramName,
      kReceivedInsecureViaDRPHistogramName,
      kReceivedInsecureBypassedHistogramName,
      kReceivedInsecureOtherHistogramName,
      // HTTP video received content length
      kReceivedVideoInsecureDirectHistogramName,
      kReceivedVideoInsecureViaDRPHistogramName,
      kReceivedVideoInsecureBypassedHistogramName,
      kReceivedVideoInsecureOtherHistogramName,
      // HTTPS received content length
      kReceivedSecureDirectHistogramName, kReceivedSecureViaDRPHistogramName,
      kReceivedSecureBypassedHistogramName, kReceivedSecureOtherHistogramName,
      // HTTPS video received content length
      kReceivedVideoSecureDirectHistogramName,
      kReceivedVideoSecureViaDRPHistogramName,
      kReceivedVideoSecureBypassedHistogramName,
      kReceivedVideoSecureOtherHistogramName,
      // HTTP Original content length
      kOriginalInsecureDirectHistogramName,
      kOriginalInsecureViaDRPHistogramName,
      kOriginalInsecureBypassedHistogramName,
      kOriginalInsecureOtherHistogramName,
      // HTTP video Original content length
      kOriginalVideoInsecureDirectHistogramName,
      kOriginalVideoInsecureViaDRPHistogramName,
      kOriginalVideoInsecureBypassedHistogramName,
      kOriginalVideoInsecureOtherHistogramName,
      // HTTPS Original content length
      kOriginalSecureDirectHistogramName, kOriginalSecureViaDRPHistogramName,
      kOriginalSecureBypassedHistogramName, kOriginalSecureOtherHistogramName,
      // HTTPS video Original content length
      kOriginalVideoSecureDirectHistogramName,
      kOriginalVideoSecureViaDRPHistogramName,
      kOriginalVideoSecureBypassedHistogramName,
      kOriginalVideoSecureOtherHistogramName,
  };

  const struct {
    const std::string name;
    bool is_video;
    bool is_https;
    ProxyTestConfig proxy_config;
    int64_t original_content_length;
    int64_t content_length;
    // Any histogram listed in all_new_net_histograms but not here should have
    // no samples.
    const std::set<ExpectedHistogram> expected_histograms;
  } tests[] = {
      {"HTTP nonvideo request via DRP",
       false,
       false,
       USE_INSECURE_PROXY,
       kOriginalContentLength,
       kResponseContentLength,
       {
           {kReceivedInsecureViaDRPHistogramName, kResponseContentLength},
           {kOriginalInsecureViaDRPHistogramName, kOriginalContentLength},
       }},
      {"HTTP video request via DRP",
       true,
       false,
       USE_INSECURE_PROXY,
       kOriginalContentLength,
       kResponseContentLength,
       {
           {kReceivedInsecureViaDRPHistogramName, kResponseContentLength},
           {kOriginalInsecureViaDRPHistogramName, kOriginalContentLength},
           {kReceivedVideoInsecureViaDRPHistogramName, kResponseContentLength},
           {kOriginalVideoInsecureViaDRPHistogramName, kOriginalContentLength},
       }},
      {"DRP not configured for http",
       false,
       false,
       BYPASS_PROXY,
       kOriginalContentLength,
       kResponseContentLength,
       {
           {kReceivedInsecureDirectHistogramName, kResponseContentLength},
           {kOriginalInsecureDirectHistogramName, kResponseContentLength},
       }},
      {"DRP not configured for http video",
       true,
       false,
       BYPASS_PROXY,
       kOriginalContentLength,
       kResponseContentLength,
       {
           {kReceivedInsecureDirectHistogramName, kResponseContentLength},
           {kOriginalInsecureDirectHistogramName, kResponseContentLength},
           {kReceivedVideoInsecureDirectHistogramName, kResponseContentLength},
           {kOriginalVideoInsecureDirectHistogramName, kResponseContentLength},
       }},
      {"nonvideo over https",
       false,
       true,
       BYPASS_PROXY,
       kOriginalContentLength,
       kResponseContentLength,
       {
           {kReceivedSecureDirectHistogramName, kResponseContentLength},
           {kOriginalSecureDirectHistogramName, kResponseContentLength},
       }},
      {"video over https",
       true,
       true,
       BYPASS_PROXY,
       kOriginalContentLength,
       kResponseContentLength,
       {
           {kReceivedSecureDirectHistogramName, kResponseContentLength},
           {kOriginalSecureDirectHistogramName, kResponseContentLength},
           {kReceivedVideoSecureDirectHistogramName, kResponseContentLength},
           {kOriginalVideoSecureDirectHistogramName, kResponseContentLength},
       }},
  };

  for (const auto& test : tests) {
    LOG(INFO) << "NetHistograms: " << test.name;
    Init(test.proxy_config, false);
    base::HistogramTester histogram_tester;

    GURL test_url = GURL(kTestURL);

    if (test.is_https) {
      test_url = GURL(kSecureTestURL);
      mock_socket_factory()->AddSSLSocketDataProvider(
          ssl_socket_data_provider());
    }

    std::string via_header;
    std::string ocl_header;

    if (test.proxy_config == USE_INSECURE_PROXY) {
      via_header = "Via: 1.1 Chrome-Compression-Proxy\r\n";
      ocl_header = "Chrome-Proxy: ofcl=" +
                   base::Int64ToString(kOriginalContentLength) + "\r\n";
    }
    if (test.is_video) {
      // Check video
      std::string video_response_headers =
          "HTTP/1.1 200 OK\r\n"
          "Date: Wed, 28 Nov 2007 09:40:09 GMT\r\n"
          "Expires: Mon, 24 Nov 2014 12:45:26 GMT\r\n"
          "Content-Type: video/mp4\r\n" +
          via_header + ocl_header + "\r\n";

      FetchURLRequest(test_url, nullptr, video_response_headers,
                      kResponseContentLength, 0);
    } else {
      // Check https
      std::string response_headers =
          "HTTP/1.1 200 OK\r\n"
          "Date: Wed, 28 Nov 2007 09:40:09 GMT\r\n"
          "Expires: Mon, 24 Nov 2014 12:45:26 GMT\r\n" +
          via_header + ocl_header + "\r\n\r\n";

      FetchURLRequest(test_url, nullptr, response_headers,
                      kResponseContentLength, 0);
    }

    for (const auto& histogram : all_new_net_histograms) {
      auto expected_it = test.expected_histograms.find({histogram, 0});
      if (expected_it == test.expected_histograms.end()) {
        histogram_tester.ExpectTotalCount(histogram, 0);
      } else {
        histogram_tester.ExpectUniqueSample(expected_it->name,
                                            expected_it->value, 1);
      }
    }
  }
}

TEST_F(DataReductionProxyNetworkDelegateTest,
       NonServerLoFiResponseDoesNotTriggerInfobar) {
  Init(USE_INSECURE_PROXY, false);

  ClearLoFiUIService();
  lofi_decider()->SetIsUsingClientLoFi(false);
  std::string response_headers =
      "HTTP/1.1 200 OK\r\n"
      "Date: Wed, 28 Nov 2007 09:40:09 GMT\r\n"
      "Expires: Mon, 24 Nov 2014 12:45:26 GMT\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n"
      "Chrome-Proxy: ofcl=200\r\n\r\n";

  auto request =
      FetchURLRequest(GURL(kTestURL), nullptr, response_headers, 140, 0);

  EXPECT_FALSE(DataReductionProxyData::GetData(*request)->lofi_received());
  VerifyDidNotifyLoFiResponse(false);
}

TEST_F(DataReductionProxyNetworkDelegateTest,
       ServerLoFiResponseDoesTriggerInfobar) {
  Init(USE_INSECURE_PROXY, false);

  ClearLoFiUIService();
  lofi_decider()->SetIsUsingClientLoFi(false);
  std::string response_headers =
      "HTTP/1.1 200 OK\r\n"
      "Date: Wed, 28 Nov 2007 09:40:09 GMT\r\n"
      "Expires: Mon, 24 Nov 2014 12:45:26 GMT\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n"
      "Chrome-Proxy: ofcl=200\r\n"
      "Chrome-Proxy-Content-Transform: empty-image\r\n\r\n";

  auto request =
      FetchURLRequest(GURL(kTestURL), nullptr, response_headers, 140, 0);

  EXPECT_TRUE(DataReductionProxyData::GetData(*request)->lofi_received());
  VerifyDidNotifyLoFiResponse(true);
}

TEST_F(DataReductionProxyNetworkDelegateTest,
       NonClientLoFiResponseDoesNotTriggerInfobar) {
  Init(USE_INSECURE_PROXY, false);

  ClearLoFiUIService();
  lofi_decider()->SetIsUsingClientLoFi(false);

  FetchURLRequest(GURL(kTestURL), nullptr,
                  "HTTP/1.1 206 Partial Content\r\n"
                  "Date: Wed, 28 Nov 2007 09:40:09 GMT\r\n"
                  "Expires: Mon, 24 Nov 2014 12:45:26 GMT\r\n"
                  "Via: 1.1 Chrome-Compression-Proxy\r\n"
                  "Content-Range: bytes 0-139/2048\r\n\r\n",
                  140, 0);

  VerifyDidNotifyLoFiResponse(false);
}

TEST_F(DataReductionProxyNetworkDelegateTest,
       ClientLoFiCompleteResponseDoesNotTriggerInfobar) {
  Init(USE_INSECURE_PROXY, false);

  const char* const test_response_headers[] = {
      "HTTP/1.1 200 OK\r\n"
      "Date: Wed, 28 Nov 2007 09:40:09 GMT\r\n"
      "Expires: Mon, 24 Nov 2014 12:45:26 GMT\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n\r\n",

      "HTTP/1.1 204 No Content\r\n"
      "Date: Wed, 28 Nov 2007 09:40:09 GMT\r\n"
      "Expires: Mon, 24 Nov 2014 12:45:26 GMT\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n\r\n",

      "HTTP/1.1 404 Not Found\r\n"
      "Date: Wed, 28 Nov 2007 09:40:09 GMT\r\n"
      "Expires: Mon, 24 Nov 2014 12:45:26 GMT\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n\r\n",

      "HTTP/1.1 206 Partial Content\r\n"
      "Date: Wed, 28 Nov 2007 09:40:09 GMT\r\n"
      "Expires: Mon, 24 Nov 2014 12:45:26 GMT\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n"
      "Content-Range: bytes 0-139/140\r\n\r\n",
  };

  for (const char* headers : test_response_headers) {
    ClearLoFiUIService();
    lofi_decider()->SetIsUsingClientLoFi(true);
    FetchURLRequest(GURL(kTestURL), nullptr, headers, 140, 0);
    VerifyDidNotifyLoFiResponse(false);
  }
}

TEST_F(DataReductionProxyNetworkDelegateTest,
       ClientLoFiPartialRangeDoesTriggerInfobar) {
  Init(USE_INSECURE_PROXY, false);

  const char* const test_response_headers[] = {
      "HTTP/1.1 206 Partial Content\r\n"
      "Date: Wed, 28 Nov 2007 09:40:09 GMT\r\n"
      "Expires: Mon, 24 Nov 2014 12:45:26 GMT\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n"
      "Content-Range: bytes 0-139/2048\r\n\r\n",

      "HTTP/1.1 206 Partial Content\r\n"
      "Date: Wed, 28 Nov 2007 09:40:09 GMT\r\n"
      "Expires: Mon, 24 Nov 2014 12:45:26 GMT\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n"
      "Content-Range: bytes 5-144/145\r\n\r\n",
  };

  for (const char* headers : test_response_headers) {
    ClearLoFiUIService();
    lofi_decider()->SetIsUsingClientLoFi(true);
    FetchURLRequest(GURL(kTestURL), nullptr, headers, 140, 0);
    VerifyDidNotifyLoFiResponse(true);
  }
}

TEST_F(DataReductionProxyNetworkDelegateTest,
       TestLoFiTransformationTypeHistogram) {
  const std::string regular_response_headers =
      "HTTP/1.1 200 OK\r\n"
      "Content-Length: 140\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n"
      "Chrome-Proxy: ofcl=200\r\n"
      "Cache-Control: max-age=0\r\n"
      "Vary: accept-encoding\r\n\r\n";

  Init(USE_INSECURE_PROXY, false);
  const char kLoFiTransformationTypeHistogram[] =
      "DataReductionProxy.LoFi.TransformationType";
  base::HistogramTester histogram_tester;

  net::HttpRequestHeaders request_headers;
  request_headers.SetHeader("chrome-proxy-accept-transform", "lite-page");
  lofi_decider()->ignore_is_using_data_reduction_proxy_check();
  FetchURLRequest(GURL(kTestURL), &request_headers, regular_response_headers,
                  140, 0);
  histogram_tester.ExpectBucketCount(kLoFiTransformationTypeHistogram,
                                     NO_TRANSFORMATION_LITE_PAGE_REQUESTED, 1);

  std::string response_headers =
      "HTTP/1.1 200 OK\r\n"
      "Chrome-Proxy-Content-Transform: lite-page\r\n"
      "Date: Wed, 28 Nov 2007 09:40:09 GMT\r\n"
      "Expires: Mon, 24 Nov 2014 12:45:26 GMT\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n"
      "Chrome-Proxy: ofcl=200\r\n";

  response_headers += "\r\n";
  auto request =
      FetchURLRequest(GURL(kTestURL), nullptr, response_headers, 140, 0);
  EXPECT_TRUE(DataReductionProxyData::GetData(*request)->lite_page_received());

  histogram_tester.ExpectBucketCount(kLoFiTransformationTypeHistogram,
                                     LITE_PAGE, 1);
}

// Test that Brotli is not added to the accept-encoding header when it is
// disabled globally.
TEST_F(DataReductionProxyNetworkDelegateTest,
       BrotliAdvertisement_BrotliDisabled) {
  Init(USE_SECURE_PROXY, false /* enable_brotli_globally */);

  ReadBrotliFile();

  std::string response_headers =
      "HTTP/1.1 200 OK\r\n"
      "Content-Length: 140\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n"
      "Chrome-Proxy: ofcl=200\r\n"
      "Cache-Control: max-age=1200\r\n"
      "Vary: accept-encoding\r\n";
  response_headers += "\r\n";

  // Use secure sockets when fetching the request since Brotli is only enabled
  // for secure connections.
  FetchURLRequestAndVerifyBrotli(nullptr, response_headers, false, false);
}

// Test that Brotli is not added to the accept-encoding header when
// kDataReductionProxyBrotliHoldback feature is enabled.
TEST_F(DataReductionProxyNetworkDelegateTest,
       BrotliAdvertisement_BrotliHoldbackEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      data_reduction_proxy::features::kDataReductionProxyBrotliHoldback);

  Init(USE_SECURE_PROXY, true /* enable_brotli_globally */);

  ReadBrotliFile();

  std::string response_headers =
      "HTTP/1.1 200 OK\r\n"
      "Content-Length: 140\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n"
      "Chrome-Proxy: ofcl=200\r\n"
      "Cache-Control: max-age=1200\r\n"
      "Vary: accept-encoding\r\n";
  response_headers += "\r\n";

  // Use secure sockets when fetching the request since Brotli is only enabled
  // for secure connections.
  FetchURLRequestAndVerifyBrotli(nullptr, response_headers, false, false);
}

// Test that Brotli is not added to the accept-encoding header when the request
// is fetched from an insecure proxy.
TEST_F(DataReductionProxyNetworkDelegateTest,
       BrotliAdvertisementInsecureProxy) {
  Init(USE_INSECURE_PROXY, true /* enable_brotli_globally */);
  std::string response_headers =
      "HTTP/1.1 200 OK\r\n"
      "Content-Length: 140\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n"
      "Chrome-Proxy: ofcl=200\r\n"
      "Cache-Control: max-age=1200\r\n"
      "Vary: accept-encoding\r\n";
  response_headers += "\r\n";

  // Use secure sockets when fetching the request since Brotli is only enabled
  // for secure connections.
  std::unique_ptr<net::URLRequest> request(
      FetchURLRequest(GURL(kTestURL), nullptr, response_headers, 140, 0));
  EXPECT_EQ(140, request->received_response_content_length());
  EXPECT_NE(0, request->GetTotalSentBytes());
  EXPECT_NE(0, request->GetTotalReceivedBytes());
  EXPECT_FALSE(request->was_cached());
  // Brotli should be added to Accept Encoding header only if secure proxy is in
  VerifyBrotliPresent(request.get(), false, false);
}

// Test that Brotli is not added to the accept-encoding header when it is
// disabled via data reduction proxy field trial.
TEST_F(DataReductionProxyNetworkDelegateTest,
       BrotliAdvertisementDisabledViaFieldTrial) {
  Init(USE_SECURE_PROXY, true /* enable_brotli_globally */);

  base::FieldTrialList field_trial_list(nullptr);
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "DataReductionProxyBrotliAcceptEncoding", "Disabled"));

  std::string response_headers =
      "HTTP/1.1 200 OK\r\n"
      "Content-Length: 140\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n"
      "Chrome-Proxy: ofcl=200\r\n"
      "Cache-Control: max-age=1200\r\n"
      "Vary: accept-encoding\r\n";
  response_headers += "\r\n";

  FetchURLRequestAndVerifyBrotli(nullptr, response_headers, false, false);
  FetchURLRequestAndVerifyBrotli(nullptr, response_headers, true, false);
}

// Test that Brotli is correctly added to the accept-encoding header when it is
// enabled globally.
TEST_F(DataReductionProxyNetworkDelegateTest, BrotliAdvertisement) {
  Init(USE_SECURE_PROXY, true /* enable_brotli_globally */);

  std::string response_headers =
      "HTTP/1.1 200 OK\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n"
      "Chrome-Proxy: ofcl=200\r\n"
      "Cache-Control: max-age=1200\r\n"
      "Content-Encoding: br\r\n"
      "Vary: accept-encoding\r\n";
  response_headers += "\r\n";

  FetchURLRequestAndVerifyBrotli(nullptr, response_headers, false, true);
  FetchURLRequestAndVerifyBrotli(nullptr, response_headers, true, true);
}

// Test that Brotli is not added a second time to the Accept-Encoding header
// when it is enabled globally but already present in the pre-existing header.
TEST_F(DataReductionProxyNetworkDelegateTest,
       BrotliAdvertisementAcceptEncodingIncludesBr) {
  Init(USE_SECURE_PROXY, true /* enable_brotli_globally */);

  net::HttpRequestHeaders request_headers;
  request_headers.SetHeader("Accept-Encoding", "gzip, deflate, br");

  std::string response_headers =
      "HTTP/1.1 200 OK\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n"
      "Chrome-Proxy: ofcl=200\r\n"
      "Cache-Control: max-age=1200\r\n"
      "Content-Encoding: br\r\n"
      "Vary: accept-encoding\r\n";
  response_headers += "\r\n";

  FetchURLRequestAndVerifyBrotli(&request_headers, response_headers, false,
                                 true);
  FetchURLRequestAndVerifyBrotli(&request_headers, response_headers, true,
                                 true);
}

// Test that Brotli is correctly added to the Accept-Encoding header when it is
// enabled globally and the pre-existing header is empty.
TEST_F(DataReductionProxyNetworkDelegateTest,
       BrotliAdvertisementAcceptEncodingEmpty) {
  Init(USE_SECURE_PROXY, true /* enable_brotli_globally */);

  net::HttpRequestHeaders request_headers;
  request_headers.SetHeader("Accept-Encoding", "");

  std::string response_headers =
      "HTTP/1.1 200 OK\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n"
      "Chrome-Proxy: ofcl=200\r\n"
      "Cache-Control: max-age=1200\r\n"
      "Content-Encoding: br\r\n"
      "Vary: accept-encoding\r\n";
  response_headers += "\r\n";

  FetchURLRequestAndVerifyBrotli(&request_headers, response_headers, false,
                                 true);
  FetchURLRequestAndVerifyBrotli(&request_headers, response_headers, true,
                                 true);
}

TEST_F(DataReductionProxyNetworkDelegateTest, IncrementingMainFramePageId) {
  // This is unaffacted by brotil and insecure proxy.
  Init(USE_SECURE_PROXY, false /* enable_brotli_globally */);

  io_data()->request_options()->SetSecureSession("new-session");
  uint64_t page_id = io_data()->request_options()->GeneratePageId();

  FetchURLRequestAndVerifyPageIdDirective(++page_id, false);

  FetchURLRequestAndVerifyPageIdDirective(++page_id, false);

  FetchURLRequestAndVerifyPageIdDirective(++page_id, false);
}

TEST_F(DataReductionProxyNetworkDelegateTest, ResetSessionResetsId) {
  // This is unaffacted by brotil and insecure proxy.
  Init(USE_SECURE_PROXY, false /* enable_brotli_globally */);

  io_data()->request_options()->SetSecureSession("new-session");
  uint64_t page_id = io_data()->request_options()->GeneratePageId();

  FetchURLRequestAndVerifyPageIdDirective(++page_id, false);

  io_data()->request_options()->SetSecureSession("new-session-2");

  page_id = io_data()->request_options()->GeneratePageId();
  FetchURLRequestAndVerifyPageIdDirective(++page_id, false);
}

TEST_F(DataReductionProxyNetworkDelegateTest, SubResourceNoPageId) {
  // This is unaffacted by brotil and insecure proxy.
  Init(USE_SECURE_PROXY, false /* enable_brotli_globally */);
  io_data()->request_options()->SetSecureSession("new-session");
  FetchURLRequestAndVerifyPageIdDirective(base::Optional<uint64_t>(), false);
}

TEST_F(DataReductionProxyNetworkDelegateTest, RedirectSharePid) {
  // The test manually controls the fetch of warmup URL and the response.
  DisableWarmupURLFetchCallback();

  // This is unaffacted by brotil and insecure proxy.
  Init(USE_SECURE_PROXY, false /* enable_brotli_globally */);

  io_data()->request_options()->SetSecureSession("new-session");
  uint64_t page_id = io_data()->request_options()->GeneratePageId();

  FetchURLRequestAndVerifyPageIdDirective(++page_id, true);
}

TEST_F(DataReductionProxyNetworkDelegateTest,
       SessionChangeResetsPageIDOnRedirect) {
  // This test calls directly into network delegate as it is difficult to mock
  // state changing in between redirects within an URLRequest's lifetime.

  // This is unaffacted by brotil and insecure proxy.
  Init(USE_INSECURE_PROXY, false /* enable_brotli_globally */);
  net::ProxyInfo data_reduction_proxy_info;
  data_reduction_proxy_info.UseProxyServer(
      params()->proxies_for_http().front().proxy_server());

  std::unique_ptr<net::URLRequest> request =
      context()->CreateRequest(GURL(kTestURL), net::RequestPriority::IDLE,
                               nullptr, TRAFFIC_ANNOTATION_FOR_TESTS);
  request->SetLoadFlags(net::LOAD_MAIN_FRAME_DEPRECATED);
  io_data()->request_options()->SetSecureSession("fake-session");

  uint64_t page_id = io_data()->request_options()->GeneratePageId();

  net::HttpRequestHeaders headers;
  net::ProxyRetryInfoMap proxy_retry_info;

  // Send a request and verify the page ID is 1.
  network_delegate()->NotifyBeforeStartTransaction(
      request.get(),
      base::BindOnce(&DataReductionProxyNetworkDelegateTest::DelegateStageDone,
                     base::Unretained(this)),
      &headers);
  network_delegate()->NotifyBeforeSendHeaders(
      request.get(), data_reduction_proxy_info, proxy_retry_info, &headers);
  DataReductionProxyData* data = DataReductionProxyData::GetData(*request);
  EXPECT_TRUE(data_reduction_proxy_info.is_http());
  EXPECT_EQ(++page_id, data->page_id().value());

  // Send a second request and verify the page ID incremements.
  request = context()->CreateRequest(GURL(kTestURL), net::RequestPriority::IDLE,
                                     nullptr, TRAFFIC_ANNOTATION_FOR_TESTS);
  request->SetLoadFlags(net::LOAD_MAIN_FRAME_DEPRECATED);

  network_delegate()->NotifyBeforeStartTransaction(
      request.get(),
      base::BindOnce(&DataReductionProxyNetworkDelegateTest::DelegateStageDone,
                     base::Unretained(this)),
      &headers);
  network_delegate()->NotifyBeforeSendHeaders(
      request.get(), data_reduction_proxy_info, proxy_retry_info, &headers);
  data = DataReductionProxyData::GetData(*request);
  EXPECT_EQ(++page_id, data->page_id().value());

  // Verify that redirects are the same page ID.
  network_delegate()->NotifyBeforeRedirect(request.get(), GURL(kTestURL));
  network_delegate()->NotifyBeforeSendHeaders(
      request.get(), data_reduction_proxy_info, proxy_retry_info, &headers);
  data = DataReductionProxyData::GetData(*request);
  EXPECT_EQ(page_id, data->page_id().value());

  network_delegate()->NotifyBeforeRedirect(request.get(), GURL(kTestURL));
  io_data()->request_options()->SetSecureSession("new-session");

  page_id = io_data()->request_options()->GeneratePageId();
  network_delegate()->NotifyBeforeSendHeaders(
      request.get(), data_reduction_proxy_info, proxy_retry_info, &headers);
  data = DataReductionProxyData::GetData(*request);
  EXPECT_EQ(++page_id, data->page_id().value());
}

// Test that effective connection type is correctly added to the request
// headers when it is enabled using field trial. The server is varying on the
// effective connection type (ECT).
TEST_F(DataReductionProxyNetworkDelegateTest, ECTHeaderEnabledWithVary) {
  Init(USE_SECURE_PROXY, false /* enable_brotli_globally */);

  std::string response_headers =
      "HTTP/1.1 200 OK\r\n"
      "Content-Length: 140\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n"
      "Cache-Control: max-age=1200\r\n"
      "Vary: chrome-proxy-ect\r\n"
      "Chrome-Proxy: ofcl=200\r\n\r\n";

  int response_body_size = 140;
  std::string response_body(base::checked_cast<size_t>(response_body_size),
                            ' ');

  std::vector<net::MockRead> reads_list;
  std::vector<std::string> mock_writes;
  std::vector<net::MockWrite> writes_list;

  std::vector<net::EffectiveConnectionType> effective_connection_types;
  effective_connection_types.push_back(net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);
  effective_connection_types.push_back(net::EFFECTIVE_CONNECTION_TYPE_2G);

  BuildSocket(response_headers, response_body, true, effective_connection_types,
              &reads_list, &mock_writes, &writes_list);

  // Add 2 socket providers since 2 requests in this test are fetched from the
  // network.
  FetchURLRequestAndVerifyECTHeader(effective_connection_types[0], true, false);

  // When the ECT is set to the same value, fetching the same resource should
  // result in a cache hit.
  FetchURLRequestAndVerifyECTHeader(effective_connection_types[0], true, true);

  // When the ECT is set to a different value, the response should not be
  // served from the cache.
  FetchURLRequestAndVerifyECTHeader(effective_connection_types[1], true, false);
}

// Test that effective connection type is correctly added to the request
// headers when it is enabled using field trial. The server is not varying on
// the effective connection type (ECT).
TEST_F(DataReductionProxyNetworkDelegateTest, ECTHeaderEnabledWithoutVary) {
  Init(USE_SECURE_PROXY, false /* enable_brotli_globally */);

  std::string response_headers =
      "HTTP/1.1 200 OK\r\n"
      "Content-Length: 140\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n"
      "Cache-Control: max-age=1200\r\n"
      "Chrome-Proxy: ofcl=200\r\n\r\n";

  int response_body_size = 140;
  std::string response_body(base::checked_cast<size_t>(response_body_size),
                            ' ');

  std::vector<net::MockRead> reads_list;
  std::vector<std::string> mock_writes;
  std::vector<net::MockWrite> writes_list;

  std::vector<net::EffectiveConnectionType> effective_connection_types;
  effective_connection_types.push_back(net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);
  effective_connection_types.push_back(net::EFFECTIVE_CONNECTION_TYPE_2G);

  BuildSocket(response_headers, response_body, true, effective_connection_types,
              &reads_list, &mock_writes, &writes_list);

  // Add 1 socket provider since 1 request in this test is fetched from the
  // network.
  FetchURLRequestAndVerifyECTHeader(effective_connection_types[0], true, false);

  // When the ECT is set to the same value, fetching the same resource should
  // result in a cache hit.
  FetchURLRequestAndVerifyECTHeader(effective_connection_types[0], true, true);

  // When the ECT is set to a different value, the response should still be
  // served from the cache.
  FetchURLRequestAndVerifyECTHeader(effective_connection_types[1], true, true);
}

class DataReductionProxyNetworkDelegateClientLoFiTest : public testing::Test {
 public:
  DataReductionProxyNetworkDelegateClientLoFiTest() : baseline_savings_(0) {}
  ~DataReductionProxyNetworkDelegateClientLoFiTest() override;

  void Reset() {
    drp_test_context_.reset();
    mock_socket_factory_.reset();
    context_storage_.reset();

    context_.reset(new net::TestURLRequestContext(true));
    context_storage_.reset(new net::URLRequestContextStorage(context_.get()));
    mock_socket_factory_.reset(new net::MockClientSocketFactory());
    context_->set_client_socket_factory(mock_socket_factory_.get());

    net::ProxyServer proxy_server = net::ProxyServer::FromURI(
        "http://origin.net:80", net::ProxyServer::SCHEME_HTTP);

    proxy_resolution_service_ =
        net::ProxyResolutionService::CreateFixedFromPacResult(
            proxy_server.ToPacString(), TRAFFIC_ANNOTATION_FOR_TESTS);
    context_->set_proxy_resolution_service(proxy_resolution_service_.get());

    drp_test_context_ =
        DataReductionProxyTestContext::Builder()
            .WithURLRequestContext(context_.get())
            .WithMockClientSocketFactory(mock_socket_factory_.get())
            .WithProxiesForHttp({DataReductionProxyServer(
                proxy_server, ProxyServer::UNSPECIFIED_TYPE)})
            .Build();

    drp_test_context_->AttachToURLRequestContext(context_storage_.get());
    context_->Init();
    base::RunLoop().RunUntilIdle();

    baseline_savings_ =
        drp_test_context()->settings()->GetTotalHttpContentLengthSaved();
  }

  void SetUpLoFiDecider(bool is_client_lofi_image,
                        bool is_client_lofi_auto_reload) const {
    std::unique_ptr<TestLoFiDecider> lofi_decider(new TestLoFiDecider());
    lofi_decider->SetIsUsingClientLoFi(is_client_lofi_image);
    lofi_decider->SetIsClientLoFiAutoReload(is_client_lofi_auto_reload);
    drp_test_context_->io_data()->set_lofi_decider(
        std::unique_ptr<LoFiDecider>(std::move(lofi_decider)));
  }

  int64_t GetSavings() const {
    return drp_test_context()->settings()->GetTotalHttpContentLengthSaved() -
           baseline_savings_;
  }

  net::TestURLRequestContext* context() const { return context_.get(); }
  net::MockClientSocketFactory* mock_socket_factory() const {
    return mock_socket_factory_.get();
  }
  DataReductionProxyTestContext* drp_test_context() const {
    return drp_test_context_.get();
  }

 private:
  base::MessageLoopForIO loop;
  std::unique_ptr<net::TestURLRequestContext> context_;
  std::unique_ptr<net::URLRequestContextStorage> context_storage_;
  std::unique_ptr<net::MockClientSocketFactory> mock_socket_factory_;
  std::unique_ptr<net::ProxyResolutionService> proxy_resolution_service_;
  std::unique_ptr<DataReductionProxyTestContext> drp_test_context_;
  int64_t baseline_savings_;
};

DataReductionProxyNetworkDelegateClientLoFiTest::
    ~DataReductionProxyNetworkDelegateClientLoFiTest() {}

TEST_F(DataReductionProxyNetworkDelegateClientLoFiTest, DataSavingsNonDRP) {
  const char kSimple200ResponseHeaders[] =
      "HTTP/1.1 200 OK\r\n"
      "Content-Length: 140\r\n\r\n";

  const struct {
    const char* headers;
    size_t response_length;
    bool is_client_lofi_image;
    bool is_client_lofi_auto_reload;
    int64_t expected_savings;
  } tests[] = {
      // 200 responses shouldn't see any savings.
      {kSimple200ResponseHeaders, 140, false, false, 0},
      {kSimple200ResponseHeaders, 140, true, false, 0},

      // Client Lo-Fi Auto-reload responses should see negative savings.
      {kSimple200ResponseHeaders, 140, false, true,
       -(static_cast<int64_t>(sizeof(kSimple200ResponseHeaders) - 1) + 140)},
      {kSimple200ResponseHeaders, 140, true, true,
       -(static_cast<int64_t>(sizeof(kSimple200ResponseHeaders) - 1) + 140)},

      // A range response that doesn't use Client Lo-Fi shouldn't see any
      // savings.
      {"HTTP/1.1 206 Partial Content\r\n"
       "Content-Range: bytes 0-2047/10000\r\n"
       "Content-Length: 2048\r\n\r\n",
       2048, false, false, 0},

      // A Client Lo-Fi range response should see savings based on the
      // Content-Range header.
      {"HTTP/1.1 206 Partial Content\r\n"
       "Content-Range: bytes 0-2047/10000\r\n"
       "Content-Length: 2048\r\n\r\n",
       2048, true, false, 10000 - 2048},

      // A Client Lo-Fi range response should see savings based on the
      // Content-Range header, which in this case is 0 savings because the range
      // response contained the entire resource.
      {"HTTP/1.1 206 Partial Content\r\n"
       "Content-Range: bytes 0-999/1000\r\n"
       "Content-Length: 1000\r\n\r\n",
       1000, true, false, 0},

      // Client Lo-Fi range responses that don't have a Content-Range with the
      // full resource length shouldn't see any savings.
      {"HTTP/1.1 206 Partial Content\r\n"
       "Content-Length: 2048\r\n\r\n",
       2048, true, false, 0},
      {"HTTP/1.1 206 Partial Content\r\n"
       "Content-Range: bytes 0-2047/*\r\n"
       "Content-Length: 2048\r\n\r\n",
       2048, true, false, 0},
      {"HTTP/1.1 206 Partial Content\r\n"
       "Content-Range: invalid_content_range\r\n"
       "Content-Length: 2048\r\n\r\n",
       2048, true, false, 0},
  };

  for (const auto& test : tests) {
    Reset();
    SetUpLoFiDecider(test.is_client_lofi_image,
                     test.is_client_lofi_auto_reload);

    std::string response_body(test.response_length, 'a');
    net::MockRead reads[] = {net::MockRead(test.headers),
                             net::MockRead(response_body.c_str()),
                             net::MockRead(net::ASYNC, net::OK)};
    net::StaticSocketDataProvider socket(reads, base::span<net::MockWrite>());
    mock_socket_factory()->AddSocketDataProvider(&socket);

    net::TestDelegate test_delegate;
    std::unique_ptr<net::URLRequest> request = context()->CreateRequest(
        GURL("http://example.com"), net::RequestPriority::IDLE, &test_delegate,
        TRAFFIC_ANNOTATION_FOR_TESTS);

    request->SetLoadFlags(request->load_flags() | net::LOAD_BYPASS_PROXY);

    request->Start();
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(test.expected_savings, GetSavings()) << (&test - tests);
  }
}

TEST_F(DataReductionProxyNetworkDelegateClientLoFiTest, DataSavingsThroughDRP) {
  Reset();
  drp_test_context()->DisableWarmupURLFetch();
  drp_test_context()->EnableDataReductionProxyWithSecureProxyCheckSuccess();
  SetUpLoFiDecider(true, false);

  const char kHeaders[] =
      "HTTP/1.1 206 Partial Content\r\n"
      "Content-Range: bytes 0-2047/10000\r\n"
      "Content-Length: 2048\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n"
      "Chrome-Proxy: ofcl=3000\r\n\r\n";

  std::string response_body(2048, 'a');
  net::MockRead reads[] = {net::MockRead(kHeaders),
                           net::MockRead(response_body.c_str()),
                           net::MockRead(net::ASYNC, net::OK)};
  net::StaticSocketDataProvider socket(reads, base::span<net::MockWrite>());
  mock_socket_factory()->AddSocketDataProvider(&socket);

  net::TestDelegate test_delegate;
  std::unique_ptr<net::URLRequest> request = context()->CreateRequest(
      GURL("http://example.com"), net::RequestPriority::IDLE, &test_delegate,
      TRAFFIC_ANNOTATION_FOR_TESTS);

  request->Start();
  base::RunLoop().RunUntilIdle();

  // Since the Data Reduction Proxy is enabled, the length of the raw headers
  // should be used in the estimated original size. The X-OCL should be ignored.
  EXPECT_EQ(static_cast<int64_t>(net::HttpUtil::AssembleRawHeaders(
                                     kHeaders, sizeof(kHeaders) - 1)
                                     .size() +
                                 10000 - request->GetTotalReceivedBytes()),
            GetSavings());
}

TEST_F(DataReductionProxyNetworkDelegateTest, TestAcceptTransformHistogram) {
  Init(USE_INSECURE_PROXY, false);
  base::HistogramTester histogram_tester;

  const std::string regular_response_headers =
      "HTTP/1.1 200 OK\r\n"
      "Content-Length: 140\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n"
      "Chrome-Proxy: ofcl=200\r\n"
      "Cache-Control: max-age=0\r\n"
      "Vary: accept-encoding\r\n\r\n";

  const char kResponseHeadersWithCPCTFormat[] =
      "HTTP/1.1 200 OK\r\n"
      "Chrome-Proxy-Content-Transform: %s\r\n"
      "Date: Wed, 28 Nov 2007 09:40:09 GMT\r\n"
      "Expires: Mon, 24 Nov 2014 12:45:26 GMT\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n"
      "Chrome-Proxy: ofcl=200\r\n"
      "\r\n";

  // Verify lite page request.
  net::HttpRequestHeaders request_headers;
  request_headers.SetHeader("chrome-proxy-accept-transform", "lite-page");
  FetchURLRequest(GURL(kTestURL), &request_headers, regular_response_headers,
                  140, 0);
  histogram_tester.ExpectTotalCount(
      "DataReductionProxy.Protocol.AcceptTransform", 1);
  histogram_tester.ExpectBucketCount(
      "DataReductionProxy.Protocol.AcceptTransform",
      0 /* LITE_PAGE_REQUESTED */, 1);
  // Check legacy histogram too:
  histogram_tester.ExpectBucketCount(
      "DataReductionProxy.LoFi.TransformationType",
      NO_TRANSFORMATION_LITE_PAGE_REQUESTED, 1);

  // Verify empty image request.
  request_headers.SetHeader("chrome-proxy-accept-transform", "empty-image");
  FetchURLRequest(GURL(kTestURL), &request_headers, regular_response_headers,
                  140, 0);
  histogram_tester.ExpectTotalCount(
      "DataReductionProxy.Protocol.AcceptTransform", 2);
  histogram_tester.ExpectBucketCount(
      "DataReductionProxy.Protocol.AcceptTransform",
      3 /* EMPTY_IMAGE_REQUESTED */, 1);

  // Verify lite page response.
  auto request = FetchURLRequest(
      GURL(kTestURL), nullptr,
      base::StringPrintf(kResponseHeadersWithCPCTFormat, "lite-page"), 140, 0);
  EXPECT_TRUE(DataReductionProxyData::GetData(*request)->lite_page_received());
  histogram_tester.ExpectTotalCount(
      "DataReductionProxy.Protocol.AcceptTransform", 3);
  histogram_tester.ExpectBucketCount(
      "DataReductionProxy.Protocol.AcceptTransform",
      1 /* LITE_PAGE_TRANSFORM_RECEIVED */, 1);
  // Check legacy histogram too:
  histogram_tester.ExpectBucketCount(
      "DataReductionProxy.LoFi.TransformationType", LITE_PAGE, 1);

  // Verify page policy response.
  std::string response_headers =
      "HTTP/1.1 200 OK\r\n"
      "Chrome-Proxy: page-policies=empty-image\r\n"
      "Date: Wed, 28 Nov 2007 09:40:09 GMT\r\n"
      "Expires: Mon, 24 Nov 2014 12:45:26 GMT\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n"
      "Chrome-Proxy: ofcl=200\r\n"
      "\r\n";
  request = FetchURLRequest(GURL(kTestURL), nullptr, response_headers, 140, 0);
  EXPECT_FALSE(DataReductionProxyData::GetData(*request)->lite_page_received());
  histogram_tester.ExpectTotalCount(
      "DataReductionProxy.Protocol.AcceptTransform", 4);
  histogram_tester.ExpectBucketCount(
      "DataReductionProxy.Protocol.AcceptTransform",
      2 /* EMPTY_IMAGE_POLICY_DIRECTIVE_RECEIVED */, 1);

  // Verify empty image response.
  request = FetchURLRequest(
      GURL(kTestURL), nullptr,
      base::StringPrintf(kResponseHeadersWithCPCTFormat, "empty-image"), 140,
      0);
  EXPECT_TRUE(DataReductionProxyData::GetData(*request)->lofi_received());
  histogram_tester.ExpectTotalCount(
      "DataReductionProxy.Protocol.AcceptTransform", 5);
  histogram_tester.ExpectBucketCount(
      "DataReductionProxy.Protocol.AcceptTransform",
      4 /* EMPTY_IMAGE_TRANSFORM_RECEIVED */, 1);

  // Verify compressed-video request.
  request_headers.SetHeader("chrome-proxy-accept-transform",
                            "compressed-video");
  FetchURLRequest(GURL(kTestURL), &request_headers, std::string(), 140, 0);
  histogram_tester.ExpectTotalCount(
      "DataReductionProxy.Protocol.AcceptTransform", 6);
  histogram_tester.ExpectBucketCount(
      "DataReductionProxy.Protocol.AcceptTransform",
      5 /* COMPRESSED_VIDEO_REQUESTED */, 1);

  // Verify compressed-video response.
  request = FetchURLRequest(
      GURL(kTestURL), nullptr,
      base::StringPrintf(kResponseHeadersWithCPCTFormat, "compressed-video"),
      140, 0);
  EXPECT_FALSE(DataReductionProxyData::GetData(*request)->lofi_received());
  histogram_tester.ExpectTotalCount(
      "DataReductionProxy.Protocol.AcceptTransform", 7);
  histogram_tester.ExpectBucketCount(
      "DataReductionProxy.Protocol.AcceptTransform",
      8 /* COMPRESSED_VIDEO_RECEIVED */, 1);

  // Verify response with an unknown CPAT value.
  request = FetchURLRequest(GURL(kTestURL), nullptr,
                            base::StringPrintf(kResponseHeadersWithCPCTFormat,
                                               "this-is-a-fake-transform"),
                            140, 0);
  EXPECT_FALSE(DataReductionProxyData::GetData(*request)->lofi_received());
  histogram_tester.ExpectTotalCount(
      "DataReductionProxy.Protocol.AcceptTransform", 8);
  histogram_tester.ExpectBucketCount(
      "DataReductionProxy.Protocol.AcceptTransform",
      9 /* UNKNOWN_TRANSFORM_RECEIVED */, 1);
}

TEST_F(DataReductionProxyNetworkDelegateTest, RecordNonContentToOtherHost) {
  const std::string response_headers =
      "HTTP/1.1 200 OK\r\n"
      "Date: Wed, 28 Nov 2007 09:40:09 GMT\r\n"
      "Expires: Mon, 24 Nov 2014 12:45:26 GMT\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n"
      "\r\n";

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      data_reduction_proxy::features::
          kDataSaverSiteBreakdownUsingPageLoadMetrics);
  Init(USE_INSECURE_PROXY, false);
  EnableDataUsageReporting();
  auto test_resource_type_provider =
      std::make_unique<TestResourceTypeProvider>();
  test_resource_type_provider->set_is_non_content_initiated_request(true);
  io_data()->set_resource_type_provider(std::move(test_resource_type_provider));

  FetchURLRequest(GURL(kTestURL), nullptr, response_headers,
                  kResponseContentLength, 0);
  base::RunLoop().RunUntilIdle();
  DCHECK_EQ(response_headers.size() + kResponseContentLength,
            GetOtherHostDataUsage());
}

}  // namespace

}  // namespace data_reduction_proxy
