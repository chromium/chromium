// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui_devtools/devtools_server.h"

#include "base/command_line.h"
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
#include "testing/gtest/include/gtest/gtest.h"

namespace ui_devtools {

// TODO(lgrey): Hopefully temporary while we figure out why this doesn't work.
#if defined(OS_MACOSX)
#define MAYBE_ConnectionToViewsServer DISABLED_ConnectionToViewsServer
#else
#define MAYBE_ConnectionToViewsServer ConnectionToViewsServer
#endif

// Tests whether the server for Views is properly created so we can connect to
// it.
TEST(UIDevToolsServerTest, MAYBE_ConnectionToViewsServer) {
  // Use port 80 to prevent firewall issues.
  static constexpr int fake_port = 80;
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableUiDevTools);
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::IO);

  mojo::Remote<network::mojom::NetworkService> network_service_remote;
  auto network_service = network::NetworkService::Create(
      network_service_remote.BindNewPipeAndPassReceiver());
  mojo::Remote<network::mojom::NetworkContext> network_context_remote;
  network_service_remote->CreateNetworkContext(
      network_context_remote.BindNewPipeAndPassReceiver(),
      network::mojom::NetworkContextParams::New());

  std::unique_ptr<UiDevToolsServer> server =
      UiDevToolsServer::CreateForViews(network_context_remote.get(), fake_port);
  // Connect to the server socket.
  net::AddressList addr(
      net::IPEndPoint(net::IPAddress(127, 0, 0, 1), fake_port));
  auto client_socket = std::make_unique<net::TCPClientSocket>(
      addr, nullptr, nullptr, net::NetLogSource());
  net::TestCompletionCallback callback;
  int connect_result =
      callback.GetResult(client_socket->Connect(callback.callback()));
  ASSERT_EQ(net::OK, connect_result);
  ASSERT_TRUE(client_socket->IsConnected());
}

}  // namespace ui_devtools
