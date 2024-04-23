// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_TEST_TEST_CLIENT_H_
#define COMPONENTS_EXO_WAYLAND_TEST_TEST_CLIENT_H_

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/threading/thread_checker.h"
#include "components/exo/wayland/clients/globals.h"
#include "components/exo/wayland/test/shm_buffer_factory.h"

namespace exo::wayland::test {

// Wayland client used by WaylandServerTest.
// You can also derive from this class to extend the client if needed. Please
// also see WaylandServerTest::InitOnClientThread().
//
// Thread affinity: Lives exclusively on the client thread.
class TestClient {
 public:
  TestClient();

  TestClient(const TestClient&) = delete;
  TestClient& operator=(const TestClient&) = delete;

  virtual ~TestClient();

  // Initializes connection to the server side and globals.
  bool Init(const std::string& wayland_socket,
            base::flat_map<std::string, uint32_t> global_versions = {});

  void Roundtrip() { wl_display_roundtrip(display()); }

  wl_display* display() {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    return display_.get();
  }

  const clients::Globals& globals() const {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    return globals_;
  }

  clients::Globals& globals() {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    return globals_;
  }

  // Convenient getters of globals.
  wl_output* output() { return globals().outputs.back().get(); }
  wl_compositor* compositor() { return globals().compositor.get(); }
  wl_shm* shm() { return globals().shm.get(); }
  wp_presentation* presentation() { return globals().presentation.get(); }
  zwp_linux_dmabuf_v1* linux_dmabuf() { return globals().linux_dmabuf.get(); }
  wl_shell* shell() { return globals().shell.get(); }
  wl_seat* seat() { return globals().seat.get(); }
  wl_subcompositor* subcompositor() { return globals().subcompositor.get(); }
  zaura_shell* aura_shell() { return globals().aura_shell.get(); }
  zaura_output* aura_output() { return globals().aura_outputs.back().get(); }
  zaura_output_manager* aura_output_manager() {
    return globals().aura_output_manager.get();
  }
  zaura_output_manager_v2* aura_output_manager_v2() {
    return globals().aura_output_manager_v2.get();
  }
  xdg_wm_base* xdg_wm_base() { return globals().xdg_wm_base.get(); }
  zwp_fullscreen_shell_v1* fullscreen_shell() {
    return globals().fullscreen_shell.get();
  }
  zwp_input_timestamps_manager_v1* input_timestamps_manager() {
    return globals().input_timestamps_manager.get();
  }
  zwp_linux_explicit_synchronization_v1* linux_explicit_synchronization() {
    return globals().linux_explicit_synchronization.get();
  }
  zcr_vsync_feedback_v1* vsync_feedback() {
    return globals().vsync_feedback.get();
  }
  zcr_color_manager_v1* color_manager() {
    return globals().color_manager.get();
  }
  zcr_stylus_v2* stylus() { return globals().stylus.get(); }
  zcr_remote_shell_v1* cr_remote_shell_v1() {
    return globals().cr_remote_shell_v1.get();
  }
  zcr_remote_shell_v2* cr_remote_shell_v2() {
    return globals().cr_remote_shell_v2.get();
  }
  surface_augmenter* surface_augmenter() {
    return globals().surface_augmenter.get();
  }
  wl_data_device_manager* data_device_manager() {
    return globals().data_device_manager.get();
  }

  //////////////////////////////////////////////////////////////////////////////
  // Buffer creation support.

  // Initializes `shm_buffer_factory_`.
  bool InitShmBufferFactory(int32_t pool_size);

  // Must call InitShmBufferFactory() beforehand.
  ShmBufferFactory* shm_buffer_factory() { return shm_buffer_factory_.get(); }

  //////////////////////////////////////////////////////////////////////////////
  // CustomData supports attaching customized data to the client.
  class CustomData {
   public:
    CustomData() = default;

    CustomData(const CustomData&) = delete;
    CustomData& operator=(const CustomData&) = delete;

    virtual ~CustomData() = default;
  };

  template <std::derived_from<CustomData> T>
  T* set_data(std::unique_ptr<T> data) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    auto* r = data.get();
    data_ = std::move(data);
    return r;
  }

  template <class DataType>
  DataType* GetDataAs() {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    return static_cast<DataType*>(data_.get());
  }

  void DestroyData() { data_.reset(); }

 protected:
  THREAD_CHECKER(thread_checker_);

  std::unique_ptr<wl_display> display_;
  clients::Globals globals_;

  std::unique_ptr<ShmBufferFactory> shm_buffer_factory_;

  std::unique_ptr<CustomData> data_;
};

}  // namespace exo::wayland::test

#endif  // COMPONENTS_EXO_WAYLAND_TEST_TEST_CLIENT_H_
