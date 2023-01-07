// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui_devtools/devtools_server.h"

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/ui_devtools/switches.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/address_list.h"
#include "net/base/completion_once_callback.h"
#include "net/base/test_completion_callback.h"
#include "net/socket/tcp_client_socket.h"
#include "services/network/network_context.h"
#include "services/network/network_service.h"
#include "services/network/public/cpp/server/http_server_request_info.h"
#include "services/network/test/fake_test_cert_verifier_params_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ui_devtools {

class UIDevToolsServerTest : public testing::Test {
 public:
  UIDevToolsServerTest() {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableUiDevTools);
  }
  ~UIDevToolsServerTest() override = default;

  void SetUp() override {
    network_service_ = network::NetworkService::Create(
        network_service_remote_.BindNewPipeAndPassReceiver());

    network::mojom::NetworkContextParamsPtr context_params =
        network::mojom::NetworkContextParams::New();
    // Use a dummy CertVerifier that always passes cert verification, since
    // these unittests don't need to test CertVerifier behavior.
    context_params->cert_verifier_params =
        network::FakeTestCertVerifierParamsFactory::GetCertVerifierParams();
    network_service_remote_->CreateNetworkContext(
        network_context_remote_.BindNewPipeAndPassReceiver(),
        std::move(context_params));

    base::RunLoop run_loop;
    server_ =
        UiDevToolsServer::CreateForViews(network_context_remote_.get(), 0);
    server_->SetOnSocketConnectedForTesting(run_loop.QuitClosure());
    run_loop.Run();
  }

 protected:
  UiDevToolsServer* server() { return server_.get(); }
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::MainThreadType::IO};
  std::unique_ptr<network::NetworkService> network_service_;
  mojo::Remote<network::mojom::NetworkService> network_service_remote_;
  mojo::Remote<network::mojom::NetworkContext> network_context_remote_;

 private:
  std::unique_ptr<UiDevToolsServer> server_;
};

// Tests whether the server for Views is properly created so we can connect to
// it.
TEST_F(UIDevToolsServerTest, ConnectionToViewsServer) {
  // Connect to the server socket.
  net::AddressList addr(
      net::IPEndPoint(net::IPAddress(127, 0, 0, 1), server()->port()));
  auto client_socket = std::make_unique<net::TCPClientSocket>(
      addr, nullptr, nullptr, nullptr, net::NetLogSource());
  net::TestCompletionCallback callback;
  int connect_result =
      callback.GetResult(client_socket->Connect(callback.callback()));
  ASSERT_EQ(net::OK, connect_result);
  ASSERT_TRUE(client_socket->IsConnected());
}

// Ensure we don't crash from OOB vector access when passed an incorrect
// client ID.
TEST_F(UIDevToolsServerTest, OutOfBoundsClientTest) {
  auto client =
      std::make_unique<UiDevToolsClient>("UiDevToolsClient", server());
  server()->AttachClient(std::move(client));
  // Cast for public `OnWebSocketRequest`.
  network::server::HttpServer::Delegate* devtools_server =
      static_cast<network::server::HttpServer::Delegate*>(server());
  network::server::HttpServerRequestInfo request;
  request.path = "/1";
  devtools_server->OnWebSocketRequest(0, request);
  request.path = "/42";
  devtools_server->OnWebSocketRequest(0, request);
  request.path = "/-5000";
  devtools_server->OnWebSocketRequest(0, request);
}

}  // namespace ui_devtools
