// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/multiprocess_test.h"
#include "base/test/task_environment.h"
#include "chrome/enterprise_companion/app/app.h"
#include "chrome/enterprise_companion/ipc_support.h"
#include "chrome/enterprise_companion/test/test_utils.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/cpp/wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

namespace enterprise_companion {

class AppNetWorkerTest : public ::testing::Test {
 private:
  base::test::TaskEnvironment environment_;
  ScopedIPCSupportWrapper ipc_support_;
};

TEST_F(AppNetWorkerTest, ServicesNetworkRequests) {
  net::EmbeddedTestServer test_server;
  test_server.RegisterRequestHandler(base::BindRepeating(
      [](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        auto http_response =
            std::make_unique<net::test_server::BasicHttpResponse>();
        http_response->set_code(net::HTTP_OK);
        http_response->set_content("hello");
        http_response->set_content_type("text/plain");
        return http_response;
      }));
  ASSERT_TRUE(test_server.Start());

  mojo::PlatformChannel channel;
  base::LaunchOptions options;
  base::CommandLine command_line =
      base::GetMultiProcessTestChildBaseCommandLine();

  channel.PrepareToPassRemoteEndpoint(&options, &command_line);
  base::Process process =
      base::SpawnMultiProcessTestChild("NetWorkerChild", command_line, options);
  channel.RemoteProcessLaunchAttempted();
  ASSERT_TRUE(process.IsValid());

  mojo::ScopedMessagePipeHandle pipe = mojo::OutgoingInvitation::SendIsolated(
      channel.TakeLocalEndpoint(), {}, process.Handle());
  mojo::PendingRemote<network::mojom::URLLoaderFactory> pending_remote(
      std::move(pipe), network::mojom::URLLoaderFactory::Version_);
  ASSERT_TRUE(pending_remote);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
      base::MakeRefCounted<network::WrapperSharedURLLoaderFactory>(
          mojo::Remote(std::move(pending_remote)));

  std::unique_ptr<network::ResourceRequest> resource_request =
      std::make_unique<network::ResourceRequest>();
  resource_request->url = test_server.GetURL("/");
  resource_request->method = net::HttpRequestHeaders::kGetMethod;
  std::unique_ptr<network::SimpleURLLoader> url_loader =
      network::SimpleURLLoader::Create(
          std::move(resource_request),
          net::DefineNetworkTrafficAnnotation(
              "enterprise_companion_app_net_worker_unittest", R"()"));

  base::RunLoop run_loop;
  url_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory.get(),
      base::BindLambdaForTesting([&](std::optional<std::string> body) {
        EXPECT_EQ(url_loader->NetError(), net::OK);
        EXPECT_EQ(body, "hello");
      }).Then(run_loop.QuitClosure()));
  run_loop.Run();

  url_loader_factory.reset();
  WaitForProcess(process);
}

TEST_F(AppNetWorkerTest, StopsOnDisconnect) {
  mojo::PlatformChannel channel;
  base::LaunchOptions options;
  base::CommandLine command_line =
      base::GetMultiProcessTestChildBaseCommandLine();

  channel.PrepareToPassRemoteEndpoint(&options, &command_line);
  base::Process process =
      base::SpawnMultiProcessTestChild("NetWorkerChild", command_line, options);
  channel.RemoteProcessLaunchAttempted();
  ASSERT_TRUE(process.IsValid());

  mojo::ScopedMessagePipeHandle pipe = mojo::OutgoingInvitation::SendIsolated(
      channel.TakeLocalEndpoint(), {}, process.Handle());
  mojo::PendingRemote<network::mojom::URLLoaderFactory> pending_remote(
      std::move(pipe), network::mojom::URLLoaderFactory::Version_);
  ASSERT_TRUE(pending_remote);

  pending_remote.ResetWithReason(42, "Hanging up for test");
  EXPECT_EQ(WaitForProcess(process), 1);
}

MULTIPROCESS_TEST_MAIN(NetWorkerChild) {
  base::test::TaskEnvironment task_environment;
  ScopedIPCSupportWrapper ipc_support;

  return !CreateAppNetWorker()->Run().ok();
}

}  // namespace enterprise_companion
