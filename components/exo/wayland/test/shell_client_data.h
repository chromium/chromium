// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_TEST_SHELL_CLIENT_DATA_H_
#define COMPONENTS_EXO_WAYLAND_TEST_SHELL_CLIENT_DATA_H_

#include <wayland-client.h>
#include <xdg-shell-client-protocol.h>

#include <memory>

#include "base/memory/raw_ptr.h"
#include "components/exo/wayland/test/test_client.h"

struct wl_surface;
struct xdg_surface;
struct xdg_toplevel;
struct zaura_toplevel;
struct zcr_remote_surface_v2;
struct wl_data_device;
struct wl_data_source;

namespace gfx {
class Size;
class Rect;
class PointF;
}  // namespace gfx

namespace exo::wayland::test {
struct ResourceKey;
class TestBuffer;

class InputListener {
 public:
  InputListener() = default;
  virtual ~InputListener() = default;

  // Input events.
  virtual void OnEnter(uint32_t serial,
                       wl_surface* surface,
                       const gfx::PointF& point) {}
  virtual void OnLeave(uint32_t serial, wl_surface* surface) {}
  virtual void OnButtonPressed(uint32_t serial, uint32_t button) {}
  virtual void OnButtonReleased(uint32_t serial, uint32_t button) {}
  virtual void OnMotion(const gfx::PointF& point) {}

  // Touch events.
  virtual void OnTouchDown(uint32_t serial,
                           wl_surface* surface,
                           int32_t id,
                           const gfx::PointF& point) {}
  virtual void OnTouchUp(uint32_t serial, int32_t id) {}
};

// A custom shell object which can act as xdg toplevel or remote surface.
// TODO(oshima): Implement more key events complete and move to a separate file.
class ShellClientData : public test::TestClient::CustomData {
 public:
  explicit ShellClientData(test::TestClient* client);
  ~ShellClientData() override;

  void CreateXdgToplevel();

  void CreateRemoteSurface();

  void Pin();

  void UnsetSnap();

  // Common to both xdg toplevel and remote surface.
  void CreateAndAttachBuffer(const gfx::Size& size);
  void Commit();
  void DestroySurface();

  // zaura_shell methods.
  void RequestWindowBounds(const gfx::Rect& bounds, wl_output* target_output);

  void set_input_listener(std::unique_ptr<InputListener> input_listener) {
    input_listener_ = std::move(input_listener);
  }

  InputListener* input_listener() { return input_listener_.get(); }

  // Start Drag and Drop operation.
  void StartDrag(uint32_t serial);
  void DestroyDataSource();

  void Close();

  ResourceKey GetSurfaceResourceKey() const;

  bool close_called() const { return close_called_; }

  void set_data_offer(
      std::unique_ptr<wl_data_offer, decltype(&wl_data_offer_destroy)>
          data_offer) {
    data_offer_ = std::move(data_offer);
  }

 private:
  bool close_called_ = false;
  const raw_ptr<TestClient> client_;
  std::unique_ptr<wl_pointer, decltype(&wl_pointer_destroy)> pointer_;
  std::unique_ptr<wl_touch, decltype(&wl_touch_destroy)> touch_;
  std::unique_ptr<wl_surface, decltype(&wl_surface_destroy)> surface_;
  std::unique_ptr<xdg_surface, decltype(&xdg_surface_destroy)> xdg_surface_;
  std::unique_ptr<xdg_toplevel, decltype(&xdg_toplevel_destroy)> xdg_toplevel_;
  std::unique_ptr<zaura_toplevel, decltype(&zaura_toplevel_destroy)>
      aura_toplevel_;
  std::unique_ptr<zcr_remote_surface_v2,
                  decltype(&zcr_remote_surface_v2_destroy)>
      remote_surface_;
  std::unique_ptr<TestBuffer> buffer_;

  std::unique_ptr<wl_data_device, decltype(&wl_data_device_destroy)>
      data_device_;
  std::unique_ptr<wl_data_source, decltype(&wl_data_source_destroy)>
      data_source_;
  std::unique_ptr<wl_data_offer, decltype(&wl_data_offer_destroy)> data_offer_;

  std::unique_ptr<InputListener> input_listener_;
};

}  // namespace exo::wayland::test

#endif  // COMPONENTS_EXO_WAYLAND_TEST_SHELL_CLIENT_DATA_H_
