// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "chromecast/cast_core/grpc/grpc_server.h"
#include "chromecast/cast_core/grpc/status_matchers.h"
#include "chromecast/cast_core/grpc/test_service.castcore.pb.h"
#include "chromecast/cast_core/grpc/test_service_extra.castcore.pb.h"
#include "chromecast/cast_core/grpc/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cast {
namespace utils {
namespace {

using ::cast::test::StatusIs;

const auto kEventTimeout = base::Seconds(1);
const auto kServerManualStopTimeout = base::Milliseconds(100);
const auto kServerStopTimeout = base::Milliseconds(100);

class GrpcUnaryTest : public ::testing::Test {
 protected:
  GrpcUnaryTest() {
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

TEST_F(GrpcUnaryTest, SyncUnaryCallSucceeds) {
  GrpcServer server;
  server.SetHandler<SimpleServiceHandler::SimpleCall>(
      base::BindLambdaForTesting(
          [](TestRequest request,
             SimpleServiceHandler::SimpleCall::Reactor* reactor) {
            EXPECT_EQ(request.foo(), "test_foo");
            TestResponse response;
            response.set_bar("test_bar");
            reactor->Write(std::move(response));
          }));
  ASSERT_THAT(server.Start(endpoint_), StatusIs(grpc::StatusCode::OK));

  SimpleServiceStub stub(endpoint_);
  auto call = stub.CreateCall<SimpleServiceStub::SimpleCall>();
  call.request().set_foo("test_foo");
  auto response = std::move(call).Invoke();
  CU_ASSERT_OK(response);
  EXPECT_EQ(response->bar(), "test_bar");

  test::StopGrpcServer(server, kServerStopTimeout);
}

TEST_F(GrpcUnaryTest, SyncUnaryCallReturnsErrorStatus) {
  GrpcServer server;
  server.SetHandler<SimpleServiceHandler::SimpleCall>(
      base::BindLambdaForTesting(
          [&](TestRequest request,
              SimpleServiceHandler::SimpleCall::Reactor* reactor) {
            EXPECT_EQ(request.foo(), "test_foo");
            reactor->Write(
                grpc::Status(grpc::StatusCode::NOT_FOUND, "Not found"));
          }));
  ASSERT_THAT(server.Start(endpoint_), StatusIs(grpc::StatusCode::OK));

  SimpleServiceStub stub(endpoint_);
  auto call = stub.CreateCall<SimpleServiceStub::SimpleCall>();
  call.request().set_foo("test_foo");
  auto response = std::move(call).Invoke();
  ASSERT_THAT(response.status(),
              StatusIs(grpc::StatusCode::NOT_FOUND, "Not found"));

  test::StopGrpcServer(server, kServerStopTimeout);
}

TEST_F(GrpcUnaryTest, SyncUnaryCallCancelledIfServerIsStopped) {
  GrpcServer server;
  base::WaitableEvent server_stopped_event;
  server.SetHandler<SimpleServiceHandler::SimpleCall>(
      base::BindLambdaForTesting(
          [&](TestRequest request,
              SimpleServiceHandler::SimpleCall::Reactor* reactor) {
            // Stop the server to trigger call cancellation.
            server.Stop(kServerManualStopTimeout.InMilliseconds(),
                        base::BindLambdaForTesting(
                            [&]() { server_stopped_event.Signal(); }));
          }));
  ASSERT_THAT(server.Start(endpoint_), StatusIs(grpc::StatusCode::OK));

  SimpleServiceStub stub(endpoint_);
  auto call = stub.CreateCall<SimpleServiceStub::SimpleCall>();
  call.request().set_foo("test_foo");
  auto response = std::move(call).Invoke();
  ASSERT_THAT(response, StatusIs(grpc::StatusCode::UNAVAILABLE));

  // Need to wait for server to fully stop.
  ASSERT_TRUE(server_stopped_event.TimedWait(kEventTimeout));
}

TEST_F(GrpcUnaryTest, AsyncUnaryCallSucceeds) {
  GrpcServer server;
  server.SetHandler<SimpleServiceHandler::SimpleCall>(
      base::BindLambdaForTesting(
          [](TestRequest request,
             SimpleServiceHandler::SimpleCall::Reactor* reactor) {
            EXPECT_EQ(request.foo(), "test_foo");
            TestResponse response;
            response.set_bar("test_bar");
            reactor->Write(std::move(response));
          }));
  ASSERT_THAT(server.Start(endpoint_), StatusIs(grpc::StatusCode::OK));

  SimpleServiceStub stub(endpoint_);
  auto call = stub.CreateCall<SimpleServiceStub::SimpleCall>();
  call.request().set_foo("test_foo");
  base::WaitableEvent response_received_event;
  std::move(call).InvokeAsync(
      base::BindLambdaForTesting([&](GrpcStatusOr<TestResponse> response) {
        CU_CHECK_OK(response);
        EXPECT_EQ(response->bar(), "test_bar");
        response_received_event.Signal();
      }));
  ASSERT_TRUE(response_received_event.TimedWait(kEventTimeout));

  test::StopGrpcServer(server, kServerStopTimeout);
}

TEST_F(GrpcUnaryTest, AsyncUnaryCallReturnsErrorStatus) {
  GrpcServer server;
  server.SetHandler<SimpleServiceHandler::SimpleCall>(
      base::BindLambdaForTesting(
          [&](TestRequest request,
              SimpleServiceHandler::SimpleCall::Reactor* reactor) {
            EXPECT_EQ(request.foo(), "test_foo");
            reactor->Write(
                grpc::Status(grpc::StatusCode::NOT_FOUND, "Not Found"));
          }));
  ASSERT_THAT(server.Start(endpoint_), StatusIs(grpc::StatusCode::OK));

  SimpleServiceStub stub(endpoint_);
  auto call = stub.CreateCall<SimpleServiceStub::SimpleCall>();
  call.request().set_foo("test_foo");
  base::WaitableEvent response_received_event;
  std::move(call).InvokeAsync(
      base::BindLambdaForTesting([&](GrpcStatusOr<TestResponse> response) {
        ASSERT_THAT(response.status(),
                    StatusIs(grpc::StatusCode::NOT_FOUND, "Not Found"));
        response_received_event.Signal();
      }));
  ASSERT_TRUE(response_received_event.TimedWait(kEventTimeout));

  test::StopGrpcServer(server, kServerStopTimeout);
}

TEST_F(GrpcUnaryTest, AsyncUnaryCallCancelledIfServerIsStopped) {
  GrpcServer server;
  base::WaitableEvent server_stopped_event;
  server.SetHandler<SimpleServiceHandler::SimpleCall>(
      base::BindLambdaForTesting(
          [&](TestRequest request,
              SimpleServiceHandler::SimpleCall::Reactor* reactor) {
            // Stop the server to trigger call cancellation.
            server.Stop(kServerManualStopTimeout.InMilliseconds(),
                        base::BindLambdaForTesting(
                            [&]() { server_stopped_event.Signal(); }));
          }));
  ASSERT_THAT(server.Start(endpoint_), StatusIs(grpc::StatusCode::OK));

  SimpleServiceStub stub(endpoint_);
  auto call = stub.CreateCall<SimpleServiceStub::SimpleCall>();
  call.request().set_foo("test_foo");
  base::WaitableEvent response_received_event;
  std::move(call).InvokeAsync(
      base::BindLambdaForTesting([&](GrpcStatusOr<TestResponse> response) {
        ASSERT_THAT(response, StatusIs(grpc::StatusCode::UNAVAILABLE));
        response_received_event.Signal();
      }));
  ASSERT_TRUE(response_received_event.TimedWait(kEventTimeout));

  // Need to wait for server to fully stop.
  ASSERT_TRUE(server_stopped_event.TimedWait(kEventTimeout));
}

TEST_F(GrpcUnaryTest, SyncUnaryCallSucceedsExtra) {
  GrpcServer server;
  server.SetHandler<SimpleServiceExtraHandler::SimpleCall>(
      base::BindLambdaForTesting(
          [](TestExtraRequest request,
             SimpleServiceExtraHandler::SimpleCall::Reactor* reactor) {
            EXPECT_EQ(request.extra(), "test_extra");
            TestResponse response;
            response.set_bar("test_bar");
            reactor->Write(std::move(response));
          }));
  ASSERT_THAT(server.Start(endpoint_), StatusIs(grpc::StatusCode::OK));

  SimpleServiceExtraStub stub(endpoint_);
  auto call = stub.CreateCall<SimpleServiceExtraStub::SimpleCall>();
  call.request().set_extra("test_extra");
  auto response = std::move(call).Invoke();
  CU_ASSERT_OK(response);
  EXPECT_EQ(response->bar(), "test_bar");

  test::StopGrpcServer(server, kServerStopTimeout);
}

// Cancelling a streaming call from the client side results in a race condition
// between two threads, one deletes the ClientContext while the other releases
// the mutex in that ClientContext. Problem is not always manifested as it's a
// timing issue between scheduled threads in the EventManager. Hence, the tsan
// config is disabled for this test.
#ifndef THREAD_SANITIZER

// TODO(b/259123902): Enable as gRPC framework is synced.
TEST_F(GrpcUnaryTest, DISABLED_AsyncUnaryCallCancelledByClient) {
  GrpcServer server;
  base::WaitableEvent request_received_event;
  server.SetHandler<SimpleServiceHandler::SimpleCall>(
      base::BindLambdaForTesting(
          [&](TestRequest request,
              SimpleServiceHandler::SimpleCall::Reactor* reactor) {
            EXPECT_EQ(request.foo(), "test_foo");
            request_received_event.Signal();
          }));
  ASSERT_THAT(server.Start(endpoint_), StatusIs(grpc::StatusCode::OK));

  SimpleServiceStub stub(endpoint_);
  auto call = stub.CreateCall<SimpleServiceStub::SimpleCall>();
  call.request().set_foo("test_foo");
  base::WaitableEvent response_received_event;
  auto context = std::move(call).InvokeAsync(
      base::BindLambdaForTesting([&](GrpcStatusOr<TestResponse> response) {
        ASSERT_THAT(response, StatusIs(grpc::StatusCode::CANCELLED));
        response_received_event.Signal();
      }));
  ASSERT_TRUE(request_received_event.TimedWait(kEventTimeout));

  context.Cancel();
  ASSERT_TRUE(response_received_event.TimedWait(kEventTimeout));

  test::StopGrpcServer(server, kServerStopTimeout);
  task_environment_.RunUntilIdle();
}

#endif  // THREAD_SANITIZER

}  // namespace
}  // namespace utils
}  // namespace cast
