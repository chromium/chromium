// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/scoped_temp_dir.h"
#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "chromecast/cast_core/grpc/grpc_server.h"
#include "chromecast/cast_core/grpc/status_matchers.h"
#include "chromecast/cast_core/grpc/test_service.castcore.pb.h"
#include "chromecast/cast_core/grpc/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cast {
namespace utils {
namespace {

using ::cast::test::StatusIs;
using ::testing::IsNull;

const auto kEventTimeout = base::Seconds(1);
const auto kServerStopTimeout = base::Seconds(1);

class GrpcServerStreamingTest : public ::testing::Test {
 protected:
  GrpcServerStreamingTest() {
    CHECK(temp_dir_.CreateUniqueTempDir());
    endpoint_ =
        "unix:" +
        temp_dir_.GetPath()
            .AppendASCII(
                "cast-uds-" +
                base::Uuid::GenerateRandomV4().AsLowercaseString().substr(24))
            .value();
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::ScopedTempDir temp_dir_;
  std::string endpoint_;
};

TEST_F(GrpcServerStreamingTest, ServerStreamingCallSucceeds) {
  const int kMaxResponseCount = base::RandInt(10, 300);
  int server_response_count = 0;
  auto writes_available_callback = base::BindLambdaForTesting(
      [&](grpc::Status status,
          ServerStreamingServiceHandler::StreamingCall::Reactor* reactor) {
        CU_CHECK_OK(status);
        if (server_response_count < kMaxResponseCount) {
          TestResponse response;
          response.set_bar(
              base::StringPrintf("test_bar%d", ++server_response_count));
          reactor->Write(std::move(response));
        } else {
          LOG(INFO) << "Writing finished";
          reactor->Write(grpc::Status::OK);
        }
      });
  auto call_handler = base::BindLambdaForTesting(
      [&](TestRequest request,
          ServerStreamingServiceHandler::StreamingCall::Reactor* reactor) {
        EXPECT_EQ(request.foo(), "test_foo");

        reactor->SetWritesAvailableCallback(
            std::move(writes_available_callback));

        TestResponse response;
        response.set_bar(
            base::StringPrintf("test_bar%d", ++server_response_count));
        reactor->Write(std::move(response));
      });

  GrpcServer server;
  server.SetHandler<ServerStreamingServiceHandler::StreamingCall>(
      std::move(call_handler));
  ASSERT_THAT(server.Start(endpoint_), StatusIs(grpc::StatusCode::OK));

  ServerStreamingServiceStub stub(endpoint_);
  auto call = stub.CreateCall<ServerStreamingServiceStub::StreamingCall>();
  call.request().set_foo("test_foo");
  int call_count = 0;
  base::WaitableEvent response_received_event;
  std::move(call).InvokeAsync(base::BindLambdaForTesting(
      [&](GrpcStatusOr<TestResponse> response, bool done) {
        CU_CHECK_OK(response);
        if (done) {
          response_received_event.Signal();
        } else {
          EXPECT_EQ(response->bar(),
                    base::StringPrintf("test_bar%d", ++call_count));
        }
      }));
  ASSERT_TRUE(response_received_event.TimedWait(kEventTimeout));
  ASSERT_EQ(call_count, kMaxResponseCount);

  test::StopGrpcServer(server, kServerStopTimeout);
}

TEST_F(GrpcServerStreamingTest, ServerStreamingCallFailsRightAway) {
  GrpcServer server;
  server.SetHandler<ServerStreamingServiceHandler::StreamingCall>(
      base::BindLambdaForTesting(
          [&](TestRequest request,
              ServerStreamingServiceHandler::StreamingCall::Reactor* reactor) {
            EXPECT_EQ(request.foo(), "test_foo");
            reactor->Write(
                grpc::Status(grpc::StatusCode::NOT_FOUND, "not found"));
          }));
  ASSERT_THAT(server.Start(endpoint_), StatusIs(grpc::StatusCode::OK));

  ServerStreamingServiceStub stub(endpoint_);
  auto call = stub.CreateCall<ServerStreamingServiceStub::StreamingCall>();
  call.request().set_foo("test_foo");
  base::WaitableEvent response_received_event;
  std::move(call).InvokeAsync(base::BindLambdaForTesting(
      [&](GrpcStatusOr<TestResponse> response, bool done) {
        CHECK(done);
        ASSERT_THAT(response.status(),
                    StatusIs(grpc::StatusCode::NOT_FOUND, "not found"));
        response_received_event.Signal();
      }));
  ASSERT_TRUE(response_received_event.TimedWait(kEventTimeout));

  test::StopGrpcServer(server, kServerStopTimeout);
}

TEST_F(GrpcServerStreamingTest, ServerStreamingCallCancelledIfServerIsStopped) {
  GrpcServer server;
  server.SetHandler<ServerStreamingServiceHandler::StreamingCall>(
      base::BindLambdaForTesting(
          [&](TestRequest request,
              ServerStreamingServiceHandler::StreamingCall::Reactor* reactor) {
            reactor->Write(grpc::Status::OK);
          }));
  ASSERT_THAT(server.Start(endpoint_), StatusIs(grpc::StatusCode::OK));

  ServerStreamingServiceStub stub(endpoint_);
  auto call = stub.CreateCall<ServerStreamingServiceStub::StreamingCall>();
  call.request().set_foo("test_foo");
  base::WaitableEvent response_received_event{
      base::WaitableEvent::ResetPolicy::AUTOMATIC};
  std::move(call).InvokeAsync(base::BindLambdaForTesting(
      [&](GrpcStatusOr<TestResponse> response, bool done) {
        CU_ASSERT_OK(response);
        response_received_event.Signal();
      }));
  ASSERT_TRUE(response_received_event.TimedWait(kEventTimeout));

  test::StopGrpcServer(server, kServerStopTimeout);

  auto call1 = stub.CreateCall<ServerStreamingServiceStub::StreamingCall>();
  std::move(call1).InvokeAsync(base::BindLambdaForTesting(
      [&](GrpcStatusOr<TestResponse> response, bool done) {
        ASSERT_THAT(response, StatusIs(grpc::StatusCode::UNAVAILABLE));
        response_received_event.Signal();
      }));
  ASSERT_TRUE(response_received_event.TimedWait(kEventTimeout));
}

TEST_F(GrpcServerStreamingTest, ServerStreamingCallIsCancelledByClient) {
  base::WaitableEvent server_aborted_event;
  auto writes_available_callback = base::BindLambdaForTesting(
      [&](grpc::Status status,
          ServerStreamingServiceHandler::StreamingCall::Reactor* reactor) {
        // The write callback can be called at any point in time with
        // ABORTED error, so ignore the success call.
        if (status.ok()) {
          return;
        }
        ASSERT_THAT(status, StatusIs(grpc::StatusCode::ABORTED));
        ASSERT_THAT(reactor, IsNull());
        server_aborted_event.Signal();
      });
  auto call_handler = base::BindLambdaForTesting(
      [&](TestRequest request,
          ServerStreamingServiceHandler::StreamingCall::Reactor* reactor) {
        EXPECT_EQ(request.foo(), "test_foo");
        reactor->SetWritesAvailableCallback(
            std::move(writes_available_callback));
        TestResponse response;
        response.set_bar("test_bar");
        reactor->Write(std::move(response));
      });

  GrpcServer server;
  server.SetHandler<ServerStreamingServiceHandler::StreamingCall>(
      std::move(call_handler));
  ASSERT_THAT(server.Start(endpoint_), StatusIs(grpc::StatusCode::OK));

  size_t response_count = 0;
  base::WaitableEvent response_received_event{
      base::WaitableEvent::ResetPolicy::AUTOMATIC};
  ServerStreamingServiceStub stub(endpoint_);
  auto call = stub.CreateCall<ServerStreamingServiceStub::StreamingCall>();
  call.request().set_foo("test_foo");
  auto context = std::move(call).InvokeAsync(base::BindLambdaForTesting(
      [&](GrpcStatusOr<TestResponse> response, bool done) {
        // Only one success response should be received.
        ++response_count;
        if (response_count == 1) {
          CU_CHECK_OK(response);
          EXPECT_EQ(response->bar(), "test_bar");
          response_received_event.Signal();
        } else {
          EXPECT_EQ(response_count, 2u);
          ASSERT_THAT(response, StatusIs(grpc::StatusCode::CANCELLED));
          response_received_event.Signal();
        }
      }));
  ASSERT_TRUE(response_received_event.TimedWait(kEventTimeout));

  // Cancel the client call and wait for server and client to get the
  // notification.
  context.Cancel();
  ASSERT_TRUE(server_aborted_event.TimedWait(kEventTimeout));
  ASSERT_TRUE(response_received_event.TimedWait(kEventTimeout));
  task_environment_.RunUntilIdle();

  test::StopGrpcServer(server, kServerStopTimeout);
  task_environment_.RunUntilIdle();
}

TEST_F(GrpcServerStreamingTest,
       ServerStreamingCallIsCancelledByClientInActiveReactor) {
  GrpcServer server;

  base::WaitableEvent server_request_received;
  ServerStreamingServiceHandler::StreamingCall::Reactor* cancelled_reactor;
  server.SetHandler<ServerStreamingServiceHandler::StreamingCall>(
      base::BindLambdaForTesting(
          [&](TestRequest request,
              ServerStreamingServiceHandler::StreamingCall::Reactor* reactor) {
            cancelled_reactor = reactor;
            server_request_received.Signal();
          }));
  ASSERT_THAT(server.Start(endpoint_), StatusIs(grpc::StatusCode::OK));

  base::WaitableEvent response_received_event;
  ServerStreamingServiceStub stub(endpoint_);
  auto call = stub.CreateCall<ServerStreamingServiceStub::StreamingCall>();
  auto context = std::move(call).InvokeAsync(base::BindLambdaForTesting(
      [&](GrpcStatusOr<TestResponse> response, bool done) {
        ASSERT_THAT(response, StatusIs(grpc::StatusCode::CANCELLED));
        response_received_event.Signal();
      }));

  // Wait for server to receive the request.
  ASSERT_TRUE(server_request_received.TimedWait(kEventTimeout));

  // Cancel the client call and wait for server and client to get the
  // notification.
  context.Cancel();
  ASSERT_TRUE(response_received_event.TimedWait(kEventTimeout));

  // Try to write a response with the reactor, and expect the ABORTED error.
  base::WaitableEvent reactor_aborted;
  cancelled_reactor->SetWritesAvailableCallback(base::BindLambdaForTesting(
      [&](grpc::Status status,
          ServerStreamingServiceHandler::StreamingCall::Reactor* reactor) {
        ASSERT_THAT(reactor, IsNull());
        ASSERT_THAT(status, StatusIs(grpc::StatusCode::ABORTED));
        reactor_aborted.Signal();
      }));
  // The handler might have already been called by gRPC framework, so write an
  // empty message to release its deferred state.
  cancelled_reactor->Write(TestResponse());

  ASSERT_TRUE(reactor_aborted.TimedWait(kEventTimeout));

  test::StopGrpcServer(server, kServerStopTimeout);
}

}  // namespace
}  // namespace utils
}  // namespace cast
