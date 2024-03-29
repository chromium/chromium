// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chromecast/cast_core/grpc/grpc_server.h"

#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/uuid.h"
#include "chromecast/cast_core/grpc/status_matchers.h"
#include "chromecast/cast_core/grpc/test_service.castcore.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cast {
namespace utils {
namespace {
using ::cast::test::StatusIs;

class GrpcServerTest : public ::testing::Test {
 protected:
  GrpcServerTest() = default;

  const std::string endpoint_ =
      "unix-abstract:cast-uds-" +
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(GrpcServerTest, FailedStartReturnsError) {
  GrpcServer server1;
  server1.SetHandler<SimpleServiceHandler::SimpleCall>(
      base::BindLambdaForTesting(
          [](TestRequest, SimpleServiceHandler::SimpleCall::Reactor* reactor) {
            reactor->Write(TestResponse());
          }));
  ASSERT_THAT(server1.Start(endpoint_), StatusIs(grpc::StatusCode::OK));

  // Verify server has started.
  SimpleServiceStub stub(endpoint_);
  auto call = stub.CreateCall<SimpleServiceStub::SimpleCall>();
  auto response_or = std::move(call).Invoke();
  CU_ASSERT_OK(response_or);

  // Verify that 2nd server on the same endpoint cannot be created.
  GrpcServer server2;
  server2.SetHandler<SimpleServiceHandler::SimpleCall>(
      base::BindLambdaForTesting(
          [](TestRequest, SimpleServiceHandler::SimpleCall::Reactor*) {}));
  EXPECT_THAT(server2.Start(endpoint_), StatusIs(grpc::StatusCode::INTERNAL));

  server1.Stop();
}

}  // namespace

}  // namespace utils
}  // namespace cast
