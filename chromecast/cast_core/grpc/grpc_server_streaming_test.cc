// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "base/files/scoped_temp_dir.h"
#include "base/guid.h"
#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "chromecast/cast_core/grpc/grpc_server.h"
#include "chromecast/cast_core/grpc/status_matchers.h"
#include "chromecast/cast_core/grpc/test_service.castcore.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cast {
namespace utils {
namespace {

using ::cast::test::StatusIs;

const auto kEventTimeout = base::Seconds(1);
const auto kServerStopTimeout = base::Seconds(1);

class GrpcServerStreamingTest : public ::testing::Test {
 protected:
  GrpcServerStreamingTest()
      : grpc_client_task_runner_(
            base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})) {
    CHECK(temp_dir_.CreateUniqueTempDir());
    endpoint_ = "unix:" +
                temp_dir_.GetPath()
                    .AppendASCII("cast-uds-" + base::GenerateGUID().substr(24))
                    .value();
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  scoped_refptr<base::SequencedTaskRunner> grpc_client_task_runner_;
  base::ScopedTempDir temp_dir_;
  std::string endpoint_;
};

TEST_F(GrpcServerStreamingTest, ServerStreamingCallSucceeds) {
  const int kMaxResponseCount = base::RandInt(10, 300);
  int server_response_count = 0;
  auto writes_available_callback = base::BindPostTask(
      grpc_client_task_runner_,
      base::BindLambdaForTesting(
          [&](GrpcStatusOr<
              ServerStreamingServiceHandler::StreamingCall::Reactor*> reactor) {
            CU_CHECK_OK(reactor);
            if (server_response_count < kMaxResponseCount) {
              TestResponse response;
              response.set_bar(
                  base::StringPrintf("test_bar%d", ++server_response_count));
              reactor.value()->Write(std::move(response));
            } else {
              LOG(INFO) << "Writing finished";
              reactor.value()->Write(grpc::Status::OK);
            }
          }));
  auto call_handler = base::BindPostTask(
      grpc_client_task_runner_,
      base::BindLambdaForTesting(
          [&](TestRequest request,
              ServerStreamingServiceHandler::StreamingCall::Reactor* reactor) {
            EXPECT_EQ(request.foo(), "test_foo");

            reactor->SetWritesAvailableCallback(
                std::move(writes_available_callback));

            TestResponse response;
            response.set_bar(
                base::StringPrintf("test_bar%d", ++server_response_count));
            reactor->Write(std::move(response));
          }));

  GrpcServer server;
  server.SetHandler<ServerStreamingServiceHandler::StreamingCall>(
      std::move(call_handler));
  server.Start(endpoint_);

  ServerStreamingServiceStub stub(endpoint_);
  auto call = stub.CreateCall<ServerStreamingServiceStub::StreamingCall>();
  call.request().set_foo("test_foo");
  int call_count = 0;
  base::WaitableEvent response_received_event;
  std::move(call).InvokeAsync(base::BindPostTask(
      grpc_client_task_runner_,
      base::BindLambdaForTesting(
          [&](GrpcStatusOr<TestResponse> response, bool done) {
            CU_CHECK_OK(response);
            if (done) {
              response_received_event.Signal();
            } else {
              EXPECT_EQ(response->bar(),
                        base::StringPrintf("test_bar%d", ++call_count));
            }
          })));
  ASSERT_TRUE(response_received_event.TimedWait(kEventTimeout));
  ASSERT_EQ(call_count, kMaxResponseCount);

  server.StopForTesting(kServerStopTimeout);
}

TEST_F(GrpcServerStreamingTest, ServerStreamingCallFailsRightAway) {
  GrpcServer server;
  server.SetHandler<ServerStreamingServiceHandler::StreamingCall>(
      base::BindPostTask(
          grpc_client_task_runner_,
          base::BindLambdaForTesting(
              [&](TestRequest request,
                  ServerStreamingServiceHandler::StreamingCall::Reactor*
                      reactor) {
                EXPECT_EQ(request.foo(), "test_foo");
                reactor->Write(
                    grpc::Status(grpc::StatusCode::NOT_FOUND, "not found"));
              })));
  server.Start(endpoint_);

  ServerStreamingServiceStub stub(endpoint_);
  auto call = stub.CreateCall<ServerStreamingServiceStub::StreamingCall>();
  call.request().set_foo("test_foo");
  base::WaitableEvent response_received_event;
  std::move(call).InvokeAsync(base::BindPostTask(
      grpc_client_task_runner_,
      base::BindLambdaForTesting(
          [&](GrpcStatusOr<TestResponse> response, bool done) {
            CHECK(done);
            ASSERT_THAT(response.status(),
                        StatusIs(grpc::StatusCode::NOT_FOUND, "not found"));
            response_received_event.Signal();
          })));
  {
    base::ScopedAllowBaseSyncPrimitivesForTesting allow_base_sync_primitives;
    ASSERT_TRUE(response_received_event.TimedWait(kEventTimeout));
  }

  server.StopForTesting(kServerStopTimeout);
}

TEST_F(GrpcServerStreamingTest, ServerStreamingCallCancelledIfServerIsStopped) {
  GrpcServer server;
  base::WaitableEvent server_stopped_event;
  server.SetHandler<ServerStreamingServiceHandler::StreamingCall>(
      base::BindPostTask(
          grpc_client_task_runner_,
          base::BindLambdaForTesting(
              [&](TestRequest request,
                  ServerStreamingServiceHandler::StreamingCall::Reactor*
                      reactor) {
                // Stop the server to trigger call cancellation.
                server.Stop(base::Milliseconds(100),
                            base::BindLambdaForTesting(
                                [&]() { server_stopped_event.Signal(); }));
              })));
  server.Start(endpoint_);

  ServerStreamingServiceStub stub(endpoint_);
  auto call = stub.CreateCall<ServerStreamingServiceStub::StreamingCall>();
  call.request().set_foo("test_foo");
  base::WaitableEvent response_received_event;
  std::move(call).InvokeAsync(base::BindPostTask(
      grpc_client_task_runner_,
      base::BindLambdaForTesting(
          [&](GrpcStatusOr<TestResponse> response, bool done) {
            ASSERT_THAT(response, StatusIs(grpc::StatusCode::UNAVAILABLE));
            response_received_event.Signal();
          })));
  {
    base::ScopedAllowBaseSyncPrimitivesForTesting allow_base_sync_primitives;
    ASSERT_TRUE(server_stopped_event.TimedWait(kEventTimeout));
    ASSERT_TRUE(response_received_event.TimedWait(kEventTimeout));
  }
}

}  // namespace
}  // namespace utils
}  // namespace cast
