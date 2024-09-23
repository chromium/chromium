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
  base::WaitableEvent request_received_event{
      base::WaitableEvent::ResetPolicy::AUTOMATIC};
  SimpleServiceHandler::SimpleCall::Reactor* reactor;
  server.SetHandler<SimpleServiceHandler::SimpleCall>(
      base::BindLambdaForTesting(
          [&](TestRequest request,
              SimpleServiceHandler::SimpleCall::Reactor* r) {
            reactor = r;
            request_received_event.Signal();
          }));
  ASSERT_THAT(server.Start(endpoint_), StatusIs(grpc::StatusCode::OK));

  // Need to run the client request in a separate thread, so that the main test
  // thread can continue handling the case.
  base::ThreadPool::PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        SimpleServiceStub stub(endpoint_);
        auto call = stub.CreateCall<SimpleServiceStub::SimpleCall>();
        call.request().set_foo("test_foo");
        auto response = std::move(call).Invoke();
        CU_ASSERT_OK(response);
      }));
  ASSERT_TRUE(request_received_event.TimedWait(kEventTimeout));
  // Allow first request to pass.
  reactor->Write(TestResponse());

  test::StopGrpcServer(server, kServerStopTimeout);

  // Server should not be available any more.
  SimpleServiceStub stub(endpoint_);
  auto call = stub.CreateCall<SimpleServiceStub::SimpleCall>();
  call.request().set_foo("test_foo");
  auto response = std::move(call).Invoke();
  ASSERT_THAT(response, StatusIs(grpc::StatusCode::UNAVAILABLE));
  ASSERT_FALSE(request_received_event.IsSignaled());
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
  base::WaitableEvent request_received_event;
  server.SetHandler<SimpleServiceHandler::SimpleCall>(
      base::BindLambdaForTesting(
          [&](TestRequest request,
              SimpleServiceHandler::SimpleCall::Reactor* reactor) {
            reactor->Write(TestResponse());
            request_received_event.Signal();
          }));
  ASSERT_THAT(server.Start(endpoint_), StatusIs(grpc::StatusCode::OK));

  SimpleServiceStub stub(endpoint_);
  auto call = stub.CreateCall<SimpleServiceStub::SimpleCall>();
  call.request().set_foo("test_foo");
  base::WaitableEvent response_received_event{
      base::WaitableEvent::ResetPolicy::AUTOMATIC};
  std::move(call).InvokeAsync(
      base::BindLambdaForTesting([&](GrpcStatusOr<TestResponse> response) {
        CU_ASSERT_OK(response);
        response_received_event.Signal();
      }));
  ASSERT_TRUE(response_received_event.TimedWait(kEventTimeout));

  test::StopGrpcServer(server, kServerStopTimeout);

  auto call1 = stub.CreateCall<SimpleServiceStub::SimpleCall>();
  std::move(call1).InvokeAsync(
      base::BindLambdaForTesting([&](GrpcStatusOr<TestResponse> response) {
        ASSERT_THAT(response, StatusIs(grpc::StatusCode::UNAVAILABLE));
        response_received_event.Signal();
      }));
  ASSERT_TRUE(response_received_event.TimedWait(kEventTimeout));
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

TEST_F(GrpcUnaryTest,
       AsyncUnaryCallCancelledByClientAndDestroyedBeforeServerShutdown) {
  GrpcServer server;
  SimpleServiceHandler::SimpleCall::Reactor* cancelled_reactor;
  base::WaitableEvent request_received_event;
  server.SetHandler<SimpleServiceHandler::SimpleCall>(
      base::BindLambdaForTesting(
          [&](TestRequest, SimpleServiceHandler::SimpleCall::Reactor* r) {
            cancelled_reactor = r;
            request_received_event.Signal();
          }));
  ASSERT_THAT(server.Start(endpoint_), StatusIs(grpc::StatusCode::OK));

  SimpleServiceStub stub(endpoint_);
  auto call = stub.CreateCall<SimpleServiceStub::SimpleCall>();
  base::WaitableEvent response_received_event;
  auto context = std::move(call).InvokeAsync(
      base::BindLambdaForTesting([&](GrpcStatusOr<TestResponse> response) {
        ASSERT_THAT(response, StatusIs(grpc::StatusCode::CANCELLED));
        response_received_event.Signal();
      }));
  ASSERT_TRUE(request_received_event.TimedWait(kEventTimeout));

  context.Cancel();
  ASSERT_TRUE(response_received_event.TimedWait(kEventTimeout));

  // Actually sleep to allow gRPC framework to propagate the cancellation to the
  // server side and mark the reactor cancelled.
  ASSERT_TRUE(
      test::WaitForPredicate(kEventTimeout, base::BindLambdaForTesting([&]() {
                               return cancelled_reactor->is_done();
                             })));

  // This releases the cancelled reactor on the server side and destroys it.
  cancelled_reactor->Write(TestResponse());
  ASSERT_TRUE(
      test::WaitForPredicate(kEventTimeout, base::BindLambdaForTesting([&]() {
                               return server.active_reactor_count() == 0;
                             })));
  ASSERT_EQ(server.active_reactor_count(), 0u);

  test::StopGrpcServer(server, kServerStopTimeout);
}

TEST_F(GrpcUnaryTest,
       AsyncUnaryCallCancelledByClientAndLeftActiveDuringServerShutdown) {
  GrpcServer server;
  base::WaitableEvent request_received_event;
  server.SetHandler<SimpleServiceHandler::SimpleCall>(
      base::BindLambdaForTesting(
          [&](TestRequest, SimpleServiceHandler::SimpleCall::Reactor*) {
            request_received_event.Signal();
          }));
  ASSERT_THAT(server.Start(endpoint_), StatusIs(grpc::StatusCode::OK));

  SimpleServiceStub stub(endpoint_);
  auto call = stub.CreateCall<SimpleServiceStub::SimpleCall>();
  base::WaitableEvent response_received_event;
  auto context = std::move(call).InvokeAsync(
      base::BindLambdaForTesting([&](GrpcStatusOr<TestResponse> response) {
        ASSERT_THAT(response, StatusIs(grpc::StatusCode::CANCELLED));
        response_received_event.Signal();
      }));
  ASSERT_TRUE(request_received_event.TimedWait(kEventTimeout));

  context.Cancel();
  ASSERT_TRUE(response_received_event.TimedWait(kEventTimeout));

  CHECK_EQ(server.active_reactor_count(), 1u);

  // All active reactors will be destroyed after this call.
  test::StopGrpcServer(server, kServerStopTimeout);
}

}  // namespace
}  // namespace utils
}  // namespace cast
