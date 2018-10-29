// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_network_delegate.h"

#include <utility>

#include "base/test/scoped_task_environment.h"
#include "chromecast/browser/cast_network_request_interceptor.h"
#include "net/base/request_priority.h"
#include "net/base/test_completion_callback.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Return;

namespace chromecast {
namespace shell {

namespace {

class MockCastNetworkRequestInterceptor : public CastNetworkRequestInterceptor {
 public:
  MockCastNetworkRequestInterceptor() {}
  ~MockCastNetworkRequestInterceptor() override {}

  MOCK_CONST_METHOD5(IsWhiteListed,
                     bool(const GURL& gurl,
                          const std::string& session_id,
                          int render_process_id,
                          int render_frame_id,
                          bool for_device_auth));
  MOCK_METHOD0(Initialize, void());
  MOCK_METHOD0(IsInitialized, bool());
  MOCK_METHOD6(OnBeforeURLRequest,
               int(net::URLRequest* request,
                   const std::string& session_id,
                   int render_process_id,
                   int render_frame_id,
                   net::CompletionOnceCallback callback,
                   GURL* new_url));
  MOCK_METHOD3(OnBeforeStartTransaction,
               int(net::URLRequest* request,
                   net::CompletionOnceCallback callback,
                   net::HttpRequestHeaders* headers));
  MOCK_METHOD1(OnURLRequestDestroyed, void(net::URLRequest* request));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockCastNetworkRequestInterceptor);
};

}  // namespace

class CastNetworkDelegateTest : public testing::Test {
 public:
  CastNetworkDelegateTest()
      : task_env_(base::test::ScopedTaskEnvironment::MainThreadType::DEFAULT,
                  base::test::ScopedTaskEnvironment::ExecutionMode::QUEUED),
        context_(true) {
    context_.Init();
    std::unique_ptr<MockCastNetworkRequestInterceptor>
        cast_network_request_interceptor_ =
            std::make_unique<MockCastNetworkRequestInterceptor>();
    cast_network_request_interceptor_ptr_ =
        cast_network_request_interceptor_.get();
    cast_network_delegate_ = std::make_unique<CastNetworkDelegate>(
        std::move(cast_network_request_interceptor_));
  }
  ~CastNetworkDelegateTest() override{};

 protected:
  base::test::ScopedTaskEnvironment task_env_;

  std::unique_ptr<CastNetworkDelegate> cast_network_delegate_;
  MockCastNetworkRequestInterceptor* cast_network_request_interceptor_ptr_;

  net::TestURLRequestContext context_;
  net::TestDelegate delegate_;
};

TEST_F(CastNetworkDelegateTest, NotifyBeforeURLRequest) {
  std::unique_ptr<net::URLRequest> request = context_.CreateRequest(
      GURL(), net::IDLE, &delegate_, TRAFFIC_ANNOTATION_FOR_TESTS);
  net::TestCompletionCallback completion_callback;

  EXPECT_CALL(*cast_network_request_interceptor_ptr_, IsInitialized())
      .WillOnce(Return(true));
  EXPECT_CALL(*cast_network_request_interceptor_ptr_,
              OnBeforeURLRequest(_, _, _, _, _, _));

  cast_network_delegate_->NotifyBeforeURLRequest(
      request.get(), completion_callback.callback(), NULL);
  task_env_.RunUntilIdle();
}

TEST_F(CastNetworkDelegateTest, NotifyBeforeStartTransaction) {
  std::unique_ptr<net::URLRequest> request = context_.CreateRequest(
      GURL(), net::IDLE, &delegate_, TRAFFIC_ANNOTATION_FOR_TESTS);
  net::TestCompletionCallback completion_callback;
  std::unique_ptr<net::HttpRequestHeaders> request_headers(
      new net::HttpRequestHeaders());

  EXPECT_CALL(*cast_network_request_interceptor_ptr_, IsInitialized())
      .WillOnce(Return(true));
  EXPECT_CALL(*cast_network_request_interceptor_ptr_,
              OnBeforeStartTransaction(_, _, _));

  cast_network_delegate_->NotifyBeforeStartTransaction(
      request.get(), completion_callback.callback(), request_headers.get());
  task_env_.RunUntilIdle();
}

TEST_F(CastNetworkDelegateTest, NotifyBeforeURLRequestDestroyed) {
  std::unique_ptr<net::URLRequest> request = context_.CreateRequest(
      GURL(), net::IDLE, &delegate_, TRAFFIC_ANNOTATION_FOR_TESTS);

  EXPECT_CALL(*cast_network_request_interceptor_ptr_, IsInitialized())
      .WillOnce(Return(true));
  EXPECT_CALL(*cast_network_request_interceptor_ptr_,
              OnURLRequestDestroyed(request.get()));

  cast_network_delegate_->NotifyURLRequestDestroyed(request.get());
  task_env_.RunUntilIdle();
}

}  // namespace shell
}  // namespace chromecast
