// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/domain_reliability/uploader.h"

#include <stddef.h>

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/domain_reliability/test_util.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/log/net_log_with_source.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_interceptor.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace domain_reliability {
namespace {

const char kUploadURL[] = "https://example/upload";

struct MockUploadResult {
  int net_error;
  scoped_refptr<net::HttpResponseHeaders> response_headers;
};

class UploadMockURLRequestJob : public net::URLRequestJob {
 public:
  UploadMockURLRequestJob(net::URLRequest* request,
                          net::NetworkDelegate* network_delegate,
                          MockUploadResult result)
      : net::URLRequestJob(request, network_delegate),
        upload_stream_(nullptr),
        result_(result) {
    int load_flags = request->load_flags();
    EXPECT_TRUE(load_flags & net::LOAD_DO_NOT_SEND_COOKIES);
    EXPECT_TRUE(load_flags & net::LOAD_DO_NOT_SAVE_COOKIES);
  }

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
  ~UploadMockURLRequestJob() override {}

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

    if (result_.net_error == net::OK)
      NotifyHeadersComplete();
    else if (result_.net_error != net::ERR_IO_PENDING)
      NotifyStartError(net::URLRequestStatus::FromError(result_.net_error));
  }

  void GetResponseInfo(net::HttpResponseInfo* info) override {
    info->headers = result_.response_headers;
  }

  net::UploadDataStream* upload_stream_;
  scoped_refptr<net::IOBufferWithSize> upload_buffer_;
  std::string upload_data_;
  MockUploadResult result_;
};

class UploadInterceptor : public net::URLRequestInterceptor {
 public:
  UploadInterceptor() : request_count_(0), last_upload_depth_(-1) {}

  ~UploadInterceptor() override { EXPECT_TRUE(results_.empty()); }

  net::URLRequestJob* MaybeInterceptRequest(
      net::URLRequest* request,
      net::NetworkDelegate* delegate) const override {
    EXPECT_FALSE(results_.empty());
    MockUploadResult result = results_.front();
    results_.pop_front();

    last_upload_depth_ =
        DomainReliabilityUploader::GetURLRequestUploadDepth(*request);

    ++request_count_;

    return new UploadMockURLRequestJob(request, delegate, result);
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
  mutable std::list<MockUploadResult> results_;
  mutable int request_count_;
  mutable int last_upload_depth_;
};

class TestUploadCallback {
 public:
  TestUploadCallback() : called_count_(0u) {}

  DomainReliabilityUploader::UploadCallback callback() {
    return base::Bind(&TestUploadCallback::OnCalled, base::Unretained(this));
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
      : url_request_context_getter_(new net::TestURLRequestContextGetter(
            base::ThreadTaskRunnerHandle::Get())),
        interceptor_(new UploadInterceptor()),
        uploader_(
            DomainReliabilityUploader::Create(&time_,
                                              url_request_context_getter_)) {
    net::URLRequestFilter::GetInstance()->AddUrlInterceptor(
        GURL(kUploadURL), base::WrapUnique(interceptor_));
    uploader_->SetDiscardUploads(false);
  }

  ~DomainReliabilityUploaderTest() override {
    net::URLRequestFilter::GetInstance()->ClearHandlers();
  }

  DomainReliabilityUploader* uploader() const { return uploader_.get(); }
  UploadInterceptor* interceptor() const { return interceptor_; }
  scoped_refptr<net::TestURLRequestContextGetter> url_request_context_getter() {
    return url_request_context_getter_;
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};
  scoped_refptr<net::TestURLRequestContextGetter> url_request_context_getter_;
  UploadInterceptor* interceptor_;
  MockTime time_;
  std::unique_ptr<DomainReliabilityUploader> uploader_;
};

TEST_F(DomainReliabilityUploaderTest, Null) {
  uploader()->Shutdown();
}

TEST_F(DomainReliabilityUploaderTest, SuccessfulUpload) {
  interceptor()->ExpectRequestAndReturnResponseHeaders("HTTP/1.1 200\r\n\r\n");

  TestUploadCallback c;
  uploader()->UploadReport("{}", 0, GURL(kUploadURL), c.callback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, c.called_count());
  EXPECT_TRUE(c.last_result().is_success());

  uploader()->Shutdown();
}

TEST_F(DomainReliabilityUploaderTest, NetworkErrorUpload) {
  interceptor()->ExpectRequestAndReturnError(net::ERR_CONNECTION_REFUSED);

  TestUploadCallback c;
  uploader()->UploadReport("{}", 0, GURL(kUploadURL), c.callback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, c.called_count());
  EXPECT_TRUE(c.last_result().is_failure());

  uploader()->Shutdown();
}

TEST_F(DomainReliabilityUploaderTest, ServerErrorUpload) {
  interceptor()->ExpectRequestAndReturnResponseHeaders("HTTP/1.1 500\r\n\r\n");

  TestUploadCallback c;
  uploader()->UploadReport("{}", 0, GURL(kUploadURL), c.callback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, c.called_count());
  EXPECT_TRUE(c.last_result().is_failure());

  uploader()->Shutdown();
}

TEST_F(DomainReliabilityUploaderTest, RetryAfterUpload) {
  interceptor()->ExpectRequestAndReturnResponseHeaders(
      "HTTP/1.1 503 Ugh\nRetry-After: 3600\n\n");

  TestUploadCallback c;
  uploader()->UploadReport("{}", 0, GURL(kUploadURL), c.callback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, c.called_count());
  EXPECT_TRUE(c.last_result().is_retry_after());

  uploader()->Shutdown();
}

TEST_F(DomainReliabilityUploaderTest, UploadDepth1) {
  interceptor()->ExpectRequestAndReturnResponseHeaders("HTTP/1.1 200\r\n\r\n");

  TestUploadCallback c;
  uploader()->UploadReport("{}", 0, GURL(kUploadURL), c.callback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, c.called_count());

  EXPECT_EQ(1, interceptor()->last_upload_depth());

  uploader()->Shutdown();
}

TEST_F(DomainReliabilityUploaderTest, UploadDepth2) {
  interceptor()->ExpectRequestAndReturnResponseHeaders("HTTP/1.1 200\r\n\r\n");

  TestUploadCallback c;
  uploader()->UploadReport("{}", 1, GURL(kUploadURL), c.callback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, c.called_count());

  EXPECT_EQ(2, interceptor()->last_upload_depth());

  uploader()->Shutdown();
}

TEST_F(DomainReliabilityUploaderTest, UploadCanceledAtShutdown) {
  interceptor()->ExpectRequestAndReturnError(net::ERR_IO_PENDING);

  TestUploadCallback c;
  uploader()->UploadReport("{}", 1, GURL(kUploadURL), c.callback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, interceptor()->request_count());
  EXPECT_EQ(0u, c.called_count());

  uploader()->Shutdown();

  EXPECT_EQ(0u, c.called_count());

  url_request_context_getter()->GetURLRequestContext()->AssertNoURLRequests();
}

TEST_F(DomainReliabilityUploaderTest, NoUploadAfterShutdown) {
  uploader()->Shutdown();

  TestUploadCallback c;
  uploader()->UploadReport("{}", 1, GURL(kUploadURL), c.callback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, c.called_count());
  EXPECT_EQ(0, interceptor()->request_count());
}

}  // namespace
}  // namespace domain_reliability
