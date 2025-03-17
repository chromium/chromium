// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chromecast/cast_core/grpc/grpc_server.h"

#include <cstdlib>
#include <string>
#include <string_view>
#include <utility>

#if BUILDFLAG(IS_LINUX)
#include <linux/un.h>
#elif BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS)
#include <sys/un.h>
#else
#error "Unexpected target OS"
#endif  // BUILDFLAG(IS_LINUX)

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
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
using ::testing::StartsWith;

class GrpcServerTest : public ::testing::Test {
 protected:
  GrpcServerTest() { EXPECT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  GrpcServer StartServer(std::string_view endpoint) {
    GrpcServer server;
    server.SetHandler<SimpleServiceHandler::SimpleCall>(
        base::BindLambdaForTesting(
            [](TestRequest,
               SimpleServiceHandler::SimpleCall::Reactor* reactor) {
              reactor->Write(TestResponse());
            }));
    EXPECT_THAT(server.Start(endpoint), StatusIs(grpc::StatusCode::OK));
    return server;
  }

  std::string GetUdsAbstractEndpoint() {
    return base::StrCat({
        "unix-abstract:cast-test.sock.",
        base::Uuid::GenerateRandomV4().AsLowercaseString(),
    });
  }

  std::string GetUdsPathEndpoint() {
    auto endpoint = base::StrCat({
        "unix:",
        temp_dir_.GetPath()
            .Append("cast-test.sock." +
                    base::Uuid::GenerateRandomV4().AsLowercaseString())
            .value(),
    });
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_ANDROID)
    if (endpoint.length() > UNIX_PATH_MAX) {
      endpoint = endpoint.substr(0, UNIX_PATH_MAX);
    }
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_ANDROID)
    return endpoint;
  }

  base::ScopedTempDir temp_dir_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(GrpcServerTest, FailsWithMalformedEndpoints) {
  {
    GrpcServer server;
    EXPECT_THAT(server.Start("192.168.0:1234"),
                StatusIs(grpc::StatusCode::INTERNAL,
                         "Failed to start gRPC server on 192.168.0:1234"));
  }
  {
    GrpcServer server;
    EXPECT_THAT(
        server.Start("localhost/1234578:123"),
        StatusIs(grpc::StatusCode::INTERNAL,
                 "Failed to start gRPC server on localhost/1234578:123"));
  }
  {
    GrpcServer server;
    EXPECT_THAT(server.Start("::]:123"),
                StatusIs(grpc::StatusCode::INTERNAL,
                         "Failed to start gRPC server on ::]:123"));
  }
}

TEST_F(GrpcServerTest, FailsWithNoPortSpecifiedForTcp) {
  {
    GrpcServer server;
    EXPECT_THAT(server.Start("localhost"),
                StatusIs(grpc::StatusCode::INTERNAL,
                         "TCP port must be specified: localhost"));
  }
  {
    GrpcServer server;
    EXPECT_THAT(
        server.Start("localhost:some_port"),
        StatusIs(grpc::StatusCode::INTERNAL,
                 "TCP port must be a valid number: localhost:some_port"));
  }
  {
    GrpcServer server;
    EXPECT_THAT(server.Start("[::]"),
                StatusIs(grpc::StatusCode::INTERNAL,
                         "TCP port must be specified: [::]"));
  }
  {
    GrpcServer server;
    EXPECT_THAT(server.Start("[::]:some_port"),
                StatusIs(grpc::StatusCode::INTERNAL,
                         "TCP port must be a valid number: [::]:some_port"));
  }
  {
    GrpcServer server;
    EXPECT_THAT(server.Start("2001:0db8:85a3:0000:0000:8a2e:0370:7334"),
                StatusIs(grpc::StatusCode::INTERNAL,
                         "TCP port must be specified: "
                         "2001:0db8:85a3:0000:0000:8a2e:0370:7334"));
  }
}

TEST_F(GrpcServerTest, StartsUdsServer) {
  auto endpoint = GetUdsPathEndpoint();
  GrpcServer server = StartServer(endpoint);
  ASSERT_TRUE(server.is_running());
  ASSERT_EQ(server.endpoint(), endpoint);

  server.Stop();
  ASSERT_FALSE(server.is_running());
}

TEST_F(GrpcServerTest, StartsUdsAbstractServer) {
  auto endpoint = GetUdsAbstractEndpoint();
  GrpcServer server = StartServer(endpoint);
  ASSERT_TRUE(server.is_running());
  ASSERT_EQ(server.endpoint(), endpoint);

  server.Stop();
  ASSERT_FALSE(server.is_running());
}

TEST_F(GrpcServerTest, FailedStartReturnsError) {
  const std::string uds_abstract_endpoint_ = GetUdsAbstractEndpoint();

  GrpcServer server1(uds_abstract_endpoint_);
  server1.SetHandler<SimpleServiceHandler::SimpleCall>(
      base::BindLambdaForTesting(
          [](TestRequest, SimpleServiceHandler::SimpleCall::Reactor* reactor) {
            reactor->Write(TestResponse());
          }));
  ASSERT_THAT(server1.Start(), StatusIs(grpc::StatusCode::OK));

  // Verify server has started.
  SimpleServiceStub stub(uds_abstract_endpoint_);
  auto call = stub.CreateCall<SimpleServiceStub::SimpleCall>();
  auto response_or = std::move(call).Invoke();
  CU_ASSERT_OK(response_or);

  // Verify that 2nd server on the same endpoint cannot be created.
  GrpcServer server2;
  server2.SetHandler<SimpleServiceHandler::SimpleCall>(
      base::BindLambdaForTesting(
          [](TestRequest, SimpleServiceHandler::SimpleCall::Reactor*) {}));
  ASSERT_THAT(server2.Start(uds_abstract_endpoint_),
              StatusIs(grpc::StatusCode::INTERNAL));
  ASSERT_FALSE(server2.is_running());

  server1.Stop();
  ASSERT_FALSE(server1.is_running());
}

TEST_F(GrpcServerTest, StartsIPv4Server) {
  GrpcServer server("127.0.0.1:0");
  server.SetHandler<SimpleServiceHandler::SimpleCall>(
      base::BindLambdaForTesting(
          [](TestRequest, SimpleServiceHandler::SimpleCall::Reactor* reactor) {
            reactor->Write(TestResponse());
          }));
  ASSERT_THAT(server.Start(), StatusIs(grpc::StatusCode::OK));
  ASSERT_THAT(server.endpoint(), StartsWith("127.0.0.1:"));

  int port;
  ASSERT_TRUE(base::StringToInt(
      server.endpoint().substr(server.endpoint().find(':') + 1), &port));
  EXPECT_GT(port, 0);

  server.Stop();
}

// This test might be flaky if the port is not available.
TEST_F(GrpcServerTest, StartsIPv4ServerWithFixedPort) {
  int port = std::rand() % 40000 + 20000;
  std::string endpoint = base::StringPrintf("127.0.0.1:%d", port);

  GrpcServer server;
  server.SetHandler<SimpleServiceHandler::SimpleCall>(
      base::BindLambdaForTesting(
          [](TestRequest, SimpleServiceHandler::SimpleCall::Reactor* reactor) {
            reactor->Write(TestResponse());
          }));

  if (server.Start(endpoint).ok()) {
    // This is a best effort to verify that the server is listening on the
    // specified port. The port may be taken on Forge, so we can't assert.
    ASSERT_EQ(server.endpoint(), endpoint);
    server.Stop();
  }
}

TEST_F(GrpcServerTest, StartsIPv6Server) {
  GrpcServer server;
  server.SetHandler<SimpleServiceHandler::SimpleCall>(
      base::BindLambdaForTesting(
          [](TestRequest, SimpleServiceHandler::SimpleCall::Reactor* reactor) {
            reactor->Write(TestResponse());
          }));
  ASSERT_THAT(server.Start("[::]:0"), StatusIs(grpc::StatusCode::OK));
  ASSERT_THAT(server.endpoint(), StartsWith("[::]"));

  int port;
  ASSERT_TRUE(base::StringToInt(
      server.endpoint().substr(server.endpoint().find("]:") + 2), &port));
  EXPECT_GT(port, 0);

  server.Stop();
}

// This test might be flaky if the port is not available.
TEST_F(GrpcServerTest, StartsIPv6ServerWithFixedPort) {
  int port = std::rand() % 40000 + 20000;
  std::string endpoint = base::StringPrintf("[::]:%d", port);

  GrpcServer server;
  server.SetHandler<SimpleServiceHandler::SimpleCall>(
      base::BindLambdaForTesting(
          [](TestRequest, SimpleServiceHandler::SimpleCall::Reactor* reactor) {
            reactor->Write(TestResponse());
          }));
  if (server.Start(endpoint).ok()) {
    // This is a best effort to verify that the server is listening on the
    // specified port. The port may be taken on Forge, so we can't assert.
    ASSERT_EQ(server.endpoint(), endpoint);
    server.Stop();
  }
}

}  // namespace

}  // namespace utils
}  // namespace cast
