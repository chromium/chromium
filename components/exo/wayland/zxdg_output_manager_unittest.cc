// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <wayland-client-core.h>
#include <wayland-client-protocol.h>
#include <xdg-output-unstable-v1-client-protocol.h>

#include <string_view>

#include "base/memory/raw_ptr.h"
#include "components/exo/wayland/test/wayland_server_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace exo {
namespace wayland {
namespace {

// Regression / security test for a use-after-free in zxdg_output_manager.
//
// xdg_output_manager_get_xdg_output() attaches the WaylandDisplayHandler*
// to the new zxdg_output_v1 resource via wl_resource_set_implementation()
// BEFORE OnXdgOutputCreated() runs the duplicate-registration guard. If the
// guard fires (a second get_xdg_output for the same wl_output) it posts a
// protocol error and returns, but the second resource ("B") still carries
// a non-null WaylandDisplayHandler* in its user_data and a destructor that
// calls handler->UnsetXdgOutputResource().
//
// wl_resource_post_error() sets client->error, the dispatch loop tears the
// client down via wl_client_destroy(), which iterates resources by ascending
// object ID. The wl_output resource (lower ID) is destroyed first, deleting
// the WaylandDisplayHandler. xdg_output_A's user_data was nulled by the
// handler's destructor; xdg_output_B's was not. When xdg_output_B is then
// destroyed, xdg_output_destroy() dereferences the freed handler (UAF write).
//
// This test sends the exact malicious wire sequence a hostile guest-VM
// Wayland client (Crostini / Borealis / ARC++) would send. Under ASAN it
// produces a heap-use-after-free in the server (browser-process) thread.
using ZxdgOutputManagerSecurityTest = test::WaylandServerTest;

TEST_F(ZxdgOutputManagerSecurityTest, DuplicateGetXdgOutputDoesNotUAF) {
  struct ClientData : test::TestClient::CustomData {
    uint32_t xdg_output_manager_name = 0;
    raw_ptr<zxdg_output_manager_v1> mgr = nullptr;
    raw_ptr<zxdg_output_v1> xdg_out_a = nullptr;
    raw_ptr<zxdg_output_v1> xdg_out_b = nullptr;
  };

  // Phase 1: discover the zxdg_output_manager_v1 global. The default
  // TestClient::Init() already bound wl_output (lower object ID) along with
  // the rest of the standard globals during SetUp().
  PostToClientAndWait([&](test::TestClient* client) {
    auto data = std::make_unique<ClientData>();
    static const wl_registry_listener kListener = {
        .global =
            [](void* user_data, wl_registry*, uint32_t name,
               const char* interface, uint32_t) {
              auto* d = static_cast<ClientData*>(user_data);
              if (std::string_view(interface) ==
                  zxdg_output_manager_v1_interface.name) {
                d->xdg_output_manager_name = name;
              }
            },
        .global_remove = [](void*, wl_registry*, uint32_t) {},
    };
    wl_registry* reg = wl_display_get_registry(client->display());
    wl_registry_add_listener(reg, &kListener, data.get());
    wl_display_roundtrip(client->display());
    wl_registry_destroy(reg);
    ASSERT_NE(0u, data->xdg_output_manager_name);
    client->set_data(std::move(data));
  });

  // Phase 2: bind the manager and call get_xdg_output() twice for the SAME
  // wl_output. The second call makes the server post a protocol error and
  // tear down the client; during that teardown the server hits the UAF.
  PostToClientAndWait([&](test::TestClient* client) {
    auto* data = client->GetDataAs<ClientData>();
    ASSERT_FALSE(client->globals().outputs.empty());
    wl_output* out = client->globals().outputs.front().get();

    data->mgr = static_cast<zxdg_output_manager_v1*>(wl_registry_bind(
        client->globals().registry.get(), data->xdg_output_manager_name,
        &zxdg_output_manager_v1_interface, /*version=*/3));
    ASSERT_TRUE(data->mgr);

    // First get_xdg_output -> server records this as xdg_output_resource_.
    data->xdg_out_a =
        zxdg_output_manager_v1_get_xdg_output(data->mgr.get(), out);
    // Second get_xdg_output for the same wl_output -> server attaches the
    // handler to resource B, then OnXdgOutputCreated() posts an error and
    // returns without clearing B's user_data. wl_client_destroy() then
    // frees the handler (wl_output, lower ID) before destroying B (UAF).
    data->xdg_out_b =
        zxdg_output_manager_v1_get_xdg_output(data->mgr.get(), out);

    // Flush to the server and wait for it to process; the server will kill
    // the connection so the roundtrip returns an error, which we expect.
    wl_display_flush(client->display());
    wl_display_roundtrip(client->display());
    // The server posted a protocol error; the client should observe it.
    EXPECT_NE(0, wl_display_get_error(client->display()));

    // Explicitly destroy proxies to avoid leaks.
    // We use wl_proxy_destroy directly because the connection is likely dead
    // and we don't want to attempt to send requests to the server.
    // We extract the raw pointer and clear the raw_ptr first to avoid
    // DanglingPtr warnings when the memory is freed.
    if (data->mgr) {
      zxdg_output_manager_v1* raw = data->mgr.get();
      data->mgr = nullptr;
      wl_proxy_destroy(reinterpret_cast<wl_proxy*>(raw));
    }
    if (data->xdg_out_a) {
      zxdg_output_v1* raw = data->xdg_out_a.get();
      data->xdg_out_a = nullptr;
      wl_proxy_destroy(reinterpret_cast<wl_proxy*>(raw));
    }
    if (data->xdg_out_b) {
      zxdg_output_v1* raw = data->xdg_out_b.get();
      data->xdg_out_b = nullptr;
      wl_proxy_destroy(reinterpret_cast<wl_proxy*>(raw));
    }
  });

  // The server already destroyed the wl_client. Drop our reference so
  // TearDown() does not try to wait on a dead client.
  client_resource_ = nullptr;
}

}  // namespace
}  // namespace wayland
}  // namespace exo
