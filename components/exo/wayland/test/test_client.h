// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_TEST_TEST_CLIENT_H_
#define COMPONENTS_EXO_WAYLAND_TEST_TEST_CLIENT_H_

#include <memory>

#include "base/threading/thread_checker.h"
#include "components/exo/wayland/clients/client_base.h"

namespace exo::wayland::test {

// Wayland client used by WaylandServerTest.
// You can also derive from this class to extend the client if needed. Please
// also see WaylandServerTest::CreateClient().
//
// Thread affinity: It is created on the main thread running WaylandServerTest,
// but used exclusively and destructed on the client thread.
class TestClient : public clients::ClientBase {
 public:
  TestClient();

  ~TestClient() override;

  TestClient(const TestClient&) = delete;
  TestClient& operator=(const TestClient&) = delete;

  wl_display* display() const {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    return display_.get();
  }

  const Globals& globals() const {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    return globals_;
  }

  // Convenient getters of globals.
  wl_output* output() const { return globals().output.get(); }
  wl_compositor* compositor() const { return globals().compositor.get(); }
  wl_shm* shm() const { return globals().shm.get(); }
  wp_presentation* presentation() const { return globals().presentation.get(); }
  zwp_linux_dmabuf_v1* linux_dmabuf() const {
    return globals().linux_dmabuf.get();
  }
  wl_shell* shell() const { return globals().shell.get(); }
  wl_seat* seat() const { return globals().seat.get(); }
  wl_subcompositor* subcompositor() const {
    return globals().subcompositor.get();
  }
  wl_touch* touch() const { return globals().touch.get(); }
  zaura_shell* aura_shell() const { return globals().aura_shell.get(); }
  zaura_output* aura_output() const { return globals().aura_output.get(); }
  zxdg_shell_v6* xdg_shell_v6() const { return globals().xdg_shell_v6.get(); }
  xdg_wm_base* xdg_wm_base() const { return globals().xdg_wm_base.get(); }
  zwp_fullscreen_shell_v1* fullscreen_shell() const {
    return globals().fullscreen_shell.get();
  }
  zwp_input_timestamps_manager_v1* input_timestamps_manager() const {
    return globals().input_timestamps_manager.get();
  }
  zwp_linux_explicit_synchronization_v1* linux_explicit_synchronization()
      const {
    return globals().linux_explicit_synchronization.get();
  }
  zcr_vsync_feedback_v1* vsync_feedback() const {
    return globals().vsync_feedback.get();
  }
  zcr_color_manager_v1* color_manager() const {
    return globals().color_manager.get();
  }
  zcr_stylus_v2* stylus() const { return globals().stylus.get(); }
  zcr_remote_shell_v1* cr_remote_shell_v1() const {
    return globals().cr_remote_shell_v1.get();
  }
  zcr_remote_shell_v2* cr_remote_shell_v2() const {
    return globals().cr_remote_shell_v2.get();
  }

  class CustomData {
   public:
    CustomData() = default;
    virtual ~CustomData() = default;

    CustomData(const CustomData&) = delete;
    CustomData& operator=(const CustomData&) = delete;
  };

  void set_data(std::unique_ptr<CustomData> data) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    data_ = std::move(data);
  }

  template <class DataType>
  DataType* GetDataAs() {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    return static_cast<DataType*>(data_.get());
  }

 protected:
  THREAD_CHECKER(thread_checker_);

  std::unique_ptr<CustomData> data_;
};

}  // namespace exo::wayland::test

#endif  // COMPONENTS_EXO_WAYLAND_TEST_TEST_CLIENT_H_
