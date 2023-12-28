// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/zwp_input_timestamps_manager.h"

#include <input-timestamps-unstable-v1-server-protocol.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol-core.h>

#include "base/memory/raw_ptr.h"
#include "components/exo/keyboard.h"
#include "components/exo/pointer.h"
#include "components/exo/touch.h"
#include "components/exo/wayland/server_util.h"
#include "components/exo/wayland/wayland_input_delegate.h"
#include "components/exo/wayland/wayland_keyboard_delegate.h"
#include "components/exo/wayland/wayland_pointer_delegate.h"
#include "components/exo/wayland/wayland_touch_delegate.h"

namespace exo {
namespace wayland {

namespace {

////////////////////////////////////////////////////////////////////////////////
// input_timestamps_v1 interface:

class WaylandInputTimestamps : public WaylandInputDelegate::Observer {
 public:
  WaylandInputTimestamps(wl_resource* resource, WaylandInputDelegate* delegate)
      : resource_(resource), delegate_(delegate) {
    delegate_->AddObserver(this);
  }

  WaylandInputTimestamps(const WaylandInputTimestamps&) = delete;
  WaylandInputTimestamps& operator=(const WaylandInputTimestamps&) = delete;

  ~WaylandInputTimestamps() override {
    if (delegate_)
      delegate_->RemoveObserver(this);
  }

  // Overridden from WaylandInputDelegate::Observer:
  void OnDelegateDestroying(WaylandInputDelegate* delegate) override {
    DCHECK(delegate_ == delegate);
    delegate_ = nullptr;
  }
  void OnSendTimestamp(base::TimeTicks time_stamp) override {
    timespec ts = (time_stamp - base::TimeTicks()).ToTimeSpec();

    zwp_input_timestamps_v1_send_timestamp(
        resource_, static_cast<uint64_t>(ts.tv_sec) >> 32,
        ts.tv_sec & 0xffffffff, ts.tv_nsec);
  }

 private:
  const raw_ptr<wl_resource> resource_;
  raw_ptr<WaylandInputDelegate> delegate_;
};

void input_timestamps_destroy(struct wl_client* client,
                              struct wl_resource* resource) {
  wl_resource_destroy(resource);
}

const struct zwp_input_timestamps_v1_interface input_timestamps_implementation =
    {input_timestamps_destroy};

////////////////////////////////////////////////////////////////////////////////
// input_timestamps_manager_v1 interface:

void input_timestamps_manager_destroy(struct wl_client* client,
                                      struct wl_resource* resource) {
  wl_resource_destroy(resource);
}

template <typename T, typename D>
void input_timestamps_manager_get_timestamps(wl_client* client,
                                             wl_resource* resource,
                                             uint32_t id,
                                             wl_resource* input_resource) {
  wl_resource* input_timestamps_resource =
      wl_resource_create(client, &zwp_input_timestamps_v1_interface, 1, id);

  auto input_timestamps = std::make_unique<WaylandInputTimestamps>(
      input_timestamps_resource,
      static_cast<WaylandInputDelegate*>(
          static_cast<D*>(GetUserDataAs<T>(input_resource)->delegate())));

  SetImplementation(input_timestamps_resource, &input_timestamps_implementation,
                    std::move(input_timestamps));
}

const struct zwp_input_timestamps_manager_v1_interface
    input_timestamps_manager_implementation = {
        input_timestamps_manager_destroy,
        input_timestamps_manager_get_timestamps<Keyboard,
                                                WaylandKeyboardDelegate>,
        input_timestamps_manager_get_timestamps<Pointer,
                                                WaylandPointerDelegate>,
        input_timestamps_manager_get_timestamps<Touch, WaylandTouchDelegate>};

}  // namespace

void bind_input_timestamps_manager(wl_client* client,
                                   void* data,
                                   uint32_t version,
                                   uint32_t id) {
  wl_resource* resource = wl_resource_create(
      client, &zwp_input_timestamps_manager_v1_interface, 1, id);

  wl_resource_set_implementation(
      resource, &input_timestamps_manager_implementation, nullptr, nullptr);
}

}  // namespace wayland
}  // namespace exo
