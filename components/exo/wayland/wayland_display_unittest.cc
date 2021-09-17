// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#include "components/exo/display.h"
#include "components/exo/test/exo_test_base.h"
#include "components/exo/wayland/server.h"
#include "components/exo/wayland/server_util.h"
#include "components/exo/wayland/wayland_display_observer.h"
#include "components/exo/wayland/wayland_display_output.h"
#include "components/exo/wayland/wl_output.h"

#include <wayland-client-core.h>
#include <wayland-client-protocol.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol-core.h>
#include <wayland-server.h>
#include <memory>
#include <thread>
#include "base/bind.h"
#include "base/files/scoped_temp_dir.h"
#include "base/threading/thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace exo {
namespace wayland {
namespace {

// Interface class for mocking.
class MockOutputInterface {
 public:
  virtual void OutputHandleGeometry(void* data,
                                    struct wl_output* wl_output,
                                    int32_t x,
                                    int32_t y,
                                    int32_t physical_width,
                                    int32_t physical_height,
                                    int32_t subpixel,
                                    const char* make,
                                    const char* model,
                                    int32_t transform) = 0;

  virtual void OutputHandleMode(void* data,
                                struct wl_output* wl_output,
                                uint32_t flags,
                                int32_t width,
                                int32_t height,
                                int32_t refresh) = 0;

  virtual void OutputHandleDone(void* data, wl_output* wl_output) = 0;
  virtual void OnRemoved() = 0;
};

class MockOutput : public MockOutputInterface {
 public:
  MOCK_METHOD(void,
              OutputHandleGeometry,
              (void* data,
               struct wl_output* wl_output,
               int32_t x,
               int32_t y,
               int32_t physical_width,
               int32_t physical_height,
               int32_t subpixel,
               const char* make,
               const char* model,
               int32_t transform),
              (override));

  MOCK_METHOD(void,
              OutputHandleMode,
              (void* data,
               struct wl_output* wl_output,
               uint32_t flags,
               int32_t width,
               int32_t height,
               int32_t refresh),
              (override));

  MOCK_METHOD(void,
              OutputHandleDone,
              (void* data, wl_output* wl_output),
              (override));
  MOCK_METHOD(void, OnRemoved, (), (override));
};

// A test output class that can be created from a wayland client to send
// requests and respond to wl_output events.
class TestWaylandOutput {
 public:
  TestWaylandOutput(wl_output* output, uint32_t id)
      : wl_output_(output), id_(id) {
    wl_output_set_user_data(wl_output_, this);

    static constexpr wl_output_listener output_listener = {
        &OutputHandleGeometry,
        &OutputHandleMode,
        &OutputHandleDone,
        &OutputHandleScale,
    };
    wl_output_add_listener(wl_output_, &output_listener, this);
  }

  ~TestWaylandOutput() { wl_output_set_user_data(wl_output_, nullptr); }

  static void OutputHandleGeometry(void* data,
                                   struct wl_output* wl_output,
                                   int32_t x,
                                   int32_t y,
                                   int32_t physical_width,
                                   int32_t physical_height,
                                   int32_t subpixel,
                                   const char* make,
                                   const char* model,
                                   int32_t transform) {
    if (TestWaylandOutput* test_output =
            static_cast<TestWaylandOutput*>(data)) {
      test_output->mock_output().OutputHandleGeometry(
          data, wl_output, x, y, physical_width, physical_height, subpixel,
          make, model, transform);
    }
  }

  static void OutputHandleMode(void* data,
                               struct wl_output* wl_output,
                               uint32_t flags,
                               int32_t width,
                               int32_t height,
                               int32_t refresh) {
    if (TestWaylandOutput* test_output =
            static_cast<TestWaylandOutput*>(data)) {
      test_output->mock_output().OutputHandleMode(data, wl_output, flags, width,
                                                  height, refresh);
    }
  }

  static void OutputHandleDone(void* data, wl_output* wl_output) {
    if (TestWaylandOutput* test_output =
            static_cast<TestWaylandOutput*>(data)) {
      test_output->mock_output().OutputHandleDone(data, wl_output);
    }
  }

  static void OutputHandleScale(void* data,
                                wl_output* wl_output,
                                int32_t factor) {}

  void OnRemoved() { mock_output_.OnRemoved(); }

  testing::NiceMock<MockOutput>& mock_output() { return mock_output_; }
  uint32_t id() const { return id_; }

 private:
  testing::NiceMock<MockOutput> mock_output_;
  wl_output* wl_output_;
  uint32_t id_;
};

// A simple wayland client for testing.
class TestClient {
 public:
  static void Global(void* data,
                     struct wl_registry* registry,
                     uint32_t name,
                     const char* interface,
                     uint32_t version) {
    TestClient* client = static_cast<TestClient*>(data);
    if (strcmp(interface, "wl_output") == 0) {
      wl_output* out = static_cast<wl_output*>(
          wl_registry_bind(registry, name, &wl_output_interface, 2));
      client->outputs_.push_back(
          std::make_unique<TestWaylandOutput>(out, name));
    }
  }

  static void GlobalRemove(void* data, wl_registry* registry, uint32_t name) {
    (static_cast<TestClient*>(data))->OnOutputRemoved(name);
  }

  void Connect(const char* socket, base::WaitableEvent* event) {
    display_ = wl_display_connect(socket);
    DCHECK(display_);
    static constexpr wl_registry_listener registry_listener = {&Global,
                                                               &GlobalRemove};
    wl_registry* reg = wl_display_get_registry(display_);
    wl_registry_add_listener(reg, &registry_listener, this);
    wl_display_roundtrip(display_);

    // Signal the client has roundtripped.
    event->Signal();
  }

  // Sets the call expectation for the display metrics events.
  void SetOutputMetricsExpectation(int output_index,
                                   gfx::Rect bounds,
                                   base::WaitableEvent* event) {
    using testing::_;
    TestWaylandOutput* output = outputs_[output_index].get();
    EXPECT_CALL(
        output->mock_output(),
        OutputHandleGeometry(_, _, bounds.x(), bounds.y(), bounds.width(),
                             bounds.height(), _, _, _, _));
    EXPECT_CALL(output->mock_output(),
                OutputHandleMode(_, _, _, bounds.width(), bounds.height(), _));
    EXPECT_CALL(output->mock_output(), OutputHandleDone(_, _));
    event->Signal();
    wl_display_dispatch(display_);
  }

  // Sets the call expectation for the output removal events.
  void SetRemovalExpectation(int output_index, base::WaitableEvent* event) {
    EXPECT_CALL(outputs_[output_index]->mock_output(), OnRemoved());
    event->Signal();
    wl_display_dispatch(display_);
  }

  void Dispatch(base::WaitableEvent* event) {
    wl_display_dispatch(display_);
    event->Signal();
  }

  void Disconnect(base::WaitableEvent* event) {
    wl_display_disconnect(display_);
    event->Signal();
  }

  void OnOutputRemoved(uint32_t id) {
    auto it = std::find_if(outputs_.begin(), outputs_.end(),
                           [&id](std::unique_ptr<TestWaylandOutput>& output) {
                             return output->id() == id;
                           });
    if (it != outputs_.end())
      (*it)->OnRemoved();
  }

  wl_display* display_;
  std::vector<std::unique_ptr<TestWaylandOutput>> outputs_;
};

// A test handler that would send display metrics to a wayland client.
class TestWaylandDisplayHandler : public WaylandDisplayHandler {
 public:
  TestWaylandDisplayHandler(WaylandDisplayOutput* output,
                            wl_resource* output_resource)
      : WaylandDisplayHandler(output, output_resource) {}

  bool SendDisplayMetrics(const display::Display& display,
                          uint32_t changed_metrics) override {
    if (!output_resource())
      return false;

    if (!(changed_metrics &
          (DISPLAY_METRIC_BOUNDS | DISPLAY_METRIC_DEVICE_SCALE_FACTOR |
           DISPLAY_METRIC_ROTATION))) {
      return false;
    }

    const gfx::Rect& bounds = display.bounds();
    wl_output_send_geometry(output_resource(), bounds.origin().x(),
                            bounds.origin().y(), bounds.width(),
                            bounds.height(), WL_OUTPUT_SUBPIXEL_UNKNOWN,
                            "unknown", "unknown", WL_OUTPUT_TRANSFORM_NORMAL);
    wl_output_send_mode(output_resource(),
                        WL_OUTPUT_MODE_CURRENT | WL_OUTPUT_MODE_PREFERRED,
                        bounds.width(), bounds.height(), 60000);
    return true;
  }
};

void output_release(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

const struct wl_output_interface output_implementation = {output_release};

void test_bind_output(wl_client* client,
                      void* data,
                      uint32_t version,
                      uint32_t output_id) {
  WaylandDisplayOutput* output = static_cast<WaylandDisplayOutput*>(data);
  wl_resource* resource =
      wl_resource_create(client, &wl_output_interface,
                         std::min(version, kWlOutputVersion), output_id);
  auto handler = std::make_unique<TestWaylandDisplayHandler>(output, resource);
  handler->Initialize();
  SetImplementation(resource, &output_implementation, std::move(handler));
}

// A test server subclassing exo::wayland::Server, used to verify the display
// metrics are sent when appropriate.
class TestServer : public Server {
 public:
  TestServer(Display* display) : Server(display) {}
  void OnDisplayAdded(const display::Display& new_display) override {
    auto output = std::make_unique<WaylandDisplayOutput>(new_display.id());
    // Use |test_bind_output| for testing. This would replaces
    // WaylandDisplayHandler with TestWaylandDisplayHandler on the server.
    output->set_global(wl_global_create(GetWaylandDisplay(),
                                        &wl_output_interface, kWlOutputVersion,
                                        output.get(), test_bind_output));
    AddWaylandOutput(new_display.id(), std::move(output));
  }
};

class WaylandDisplayTest : public test::ExoTestBase {
 protected:
  void SetUp() override {
    ASSERT_TRUE(xdg_temp_dir_.CreateUniqueTempDir());
    setenv("XDG_RUNTIME_DIR", xdg_temp_dir_.GetPath().MaybeAsASCII().c_str(),
           1 /* overwrite */);
    test::ExoTestBase::SetUp();
  }

  void ServerDispatches(base::WaitableEvent& event) {
    while (!event.IsSignaled()) {
      server_->Dispatch(base::TimeDelta::FromMilliseconds(20));
      server_->Flush();
    }
  }

  std::unique_ptr<Display> display_;
  std::unique_ptr<TestServer> server_;

 private:
  base::ScopedTempDir xdg_temp_dir_;
};

TEST_F(WaylandDisplayTest, SendDisplayMetrics) {
  // Set a display.
  UpdateDisplay("480x320");
  display_ = std::make_unique<Display>();
  server_ = std::make_unique<TestServer>(display_.get());
  server_->Initialize();

  const char* socket("wayland_test");
  DCHECK(server_->AddSocket(socket));

  // Run the wayland client on a thread.
  base::Thread thread("client");
  thread.Start();

  TestClient client;
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC);
  thread.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&TestClient::Connect, base::Unretained(&client),
                                socket, &event));

  ServerDispatches(event);

  gfx::Rect expected_bounds(0, 0, 480, 320);
  thread.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&TestClient::SetOutputMetricsExpectation,
                     base::Unretained(&client), 0, expected_bounds, &event));
  ServerDispatches(event);

  // Add a second display.
  UpdateDisplay("480x320,400x300");
  server_->Dispatch(base::TimeDelta::FromMilliseconds(20));
  server_->Flush();

  // Let the client create an output for the second display.
  thread.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&TestClient::Dispatch, base::Unretained(&client), &event));
  event.Wait();

  expected_bounds = gfx::Rect(480, 0, 400, 300);
  thread.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&TestClient::SetOutputMetricsExpectation,
                     base::Unretained(&client), 1, expected_bounds, &event));
  ServerDispatches(event);

  // Remove the second display.
  UpdateDisplay("480x320");

  // Set the expectation that the second output would be removed on the client.
  thread.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&TestClient::SetRemovalExpectation,
                                base::Unretained(&client), 1, &event));
  ServerDispatches(event);

  thread.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&TestClient::Disconnect,
                                base::Unretained(&client), &event));
  // Wait for the client to disconnect.
  event.Wait();

  server_.reset();
  display_.reset();
}

}  // namespace
}  // namespace wayland
}  // namespace exo
