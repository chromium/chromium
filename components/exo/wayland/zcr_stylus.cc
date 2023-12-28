// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/zcr_stylus.h"

#include "base/memory/raw_ptr.h"
#include "components/exo/pointer.h"
#include "components/exo/pointer_stylus_delegate.h"
#include "components/exo/touch.h"
#include "components/exo/touch_stylus_delegate.h"
#include "components/exo/wayland/server_util.h"

namespace exo::wayland {
namespace {

////////////////////////////////////////////////////////////////////////////////
// zcr_touch_stylus_v2 interface:

class WaylandTouchStylusDelegate : public TouchStylusDelegate {
 public:
  WaylandTouchStylusDelegate(wl_resource* resource, Touch* touch)
      : resource_(resource), touch_(touch) {
    touch_->SetStylusDelegate(this);
  }

  WaylandTouchStylusDelegate(const WaylandTouchStylusDelegate&) = delete;
  WaylandTouchStylusDelegate& operator=(const WaylandTouchStylusDelegate&) =
      delete;

  ~WaylandTouchStylusDelegate() override {
    if (touch_ != nullptr)
      touch_->SetStylusDelegate(nullptr);
  }
  void OnTouchDestroying(Touch* touch) override { touch_ = nullptr; }
  void OnTouchTool(int touch_id, ui::EventPointerType type) override {
    uint wayland_type = ZCR_TOUCH_STYLUS_V2_TOOL_TYPE_TOUCH;
    if (type == ui::EventPointerType::kPen)
      wayland_type = ZCR_TOUCH_STYLUS_V2_TOOL_TYPE_PEN;
    else if (type == ui::EventPointerType::kEraser)
      wayland_type = ZCR_TOUCH_STYLUS_V2_TOOL_TYPE_ERASER;
    zcr_touch_stylus_v2_send_tool(resource_, touch_id, wayland_type);
  }
  void OnTouchForce(base::TimeTicks time_stamp,
                    int touch_id,
                    float force) override {
    zcr_touch_stylus_v2_send_force(resource_,
                                   TimeTicksToMilliseconds(time_stamp),
                                   touch_id, wl_fixed_from_double(force));
  }
  void OnTouchTilt(base::TimeTicks time_stamp,
                   int touch_id,
                   const gfx::Vector2dF& tilt) override {
    zcr_touch_stylus_v2_send_tilt(
        resource_, TimeTicksToMilliseconds(time_stamp), touch_id,
        wl_fixed_from_double(tilt.x()), wl_fixed_from_double(tilt.y()));
  }

 private:
  raw_ptr<wl_resource> resource_;
  raw_ptr<Touch> touch_;
};

void touch_stylus_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

const struct zcr_touch_stylus_v2_interface touch_stylus_implementation = {
    touch_stylus_destroy};

////////////////////////////////////////////////////////////////////////////////
// zcr_pointer_stylus_v2 interface:

class WaylandPointerStylusDelegate : public PointerStylusDelegate {
 public:
  WaylandPointerStylusDelegate(wl_resource* resource, Pointer* pointer)
      : resource_(resource), pointer_(pointer) {
    pointer_->SetStylusDelegate(this);
  }
  WaylandPointerStylusDelegate(const WaylandPointerStylusDelegate&) = delete;
  const WaylandPointerStylusDelegate& operator=(
      const WaylandPointerStylusDelegate&) = delete;
  ~WaylandPointerStylusDelegate() override {
    if (pointer_ != nullptr)
      pointer_->SetStylusDelegate(nullptr);
  }
  void OnPointerDestroying(Pointer* pointer) override { pointer_ = nullptr; }
  void OnPointerToolChange(ui::EventPointerType type) override {
    uint wayland_type = ZCR_POINTER_STYLUS_V2_TOOL_TYPE_NONE;
    if (type == ui::EventPointerType::kTouch)
      wayland_type = ZCR_POINTER_STYLUS_V2_TOOL_TYPE_TOUCH;
    else if (type == ui::EventPointerType::kPen)
      wayland_type = ZCR_POINTER_STYLUS_V2_TOOL_TYPE_PEN;
    else if (type == ui::EventPointerType::kEraser)
      wayland_type = ZCR_POINTER_STYLUS_V2_TOOL_TYPE_ERASER;
    zcr_pointer_stylus_v2_send_tool(resource_, wayland_type);
    supports_force_ = false;
    supports_tilt_ = false;
  }
  void OnPointerForce(base::TimeTicks time_stamp, float force) override {
    // Set the force as 0 if the current tool previously reported a valid
    // force, but is now reporting a NaN value indicating that force is not
    // supported.
    if (std::isnan(force)) {
      if (supports_force_)
        force = 0;
      else
        return;
    }
    supports_force_ = true;
    zcr_pointer_stylus_v2_send_force(resource_,
                                     TimeTicksToMilliseconds(time_stamp),
                                     wl_fixed_from_double(force));
  }
  void OnPointerTilt(base::TimeTicks time_stamp,
                     const gfx::Vector2dF& tilt) override {
    if (!supports_tilt_ && tilt.IsZero())
      return;
    supports_tilt_ = true;
    zcr_pointer_stylus_v2_send_tilt(
        resource_, TimeTicksToMilliseconds(time_stamp),
        wl_fixed_from_double(tilt.x()), wl_fixed_from_double(tilt.y()));
  }

 private:
  raw_ptr<wl_resource> resource_;
  raw_ptr<Pointer> pointer_;
  bool supports_force_ = false;
  bool supports_tilt_ = false;
};

void pointer_stylus_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

const struct zcr_pointer_stylus_v2_interface pointer_stylus_implementation = {
    pointer_stylus_destroy};

////////////////////////////////////////////////////////////////////////////////
// zcr_stylus_v2 interface:

void stylus_get_touch_stylus(wl_client* client,
                             wl_resource* resource,
                             uint32_t id,
                             wl_resource* touch_resource) {
  Touch* touch = GetUserDataAs<Touch>(touch_resource);
  if (touch->HasStylusDelegate()) {
    wl_resource_post_error(
        resource, ZCR_STYLUS_V2_ERROR_TOUCH_STYLUS_EXISTS,
        "touch has already been associated with a stylus object");
    return;
  }

  wl_resource* stylus_resource =
      wl_resource_create(client, &zcr_touch_stylus_v2_interface,
                         wl_resource_get_version(resource), id);

  SetImplementation(
      stylus_resource, &touch_stylus_implementation,
      std::make_unique<WaylandTouchStylusDelegate>(stylus_resource, touch));
}

void stylus_get_pointer_stylus(wl_client* client,
                               wl_resource* resource,
                               uint32_t id,
                               wl_resource* pointer_resource) {
  Pointer* pointer = GetUserDataAs<Pointer>(pointer_resource);
  if (pointer->HasStylusDelegate()) {
    wl_resource_post_error(
        resource, ZCR_STYLUS_V2_ERROR_POINTER_STYLUS_EXISTS,
        "pointer has already been associated with a stylus object");
    return;
  }

  wl_resource* stylus_resource =
      wl_resource_create(client, &zcr_pointer_stylus_v2_interface,
                         wl_resource_get_version(resource), id);

  SetImplementation(
      stylus_resource, &pointer_stylus_implementation,
      std::make_unique<WaylandPointerStylusDelegate>(stylus_resource, pointer));
}

const struct zcr_stylus_v2_interface stylus_v2_implementation = {
    stylus_get_touch_stylus, stylus_get_pointer_stylus};

}  // namespace

void bind_stylus_v2(wl_client* client,
                    void* data,
                    uint32_t version,
                    uint32_t id) {
  wl_resource* resource =
      wl_resource_create(client, &zcr_stylus_v2_interface, version, id);
  wl_resource_set_implementation(resource, &stylus_v2_implementation, data,
                                 nullptr);
}

}  // namespace exo::wayland
