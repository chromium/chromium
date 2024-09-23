// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/wayland_protocol_logger.h"

#include <wayland-server-core.h>
#include <wayland-util.h>
#include <xdg-shell-client-protocol.h>
#include <xdg-shell-server-protocol.h>

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "components/exo/wayland/test/client_util.h"
#include "components/exo/wayland/test/resource_key.h"
#include "components/exo/wayland/test/server_util.h"
#include "components/exo/wayland/test/wayland_server_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace exo::wayland {
namespace {

class WaylandProtocolLoggerTest : public test::WaylandServerTest {
 public:
  WaylandProtocolLoggerTest() = default;
  WaylandProtocolLoggerTest(const WaylandProtocolLoggerTest&) = delete;
  WaylandProtocolLoggerTest& operator=(const WaylandProtocolLoggerTest&) =
      delete;
  ~WaylandProtocolLoggerTest() override = default;

  // test::WaylandServerTest:
  void SetUp() override {
    WaylandProtocolLogger::SetHandlerFuncForTesting(
        [](void* user_data, wl_protocol_logger_type type,
           const wl_protocol_logger_message* message) {
          messages_.push_back(
              WaylandProtocolLogger::FormatMessage(type, message));
        });
    test::WaylandServerTest::SetUp();

    // Ignore messages from this test case's setup phase.
    messages_.clear();
  }

  // test::WaylandServerTest:
  void TearDown() override {
    messages_.clear();
    test::WaylandServerTest::TearDown();
  }

  static std::vector<std::vector<std::string>> messages_;
};

std::vector<std::vector<std::string>> WaylandProtocolLoggerTest::messages_ = {};

class ClientData : public test::TestClient::CustomData {
 public:
  ClientData()
      : callback(nullptr, &wl_callback_destroy),
        data_source(nullptr, &wl_data_source_destroy),
        pointer(nullptr, &wl_pointer_destroy),
        surface(nullptr, &wl_surface_destroy),
        xdg_surface(nullptr, &xdg_surface_destroy),
        xdg_toplevel(nullptr, &xdg_toplevel_destroy) {}
  ~ClientData() override = default;

  std::unique_ptr<wl_callback, decltype(&wl_callback_destroy)> callback;
  std::unique_ptr<wl_data_source, decltype(&wl_data_source_destroy)>
      data_source;
  std::unique_ptr<wl_pointer, decltype(&wl_pointer_destroy)> pointer;
  std::unique_ptr<wl_surface, decltype(&wl_surface_destroy)> surface;
  std::unique_ptr<xdg_surface, decltype(&xdg_surface_destroy)> xdg_surface;
  std::unique_ptr<xdg_toplevel, decltype(&xdg_toplevel_destroy)> xdg_toplevel;
};

std::string ProxyId(void* proxy) {
  return base::NumberToString(
      wl_proxy_get_id(reinterpret_cast<wl_proxy*>(proxy)));
}

void AddState(wl_array* states, xdg_toplevel_state state) {
  xdg_toplevel_state* value = static_cast<xdg_toplevel_state*>(
      wl_array_add(states, sizeof(xdg_toplevel_state)));
  DCHECK(value);
  *value = state;
}

}  // namespace

TEST_F(WaylandProtocolLoggerTest, LogsBasicRequest) {
  std::string surface_id;
  std::string compositor_id;

  PostToClientAndWait([&](test::TestClient* client) {
    auto* data = client->set_data(std::make_unique<ClientData>());
    data->surface.reset(wl_compositor_create_surface(client->compositor()));
    compositor_id = ProxyId(client->compositor());
    surface_id = ProxyId(data->surface.get());
  });

  EXPECT_THAT(messages_,
              ::testing::Contains(::testing::ElementsAre(
                  base::StrCat({"Received request: wl_compositor@",
                                compositor_id, ".create_surface"}),
                  base::StrCat({"new id wl_surface@", surface_id}))));
}

TEST_F(WaylandProtocolLoggerTest, LogsBasicEvent) {
  std::string callback_id;

  EXPECT_THAT(messages_, ::testing::IsEmpty());

  PostToClientAndWait([&](test::TestClient* client) {
    auto* data = client->set_data(std::make_unique<ClientData>());
    data->callback.reset(wl_display_sync(client->display()));
    callback_id = ProxyId(data->callback.get());
  });

  EXPECT_THAT(messages_,
              ::testing::Contains(::testing::Contains(base::StrCat(
                  {"Sent event: wl_callback@", callback_id, ".done"}))));
}

TEST_F(WaylandProtocolLoggerTest, LogsObjects) {
  std::string xdg_wm_base_id;
  std::string xdg_surface_id;
  std::string surface_id;

  PostToClientAndWait([&](test::TestClient* client) {
    auto* data = client->set_data(std::make_unique<ClientData>());
    xdg_wm_base_id = ProxyId(client->xdg_wm_base());
    data->surface.reset(wl_compositor_create_surface(client->compositor()));
    data->xdg_surface.reset(xdg_wm_base_get_xdg_surface(client->xdg_wm_base(),
                                                        data->surface.get()));
    surface_id = ProxyId(data->surface.get());
    xdg_surface_id = ProxyId(data->xdg_surface.get());
  });

  EXPECT_THAT(messages_,
              ::testing::Contains(::testing::ElementsAre(
                  base::StrCat({"Received request: xdg_wm_base@",
                                xdg_wm_base_id, ".get_xdg_surface"}),
                  base::StrCat({"new id xdg_surface@", xdg_surface_id}),
                  base::StrCat({"wl_surface@", surface_id}))));
}

TEST_F(WaylandProtocolLoggerTest, LogsStrings) {
  constexpr const char* window_title = "🦄🌈 I ♥️ UTF-8 😋";
  std::string xdg_toplevel_id;

  PostToClientAndWait([&](test::TestClient* client) {
    auto* data = client->set_data(std::make_unique<ClientData>());
    data->surface.reset(wl_compositor_create_surface(client->compositor()));
    data->xdg_surface.reset(xdg_wm_base_get_xdg_surface(client->xdg_wm_base(),
                                                        data->surface.get()));
    data->xdg_toplevel.reset(xdg_surface_get_toplevel(data->xdg_surface.get()));
    xdg_toplevel_set_title(data->xdg_toplevel.get(), window_title);
    xdg_toplevel_id = ProxyId(data->xdg_toplevel.get());
  });

  EXPECT_THAT(messages_, ::testing::Contains(::testing::ElementsAre(
                             base::StrCat({"Received request: xdg_toplevel@",
                                           xdg_toplevel_id, ".set_title"}),
                             window_title)));
}

// TODO(b/335767378): triggers dangling pointer detection.
TEST_F(WaylandProtocolLoggerTest, DISABLED_LogsObjectsAndStrings) {
  std::string display_id;
  constexpr uint32_t fake_error_code = 0x12345678u;
  constexpr const char* fake_error = "🦄🌈 Fake error 😋";
  EXPECT_THAT(messages_, ::testing::IsEmpty());

  PostToClientAndWait([&](test::TestClient* client) {
    wl_resource_post_error(test::server_util::LookUpResource(
                               server_.get(), test::client_util::GetResourceKey(
                                                  client->display())),
                           fake_error_code, "%s", fake_error);
    display_id = ProxyId(client->display());
  });

  EXPECT_THAT(
      messages_,
      ::testing::Contains(::testing::ElementsAre(
          base::StrCat({"Sent event: wl_display@", display_id, ".error"}),
          base::StrCat({"wl_display@", display_id}),
          base::NumberToString(fake_error_code), fake_error)));
}

TEST_F(WaylandProtocolLoggerTest, LogsFileDescriptors) {
  std::string data_source_id;
  test::ResourceKey data_source_resource_key;

  PostToClientAndWait([&](test::TestClient* client) {
    auto* data = client->set_data(std::make_unique<ClientData>());
    data->data_source.reset(wl_data_device_manager_create_data_source(
        client->data_device_manager()));
    data_source_id = ProxyId(data->data_source.get());
    data_source_resource_key =
        test::client_util::GetResourceKey(data->data_source.get());
  });

  wl_resource* data_source_resource = test::server_util::LookUpResource(
      server_.get(), data_source_resource_key);
  EXPECT_NE(data_source_resource, nullptr);

  // Actually sending a wl_data_source::send event requires a valid file
  // descriptor, which requires some ceremony to set up properly.
  // Instead, call the message formatting utility directly.
  const struct wl_interface* arg_types[2] = {nullptr, nullptr};
  wl_message message = {
      .name = "send", .signature = "sh", .types = &arg_types[0]};
  wl_argument args[] = {{.s = "text/plain"}, {.h = 12345}};
  wl_protocol_logger_message logger_message = {
      .resource = data_source_resource,
      .message_opcode = WL_DATA_SOURCE_SEND,
      .message = &message,
      .arguments_count = 2,
      .arguments = &args[0],
  };

  auto result = WaylandProtocolLogger::FormatMessage(WL_PROTOCOL_LOGGER_EVENT,
                                                     &logger_message);
  EXPECT_THAT(result, ::testing::ElementsAre(
                          base::StrCat({"Sent event: wl_data_source@",
                                        data_source_id, ".send"}),
                          "text/plain", "fd 12345"));
}

TEST_F(WaylandProtocolLoggerTest, LogsFixedPointNumbers) {
  // Arrange: Create a wl_pointer client object, wait for server to receive.
  std::string pointer_id;
  test::ResourceKey pointer_resource_key;

  PostToClientAndWait([&](test::TestClient* client) {
    auto* data = client->set_data(std::make_unique<ClientData>());
    data->pointer.reset(wl_seat_get_pointer(client->seat()));
    pointer_id = ProxyId(data->pointer.get());
    pointer_resource_key =
        test::client_util::GetResourceKey(data->pointer.get());
  });

  // Act: Send a motion event
  wl_resource* pointer_resource =
      test::server_util::LookUpResource(server_.get(), pointer_resource_key);
  EXPECT_NE(pointer_resource, nullptr);

  wl_pointer_send_motion(pointer_resource, 123, wl_fixed_from_double(1234.5),
                         wl_fixed_from_double(0));

  // Assert: Motion event correctly logged
  EXPECT_THAT(
      messages_,
      ::testing::Contains(::testing::ElementsAre(
          base::StrCat({"Sent event: wl_pointer@", pointer_id, ".motion"}),
          "123", "1234.5", "0")));
}

TEST_F(WaylandProtocolLoggerTest, LogsArrays) {
  std::string xdg_toplevel_id;
  test::ResourceKey xdg_toplevel_resource_key;

  PostToClientAndWait([&](test::TestClient* client) {
    auto* data = client->set_data(std::make_unique<ClientData>());
    data->surface.reset(wl_compositor_create_surface(client->compositor()));
    data->xdg_surface.reset(xdg_wm_base_get_xdg_surface(client->xdg_wm_base(),
                                                        data->surface.get()));
    data->xdg_toplevel.reset(xdg_surface_get_toplevel(data->xdg_surface.get()));
    xdg_toplevel_id = ProxyId(data->xdg_toplevel.get());
    xdg_toplevel_resource_key =
        test::client_util::GetResourceKey(data->xdg_toplevel.get());
  });

  wl_resource* xdg_toplevel_resource = test::server_util::LookUpResource(
      server_.get(), xdg_toplevel_resource_key);
  EXPECT_NE(xdg_toplevel_resource, nullptr);

  // Act: Send a typical configure event.
  {
    messages_.clear();
    wl_array states;
    wl_array_init(&states);
    AddState(&states, XDG_TOPLEVEL_STATE_ACTIVATED);
    AddState(&states, XDG_TOPLEVEL_STATE_FULLSCREEN);
    xdg_toplevel_send_configure(xdg_toplevel_resource, 1024, 768, &states);
    wl_array_release(&states);
  }

  // Assert: Short arrays print correctly.
  EXPECT_THAT(messages_,
              ::testing::Contains(::testing::ElementsAre(
                  base::StrCat({"Sent event: xdg_toplevel@", xdg_toplevel_id,
                                ".configure"}),
                  "1024", "768", "array[8 bytes]{0400000002000000}")));

  // Act: Send a very long array.
  {
    messages_.clear();
    wl_array states;
    wl_array_init(&states);
    for (int i = 0; i < 14; i++) {
      AddState(&states, XDG_TOPLEVEL_STATE_ACTIVATED);
    }
    xdg_toplevel_send_configure(xdg_toplevel_resource, 1024, 768, &states);
    wl_array_release(&states);
  }

  // Assert: Long arrays truncate as expected.
  EXPECT_THAT(messages_,
              ::testing::Contains(::testing::ElementsAre(
                  base::StrCat({"Sent event: xdg_toplevel@", xdg_toplevel_id,
                                ".configure"}),
                  "1024", "768",
                  "array[56 bytes]"
                  "{04000000040000000400000004000000040000000400000004000000"
                  "0400000004000000040000000400000004000000...}")));
}

}  // namespace exo::wayland
