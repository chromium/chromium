// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/domain_reliability/uploader.h"

#include <stddef.h>

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "components/domain_reliability/test_util.h"
#include "net/base/isolation_info.h"
#include "net/base/load_flags.h"
#include "net/cookies/site_for_cookies.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/http/http_util.h"
#include "net/log/net_log_with_source.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_interceptor.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace domain_reliability {
namespace {

const char kUploadURL[] = "https://example/upload";

struct MockUploadResult {
  int net_error;
  scoped_refptr<net::HttpResponseHeaders> response_headers;
};

class UploadMockURLRequestJob : public net::URLRequestJob {
 public:
  UploadMockURLRequestJob(net::URLRequest* request, MockUploadResult result)
      : net::URLRequestJob(request), upload_stream_(nullptr), result_(result) {
    EXPECT_FALSE(request->allow_credentials());
    EXPECT_TRUE(request->load_flags() & net::LOAD_DO_NOT_SAVE_COOKIES);
  }

  ~UploadMockURLRequestJob() override = default;

 protected:
  void Start() override {
    int rv = upload_stream_->Init(
        base::BindOnce(&UploadMockURLRequestJob::OnStreamInitialized,
                       base::Unretained(this)),
        net::NetLogWithSource());
    if (rv == net::ERR_IO_PENDING)
      return;
    OnStreamInitialized(rv);
  }

  void SetUpload(net::UploadDataStream* upload_stream) override {
    upload_stream_ = upload_stream;
  }

 private:
  void OnStreamInitialized(int rv) {
    EXPECT_EQ(net::OK, rv);

    size_t upload_size = upload_stream_->size();
    upload_buffer_ = base::MakeRefCounted<net::IOBufferWithSize>(upload_size);
    rv = upload_stream_->Read(
        upload_buffer_.get(), upload_size,
        base::BindOnce(&UploadMockURLRequestJob::OnStreamRead,
                       base::Unretained(this)));
    if (rv == net::ERR_IO_PENDING)
      return;
    OnStreamRead(rv);
  }

  void OnStreamRead(int rv) {
    EXPECT_EQ(upload_buffer_->size(), rv);

    upload_data_ = std::string(upload_buffer_->data(), upload_buffer_->size());
    upload_buffer_ = nullptr;

    if (result_.net_error == net::OK) {
      NotifyHeadersComplete();
    } else if (result_.net_error != net::ERR_IO_PENDING) {
      NotifyStartError(result_.net_error);
    }
  }

  void GetResponseInfo(net::HttpResponseInfo* info) override {
    info->headers = result_.response_headers;
  }

  raw_ptr<net::UploadDataStream> upload_stream_;
  scoped_refptr<net::IOBufferWithSize> upload_buffer_;
  std::string upload_data_;
  MockUploadResult result_;
};

class UploadInterceptor : public net::URLRequestInterceptor {
 public:
  explicit UploadInterceptor(
      const net::IsolationInfo& expected_network_isolation_info)
      : expected_network_isolation_info_(expected_network_isolation_info),
        request_count_(0),
        last_upload_depth_(-1) {}

  ~UploadInterceptor() override { EXPECT_TRUE(results_.empty()); }

  std::unique_ptr<net::URLRequestJob> MaybeInterceptRequest(
      net::URLRequest* request) const override {
    EXPECT_TRUE(expected_network_isolation_info_.IsEqualForTesting(
        request->isolation_info()));

    EXPECT_FALSE(results_.empty());
    MockUploadResult result = results_.front();
    results_.pop_front();

    last_upload_depth_ =
        DomainReliabilityUploader::GetURLRequestUploadDepth(*request);

    ++request_count_;

    return std::make_unique<UploadMockURLRequestJob>(request, result);
  }

  void ExpectRequestAndReturnError(int net_error) {
    MockUploadResult result;
    result.net_error = net_error;
    results_.push_back(result);
  }

  void ExpectRequestAndReturnResponseHeaders(const char* headers) {
    MockUploadResult result;
    result.net_error = net::OK;
    result.response_headers = base::MakeRefCounted<net::HttpResponseHeaders>(
        net::HttpUtil::AssembleRawHeaders(headers));
    results_.push_back(result);
  }

  int request_count() const { return request_count_; }
  int last_upload_depth() const { return last_upload_depth_; }

 private:
  const net::IsolationInfo expected_network_isolation_info_;

  mutable std::list<MockUploadResult> results_;
  mutable int request_count_;
  mutable int last_upload_depth_;
};

class TestUploadCallback {
 public:
  TestUploadCallback() : called_count_(0u) {}

  DomainReliabilityUploader::UploadCallback callback() {
    return base::BindOnce(&TestUploadCallback::OnCalled,
                          base::Unretained(this));
  }

  unsigned called_count() const { return called_count_; }
  DomainReliabilityUploader::UploadResult last_result() const {
    return last_result_;
  }

 private:
  void OnCalled(const DomainReliabilityUploader::UploadResult& result) {
    called_count_++;
    last_result_ = result;
  }

  unsigned called_count_;
  DomainReliabilityUploader::UploadResult last_result_;
};

class DomainReliabilityUploaderTest : public testing::Test {
 protected:
  DomainReliabilityUploaderTest()
      : url_request_context_(
            net::CreateTestURLRequestContextBuilder()->Build()),
        uploader_(
            DomainReliabilityUploader::Create(&time_,
                                              url_request_context_.get())) {
    expected_isolation_info_ = net::IsolationInfo::CreateTransient();

    auto interceptor =
        std::make_unique<UploadInterceptor>(expected_isolation_info_);
    interceptor_ = interceptor.get();
    EXPECT_TRUE(net::URLRequestFilter::GetInstance()->AddUrlInterceptor(
        GURL(kUploadURL), std::move(interceptor)));

    uploader_->SetDiscardUploads(false);
  }

  ~DomainReliabilityUploaderTest() override {
    interceptor_ = nullptr;
    net::URLRequestFilter::GetInstance()->ClearHandlers();
  }

  DomainReliabilityUploader* uploader() const { return uploader_.get(); }
  UploadInterceptor* interceptor() const { return interceptor_; }
  net::URLRequestContext* url_request_context() {
    return url_request_context_.get();
  }

  const net::IsolationInfo& isolation_info() const {
    return expected_isolation_info_;
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};

  net::IsolationInfo expected_isolation_info_;

  std::unique_ptr<net::URLRequestContext> url_request_context_;
  raw_ptr<UploadInterceptor> interceptor_;
  MockTime time_;
  std::unique_ptr<DomainReliabilityUploader> uploader_;
};

TEST_F(DomainReliabilityUploaderTest, Null) {
  uploader()->Shutdown();
}

TEST_F(DomainReliabilityUploaderTest, SuccessfulUpload) {
  interceptor()->ExpectRequestAndReturnResponseHeaders("HTTP/1.1 200\r\n\r\n");

  TestUploadCallback c;
  uploader()->UploadReport("{}", 0, GURL(kUploadURL), isolation_info(),
                           c.callback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, c.called_count());
  EXPECT_TRUE(c.last_result().is_success());

  uploader()->Shutdown();
}

TEST_F(DomainReliabilityUploaderTest, NetworkErrorUpload) {
  interceptor()->ExpectRequestAndReturnError(net::ERR_CONNECTION_REFUSED);

  TestUploadCallback c;
  uploader()->UploadReport("{}", 0, GURL(kUploadURL), isolation_info(),
                           c.callback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, c.called_count());
  EXPECT_TRUE(c.last_result().is_failure());

  uploader()->Shutdown();
}

TEST_F(DomainReliabilityUploaderTest, ServerErrorUpload) {
  interceptor()->ExpectRequestAndReturnResponseHeaders("HTTP/1.1 500\r\n\r\n");

  TestUploadCallback c;
  uploader()->UploadReport("{}", 0, GURL(kUploadURL), isolation_info(),
                           c.callback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, c.called_count());
  EXPECT_TRUE(c.last_result().is_failure());

  uploader()->Shutdown();
}

TEST_F(DomainReliabilityUploaderTest, RetryAfterUpload) {
  interceptor()->ExpectRequestAndReturnResponseHeaders(
      "HTTP/1.1 503 Ugh\nRetry-After: 3600\n\n");

  TestUploadCallback c;
  uploader()->UploadReport("{}", 0, GURL(kUploadURL), isolation_info(),
                           c.callback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, c.called_count());
  EXPECT_TRUE(c.last_result().is_retry_after());

  uploader()->Shutdown();
}

TEST_F(DomainReliabilityUploaderTest, UploadDepth1) {
  interceptor()->ExpectRequestAndReturnResponseHeaders("HTTP/1.1 200\r\n\r\n");

  TestUploadCallback c;
  uploader()->UploadReport("{}", 0, GURL(kUploadURL), isolation_info(),
                           c.callback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, c.called_count());

  EXPECT_EQ(1, interceptor()->last_upload_depth());

  uploader()->Shutdown();
}

TEST_F(DomainReliabilityUploaderTest, UploadDepth2) {
  interceptor()->ExpectRequestAndReturnResponseHeaders("HTTP/1.1 200\r\n\r\n");

  TestUploadCallback c;
  uploader()->UploadReport("{}", 1, GURL(kUploadURL), isolation_info(),
                           c.callback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, c.called_count());

  EXPECT_EQ(2, interceptor()->last_upload_depth());

  uploader()->Shutdown();
}

TEST_F(DomainReliabilityUploaderTest, UploadCanceledAtShutdown) {
  interceptor()->ExpectRequestAndReturnError(net::ERR_IO_PENDING);

  TestUploadCallback c;
  uploader()->UploadReport("{}", 1, GURL(kUploadURL), isolation_info(),
                           c.callback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, interceptor()->request_count());
  EXPECT_EQ(0u, c.called_count());

  uploader()->Shutdown();

  EXPECT_EQ(0u, c.called_count());

  url_request_context()->AssertNoURLRequests();
}

TEST_F(DomainReliabilityUploaderTest, NoUploadAfterShutdown) {
  uploader()->Shutdown();

  TestUploadCallback c;
  uploader()->UploadReport("{}", 1, GURL(kUploadURL), isolation_info(),
                           c.callback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, c.called_count());
  EXPECT_EQ(0, interceptor()->request_count());
}

}  // namespace
}  // namespace domain_reliability
