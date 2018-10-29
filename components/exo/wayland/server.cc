// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/server.h"

#include <alpha-compositing-unstable-v1-server-protocol.h>
#include <aura-shell-server-protocol.h>
#include <cursor-shapes-unstable-v1-server-protocol.h>
#include <gaming-input-unstable-v1-server-protocol.h>
#include <gaming-input-unstable-v2-server-protocol.h>
#include <grp.h>
#include <input-timestamps-unstable-v1-server-protocol.h>
#include <keyboard-configuration-unstable-v1-server-protocol.h>
#include <keyboard-extension-unstable-v1-server-protocol.h>
#include <linux/input.h>
#include <notification-shell-unstable-v1-server-protocol.h>
#include <pointer-gestures-unstable-v1-server-protocol.h>
#include <presentation-time-server-protocol.h>
#include <remote-shell-unstable-v1-server-protocol.h>
#include <secure-output-unstable-v1-server-protocol.h>
#include <stddef.h>
#include <stdint.h>
#include <stylus-tools-unstable-v1-server-protocol.h>
#include <stylus-unstable-v2-server-protocol.h>
#include <text-input-unstable-v1-server-protocol.h>
#include <viewporter-server-protocol.h>
#include <vsync-feedback-unstable-v1-server-protocol.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol-core.h>
#include <xdg-shell-unstable-v6-server-protocol.h>

#include <algorithm>
#include <cstdlib>
#include <iterator>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/display/screen_orientation_controller.h"
#include "ash/ime/ime_controller.h"
#include "ash/public/cpp/caption_buttons/caption_button_types.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/public/interfaces/window_pin_type.mojom.h"
#include "ash/public/interfaces/window_state_type.mojom.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/wm/window_resizer.h"
#include "base/atomic_sequence_num.h"
#include "base/bind.h"
#include "base/cancelable_callback.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/free_deleter.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/exo/buffer.h"
#include "components/exo/client_controlled_shell_surface.h"
#include "components/exo/data_device.h"
#include "components/exo/data_device_delegate.h"
#include "components/exo/data_offer.h"
#include "components/exo/data_offer_delegate.h"
#include "components/exo/data_source.h"
#include "components/exo/data_source_delegate.h"
#include "components/exo/display.h"
#include "components/exo/gamepad_delegate.h"
#include "components/exo/gaming_seat.h"
#include "components/exo/gaming_seat_delegate.h"
#include "components/exo/input_method_surface.h"
#include "components/exo/keyboard.h"
#include "components/exo/keyboard_delegate.h"
#include "components/exo/keyboard_device_configuration_delegate.h"
#include "components/exo/keyboard_observer.h"
#include "components/exo/notification.h"
#include "components/exo/notification_surface.h"
#include "components/exo/notification_surface_manager.h"
#include "components/exo/pointer.h"
#include "components/exo/pointer_delegate.h"
#include "components/exo/pointer_gesture_pinch_delegate.h"
#include "components/exo/shared_memory.h"
#include "components/exo/shell_surface.h"
#include "components/exo/sub_surface.h"
#include "components/exo/surface.h"
#include "components/exo/text_input.h"
#include "components/exo/touch.h"
#include "components/exo/touch_delegate.h"
#include "components/exo/touch_stylus_delegate.h"
#include "components/exo/wm_helper.h"
#include "components/exo/xdg_shell_surface.h"
#include "services/viz/public/interfaces/compositing/compositor_frame_sink.mojom.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/base/class_property.h"
#include "ui/base/hit_test.h"
#include "ui/base/ui_features.h"
#include "ui/compositor/compositor_vsync_manager.h"
#include "ui/display/display_switches.h"
#include "ui/display/manager/display_util.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/display/screen.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/window_animations.h"
#include "ui/wm/public/activation_change_observer.h"

#if defined(USE_OZONE)
#include <drm_fourcc.h>
#include <linux-dmabuf-unstable-v1-server-protocol.h>
#if defined(OS_CHROMEOS)
#include "ui/events/ozone/layout/xkb/xkb_keyboard_layout_engine.h"
#endif
#endif

#if BUILDFLAG(USE_XKBCOMMON)
#include <xkbcommon/xkbcommon.h>
#include "ui/events/keycodes/scoped_xkb.h"  // nogncheck
#endif

DEFINE_UI_CLASS_PROPERTY_TYPE(wl_resource*);

namespace exo {
namespace wayland {
namespace switches {

// This flag can be used to emulate device scale factor for remote shell.
constexpr char kForceRemoteShellScale[] = "force-remote-shell-scale";

// This flag can be used to override the default wayland socket name. It is
// useful when another wayland server is already running and using the
// default name.
constexpr char kWaylandServerSocket[] = "wayland-server-socket";
}

namespace {

// We don't send configure immediately after tablet mode switch
// because layout can change due to orientation lock state or accelerometer.
const int kConfigureDelayAfterLayoutSwitchMs = 300;

// Default wayland socket name.
const base::FilePath::CharType kSocketName[] = FILE_PATH_LITERAL("wayland-0");

// Group used for wayland socket.
const char kWaylandSocketGroup[] = "wayland";

// Notification id and notifier id used for NotificationShell.
constexpr char kNotificationShellNotificationIdFormat[] =
    "exo-notification-shell.%d.%s";
constexpr char kNotificationShellNotifierId[] = "exo-notification-shell";

// Incremental id for notification shell instance.
base::AtomicSequenceNumber g_next_notification_shell_id;

template <class T>
T* GetUserDataAs(wl_resource* resource) {
  return static_cast<T*>(wl_resource_get_user_data(resource));
}

template <class T>
std::unique_ptr<T> TakeUserDataAs(wl_resource* resource) {
  std::unique_ptr<T> user_data = base::WrapUnique(GetUserDataAs<T>(resource));
  wl_resource_set_user_data(resource, nullptr);
  return user_data;
}

template <class T>
void DestroyUserData(wl_resource* resource) {
  TakeUserDataAs<T>(resource);
}

template <class T>
void SetImplementation(wl_resource* resource,
                       const void* implementation,
                       std::unique_ptr<T> user_data) {
  wl_resource_set_implementation(resource, implementation, user_data.release(),
                                 DestroyUserData<T>);
}

// Returns the scale factor to be used by remote shell clients.
double GetDefaultDeviceScaleFactor() {
  // A flag used by VM to emulate a device scale for a particular board.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kForceRemoteShellScale)) {
    std::string value =
        command_line->GetSwitchValueASCII(switches::kForceRemoteShellScale);
    double scale = 1.0;
    if (base::StringToDouble(value, &scale))
      return std::max(1.0, scale);
  }
  return WMHelper::GetInstance()->GetDefaultDeviceScaleFactor();
}

// Convert a timestamp to a time value that can be used when interfacing
// with wayland. Note that we cast a int64_t value to uint32_t which can
// potentially overflow.
uint32_t TimeTicksToMilliseconds(base::TimeTicks ticks) {
  return (ticks - base::TimeTicks()).InMilliseconds();
}

uint32_t NowInMilliseconds() {
  return TimeTicksToMilliseconds(base::TimeTicks::Now());
}

class WaylandInputDelegate {
 public:
  class Observer {
   public:
    virtual void OnDelegateDestroying(WaylandInputDelegate* delegate) = 0;
    virtual void OnSendTimestamp(base::TimeTicks time_stamp) = 0;

   protected:
    virtual ~Observer() = default;
  };

  void AddObserver(Observer* observer) { observers_.AddObserver(observer); }

  void RemoveObserver(Observer* observer) {
    observers_.RemoveObserver(observer);
  }

  void SendTimestamp(base::TimeTicks time_stamp) {
    for (auto& observer : observers_)
      observer.OnSendTimestamp(time_stamp);
  }

 protected:
  WaylandInputDelegate() = default;
  virtual ~WaylandInputDelegate() {
    for (auto& observer : observers_)
      observer.OnDelegateDestroying(this);
  }

 private:
  base::ObserverList<Observer>::Unchecked observers_;

  DISALLOW_COPY_AND_ASSIGN(WaylandInputDelegate);
};

uint32_t WaylandDataDeviceManagerDndAction(DndAction action) {
  switch (action) {
    case DndAction::kNone:
      return WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE;
    case DndAction::kCopy:
      return WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY;
    case DndAction::kMove:
      return WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE;
    case DndAction::kAsk:
      return WL_DATA_DEVICE_MANAGER_DND_ACTION_ASK;
  }
  NOTREACHED();
}

uint32_t WaylandDataDeviceManagerDndActions(
    const base::flat_set<DndAction>& dnd_actions) {
  uint32_t actions = 0;
  for (DndAction action : dnd_actions)
    actions |= WaylandDataDeviceManagerDndAction(action);
  return actions;
}

DndAction DataDeviceManagerDndAction(uint32_t value) {
  switch (value) {
    case WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE:
      return DndAction::kNone;
    case WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY:
      return DndAction::kCopy;
    case WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE:
      return DndAction::kMove;
    case WL_DATA_DEVICE_MANAGER_DND_ACTION_ASK:
      return DndAction::kAsk;
    default:
      NOTREACHED();
      return DndAction::kNone;
  }
}

base::flat_set<DndAction> DataDeviceManagerDndActions(uint32_t value) {
  base::flat_set<DndAction> actions;
  if (value & WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY)
    actions.insert(DndAction::kCopy);
  if (value & WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE)
    actions.insert(DndAction::kMove);
  if (value & WL_DATA_DEVICE_MANAGER_DND_ACTION_ASK)
    actions.insert(DndAction::kAsk);
  return actions;
}

uint32_t ResizeDirection(int component) {
  switch (component) {
    case HTCAPTION:
      return ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_NONE;
    case HTTOP:
      return ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_TOP;
    case HTTOPRIGHT:
      return ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_TOPRIGHT;
    case HTRIGHT:
      return ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_RIGHT;
    case HTBOTTOMRIGHT:
      return ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_BOTTOMRIGHT;
    case HTBOTTOM:
      return ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_BOTTOM;
    case HTBOTTOMLEFT:
      return ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_BOTTOMLEFT;
    case HTLEFT:
      return ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_LEFT;
    case HTTOPLEFT:
      return ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_TOPLEFT;
    default:
      LOG(ERROR) << "Unknown component:" << component;
      break;
  }
  NOTREACHED();
  return ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_NONE;
}

int Component(uint32_t direction) {
  switch (direction) {
    case ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_NONE:
      return HTNOWHERE;
    case ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_TOP:
      return HTTOP;
    case ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_TOPRIGHT:
      return HTTOPRIGHT;
    case ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_RIGHT:
      return HTRIGHT;
    case ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_BOTTOMRIGHT:
      return HTBOTTOMRIGHT;
    case ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_BOTTOM:
      return HTBOTTOM;
    case ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_BOTTOMLEFT:
      return HTBOTTOMLEFT;
    case ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_LEFT:
      return HTLEFT;
    case ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_TOPLEFT:
      return HTTOPLEFT;
    default:
      VLOG(2) << "Unknown direction:" << direction;
      break;
  }
  return HTNOWHERE;
}

uint32_t CaptionButtonMask(uint32_t mask) {
  uint32_t caption_button_icon_mask = 0;
  if (mask & ZCR_REMOTE_SURFACE_V1_FRAME_BUTTON_TYPE_BACK)
    caption_button_icon_mask |= 1 << ash::CAPTION_BUTTON_ICON_BACK;
  if (mask & ZCR_REMOTE_SURFACE_V1_FRAME_BUTTON_TYPE_MENU)
    caption_button_icon_mask |= 1 << ash::CAPTION_BUTTON_ICON_MENU;
  if (mask & ZCR_REMOTE_SURFACE_V1_FRAME_BUTTON_TYPE_MINIMIZE)
    caption_button_icon_mask |= 1 << ash::CAPTION_BUTTON_ICON_MINIMIZE;
  if (mask & ZCR_REMOTE_SURFACE_V1_FRAME_BUTTON_TYPE_MAXIMIZE_RESTORE)
    caption_button_icon_mask |= 1 << ash::CAPTION_BUTTON_ICON_MAXIMIZE_RESTORE;
  if (mask & ZCR_REMOTE_SURFACE_V1_FRAME_BUTTON_TYPE_CLOSE)
    caption_button_icon_mask |= 1 << ash::CAPTION_BUTTON_ICON_CLOSE;
  if (mask & ZCR_REMOTE_SURFACE_V1_FRAME_BUTTON_TYPE_ZOOM)
    caption_button_icon_mask |= 1 << ash::CAPTION_BUTTON_ICON_ZOOM;
  return caption_button_icon_mask;
}

// A property key containing the surface resource that is associated with
// window. If unset, no surface resource is associated with surface object.
DEFINE_UI_CLASS_PROPERTY_KEY(wl_resource*, kSurfaceResourceKey, nullptr);

// A property key containing a boolean set to true if a viewport is associated
// with with surface object.
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kSurfaceHasViewportKey, false);

// A property key containing a boolean set to true if a security object is
// associated with surface object.
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kSurfaceHasSecurityKey, false);

// A property key containing a boolean set to true if a blending object is
// associated with surface object.
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kSurfaceHasBlendingKey, false);

// A property key containing a boolean set to true if the stylus_tool
// object is associated with surface object.
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kSurfaceHasStylusToolKey, false);

// A property key containing the data offer resource that is associated with
// data offer object.
DEFINE_UI_CLASS_PROPERTY_KEY(wl_resource*, kDataOfferResourceKey, nullptr);

// A property key containing a boolean set to true if na aura surface object is
// associated with surface object.
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kSurfaceHasAuraSurfaceKey, false);

wl_resource* GetSurfaceResource(Surface* surface) {
  return surface->GetProperty(kSurfaceResourceKey);
}

wl_resource* GetDataOfferResource(const DataOffer* data_offer) {
  return data_offer->GetProperty(kDataOfferResourceKey);
}

////////////////////////////////////////////////////////////////////////////////
// wl_buffer_interface:

void buffer_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

const struct wl_buffer_interface buffer_implementation = {buffer_destroy};

void HandleBufferReleaseCallback(wl_resource* resource) {
  wl_buffer_send_release(resource);
  wl_client_flush(wl_resource_get_client(resource));
}

////////////////////////////////////////////////////////////////////////////////
// wl_surface_interface:

void surface_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void surface_attach(wl_client* client,
                    wl_resource* resource,
                    wl_resource* buffer,
                    int32_t x,
                    int32_t y) {
  // TODO(reveman): Implement buffer offset support.
  DLOG_IF(WARNING, x || y) << "Unsupported buffer offset: "
                           << gfx::Point(x, y).ToString();

  GetUserDataAs<Surface>(resource)
      ->Attach(buffer ? GetUserDataAs<Buffer>(buffer) : nullptr);
}

void surface_damage(wl_client* client,
                    wl_resource* resource,
                    int32_t x,
                    int32_t y,
                    int32_t width,
                    int32_t height) {
  GetUserDataAs<Surface>(resource)->Damage(gfx::Rect(x, y, width, height));
}

void HandleSurfaceFrameCallback(wl_resource* resource,
                                base::TimeTicks frame_time) {
  if (!frame_time.is_null()) {
    wl_callback_send_done(resource, TimeTicksToMilliseconds(frame_time));
    // TODO(reveman): Remove this potentially blocking flush and instead watch
    // the file descriptor to be ready for write without blocking.
    wl_client_flush(wl_resource_get_client(resource));
  }
  wl_resource_destroy(resource);
}

void surface_frame(wl_client* client,
                   wl_resource* resource,
                   uint32_t callback) {
  wl_resource* callback_resource =
      wl_resource_create(client, &wl_callback_interface, 1, callback);

  // base::Unretained is safe as the resource owns the callback.
  auto cancelable_callback =
      std::make_unique<base::CancelableCallback<void(base::TimeTicks)>>(
          base::Bind(&HandleSurfaceFrameCallback,
                     base::Unretained(callback_resource)));

  GetUserDataAs<Surface>(resource)
      ->RequestFrameCallback(cancelable_callback->callback());

  SetImplementation(callback_resource, nullptr, std::move(cancelable_callback));
}

void surface_set_opaque_region(wl_client* client,
                               wl_resource* resource,
                               wl_resource* region_resource) {
  SkRegion region = region_resource ? *GetUserDataAs<SkRegion>(region_resource)
                                    : SkRegion(SkIRect::MakeEmpty());
  GetUserDataAs<Surface>(resource)->SetOpaqueRegion(cc::Region(region));
}

void surface_set_input_region(wl_client* client,
                              wl_resource* resource,
                              wl_resource* region_resource) {
  Surface* surface = GetUserDataAs<Surface>(resource);
  if (region_resource) {
    surface->SetInputRegion(
        cc::Region(*GetUserDataAs<SkRegion>(region_resource)));
  } else
    surface->ResetInputRegion();
}

void surface_commit(wl_client* client, wl_resource* resource) {
  GetUserDataAs<Surface>(resource)->Commit();
}

void surface_set_buffer_transform(wl_client* client,
                                  wl_resource* resource,
                                  int32_t transform) {
  Transform buffer_transform;
  switch (transform) {
    case WL_OUTPUT_TRANSFORM_NORMAL:
      buffer_transform = Transform::NORMAL;
      break;
    case WL_OUTPUT_TRANSFORM_90:
      buffer_transform = Transform::ROTATE_90;
      break;
    case WL_OUTPUT_TRANSFORM_180:
      buffer_transform = Transform::ROTATE_180;
      break;
    case WL_OUTPUT_TRANSFORM_270:
      buffer_transform = Transform::ROTATE_270;
      break;
    case WL_OUTPUT_TRANSFORM_FLIPPED:
    case WL_OUTPUT_TRANSFORM_FLIPPED_90:
    case WL_OUTPUT_TRANSFORM_FLIPPED_180:
    case WL_OUTPUT_TRANSFORM_FLIPPED_270:
      NOTIMPLEMENTED();
      return;
    default:
      wl_resource_post_error(resource, WL_SURFACE_ERROR_INVALID_TRANSFORM,
                             "buffer transform must be one of the values from "
                             "the wl_output.transform enum ('%d' specified)",
                             transform);
      return;
  }

  GetUserDataAs<Surface>(resource)->SetBufferTransform(buffer_transform);
}

void surface_set_buffer_scale(wl_client* client,
                              wl_resource* resource,
                              int32_t scale) {
  if (scale < 1) {
    wl_resource_post_error(resource, WL_SURFACE_ERROR_INVALID_SCALE,
                           "buffer scale must be at least one "
                           "('%d' specified)",
                           scale);
    return;
  }

  GetUserDataAs<Surface>(resource)->SetBufferScale(scale);
}

const struct wl_surface_interface surface_implementation = {
    surface_destroy,
    surface_attach,
    surface_damage,
    surface_frame,
    surface_set_opaque_region,
    surface_set_input_region,
    surface_commit,
    surface_set_buffer_transform,
    surface_set_buffer_scale};

////////////////////////////////////////////////////////////////////////////////
// wl_region_interface:

void region_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void region_add(wl_client* client,
                wl_resource* resource,
                int32_t x,
                int32_t y,
                int32_t width,
                int32_t height) {
  GetUserDataAs<SkRegion>(resource)
      ->op(SkIRect::MakeXYWH(x, y, width, height), SkRegion::kUnion_Op);
}

static void region_subtract(wl_client* client,
                            wl_resource* resource,
                            int32_t x,
                            int32_t y,
                            int32_t width,
                            int32_t height) {
  GetUserDataAs<SkRegion>(resource)
      ->op(SkIRect::MakeXYWH(x, y, width, height), SkRegion::kDifference_Op);
}

const struct wl_region_interface region_implementation = {
    region_destroy, region_add, region_subtract};

////////////////////////////////////////////////////////////////////////////////
// wl_compositor_interface:

void compositor_create_surface(wl_client* client,
                               wl_resource* resource,
                               uint32_t id) {
  std::unique_ptr<Surface> surface =
      GetUserDataAs<Display>(resource)->CreateSurface();

  wl_resource* surface_resource = wl_resource_create(
      client, &wl_surface_interface, wl_resource_get_version(resource), id);

  // Set the surface resource property for type-checking downcast support.
  surface->SetProperty(kSurfaceResourceKey, surface_resource);

  SetImplementation(surface_resource, &surface_implementation,
                    std::move(surface));
}

void compositor_create_region(wl_client* client,
                              wl_resource* resource,
                              uint32_t id) {
  wl_resource* region_resource =
      wl_resource_create(client, &wl_region_interface, 1, id);

  SetImplementation(region_resource, &region_implementation,
                    base::WrapUnique(new SkRegion));
}

const struct wl_compositor_interface compositor_implementation = {
    compositor_create_surface, compositor_create_region};

const uint32_t compositor_version = 3;

void bind_compositor(wl_client* client,
                     void* data,
                     uint32_t version,
                     uint32_t id) {
  wl_resource* resource =
      wl_resource_create(client, &wl_compositor_interface,
                         std::min(version, compositor_version), id);

  wl_resource_set_implementation(resource, &compositor_implementation, data,
                                 nullptr);
}

////////////////////////////////////////////////////////////////////////////////
// wl_shm_pool_interface:

const struct shm_supported_format {
  uint32_t shm_format;
  gfx::BufferFormat buffer_format;
} shm_supported_formats[] = {
    {WL_SHM_FORMAT_XBGR8888, gfx::BufferFormat::RGBX_8888},
    {WL_SHM_FORMAT_ABGR8888, gfx::BufferFormat::RGBA_8888},
    {WL_SHM_FORMAT_XRGB8888, gfx::BufferFormat::BGRX_8888},
    {WL_SHM_FORMAT_ARGB8888, gfx::BufferFormat::BGRA_8888}};

void shm_pool_create_buffer(wl_client* client,
                            wl_resource* resource,
                            uint32_t id,
                            int32_t offset,
                            int32_t width,
                            int32_t height,
                            int32_t stride,
                            uint32_t format) {
  const auto* supported_format =
      std::find_if(shm_supported_formats,
                   shm_supported_formats + arraysize(shm_supported_formats),
                   [format](const shm_supported_format& supported_format) {
                     return supported_format.shm_format == format;
                   });
  if (supported_format ==
      (shm_supported_formats + arraysize(shm_supported_formats))) {
    wl_resource_post_error(resource, WL_SHM_ERROR_INVALID_FORMAT,
                           "invalid format 0x%x", format);
    return;
  }

  if (offset < 0) {
    wl_resource_post_error(resource, WL_SHM_ERROR_INVALID_FORMAT,
                           "invalid offset %d", offset);
    return;
  }

  std::unique_ptr<Buffer> buffer =
      GetUserDataAs<SharedMemory>(resource)->CreateBuffer(
          gfx::Size(width, height), supported_format->buffer_format, offset,
          stride);
  if (!buffer) {
    wl_resource_post_no_memory(resource);
    return;
  }

  wl_resource* buffer_resource =
      wl_resource_create(client, &wl_buffer_interface, 1, id);

  buffer->set_release_callback(base::Bind(&HandleBufferReleaseCallback,
                                          base::Unretained(buffer_resource)));

  SetImplementation(buffer_resource, &buffer_implementation, std::move(buffer));
}

void shm_pool_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void shm_pool_resize(wl_client* client, wl_resource* resource, int32_t size) {
  // Nothing to do here.
}

const struct wl_shm_pool_interface shm_pool_implementation = {
    shm_pool_create_buffer, shm_pool_destroy, shm_pool_resize};

////////////////////////////////////////////////////////////////////////////////
// wl_shm_interface:

void shm_create_pool(wl_client* client,
                     wl_resource* resource,
                     uint32_t id,
                     int fd,
                     int32_t size) {
  static const auto kMode =
      base::subtle::PlatformSharedMemoryRegion::Mode::kUnsafe;
  auto fd_pair = base::subtle::ScopedFDPair(base::ScopedFD(fd),
                                            base::ScopedFD() /* readonly_fd */);
  auto guid = base::UnguessableToken::Create();
  auto platform_shared_memory = base::subtle::PlatformSharedMemoryRegion::Take(
      std::move(fd_pair), kMode, size, guid);
  std::unique_ptr<SharedMemory> shared_memory =
      GetUserDataAs<Display>(resource)->CreateSharedMemory(
          base::UnsafeSharedMemoryRegion::Deserialize(
              std::move(platform_shared_memory)));
  if (!shared_memory) {
    wl_resource_post_no_memory(resource);
    return;
  }

  wl_resource* shm_pool_resource =
      wl_resource_create(client, &wl_shm_pool_interface, 1, id);

  SetImplementation(shm_pool_resource, &shm_pool_implementation,
                    std::move(shared_memory));
}

const struct wl_shm_interface shm_implementation = {shm_create_pool};

void bind_shm(wl_client* client, void* data, uint32_t version, uint32_t id) {
  wl_resource* resource = wl_resource_create(client, &wl_shm_interface, 1, id);

  wl_resource_set_implementation(resource, &shm_implementation, data, nullptr);

  for (const auto& supported_format : shm_supported_formats)
    wl_shm_send_format(resource, supported_format.shm_format);
}

#if defined(USE_OZONE)

////////////////////////////////////////////////////////////////////////////////
// linux_buffer_params_interface:

const struct dmabuf_supported_format {
  uint32_t dmabuf_format;
  gfx::BufferFormat buffer_format;
} dmabuf_supported_formats[] = {
    {DRM_FORMAT_RGB565, gfx::BufferFormat::BGR_565},
    {DRM_FORMAT_XBGR8888, gfx::BufferFormat::RGBX_8888},
    {DRM_FORMAT_ABGR8888, gfx::BufferFormat::RGBA_8888},
    {DRM_FORMAT_XRGB8888, gfx::BufferFormat::BGRX_8888},
    {DRM_FORMAT_ARGB8888, gfx::BufferFormat::BGRA_8888},
    {DRM_FORMAT_NV12, gfx::BufferFormat::YUV_420_BIPLANAR},
    {DRM_FORMAT_YVU420, gfx::BufferFormat::YVU_420}};

struct LinuxBufferParams {
  struct Plane {
    base::ScopedFD fd;
    uint32_t stride;
    uint32_t offset;
  };

  explicit LinuxBufferParams(Display* display) : display(display) {}

  Display* const display;
  std::map<uint32_t, Plane> planes;
};

void linux_buffer_params_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void linux_buffer_params_add(wl_client* client,
                             wl_resource* resource,
                             int32_t fd,
                             uint32_t plane_idx,
                             uint32_t offset,
                             uint32_t stride,
                             uint32_t modifier_hi,
                             uint32_t modifier_lo) {
  LinuxBufferParams* linux_buffer_params =
      GetUserDataAs<LinuxBufferParams>(resource);

  LinuxBufferParams::Plane plane{base::ScopedFD(fd), stride, offset};

  const auto& inserted = linux_buffer_params->planes.insert(
      std::pair<uint32_t, LinuxBufferParams::Plane>(plane_idx,
                                                    std::move(plane)));
  if (!inserted.second) {  // The plane was already there.
    wl_resource_post_error(resource, ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_PLANE_SET,
                           "plane already set");
  }
}

bool ValidateLinuxBufferParams(wl_resource* resource,
                               int32_t width,
                               int32_t height,
                               gfx::BufferFormat format,
                               uint32_t flags) {
  if (width <= 0 || height <= 0) {
    wl_resource_post_error(resource,
                           ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INVALID_DIMENSIONS,
                           "invalid width or height");
    return false;
  }

  if (flags & (ZWP_LINUX_BUFFER_PARAMS_V1_FLAGS_Y_INVERT |
               ZWP_LINUX_BUFFER_PARAMS_V1_FLAGS_INTERLACED)) {
    wl_resource_post_error(resource,
                           ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INCOMPLETE,
                           "flags not supported");
    return false;
  }

  LinuxBufferParams* linux_buffer_params =
      GetUserDataAs<LinuxBufferParams>(resource);
  size_t num_planes = gfx::NumberOfPlanesForBufferFormat(format);

  for (uint32_t i = 0; i < num_planes; ++i) {
    auto plane_it = linux_buffer_params->planes.find(i);
    if (plane_it == linux_buffer_params->planes.end()) {
      wl_resource_post_error(resource,
                             ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INCOMPLETE,
                             "missing a plane");
      return false;
    }
  }

  if (linux_buffer_params->planes.size() != num_planes) {
    wl_resource_post_error(resource, ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_PLANE_IDX,
                           "plane idx out of bounds");
    return false;
  }

  return true;
}

void linux_buffer_params_create(wl_client* client,
                                wl_resource* resource,
                                int32_t width,
                                int32_t height,
                                uint32_t format,
                                uint32_t flags) {
  const auto* supported_format = std::find_if(
      std::begin(dmabuf_supported_formats), std::end(dmabuf_supported_formats),
      [format](const dmabuf_supported_format& supported_format) {
        return supported_format.dmabuf_format == format;
      });
  if (supported_format == std::end(dmabuf_supported_formats)) {
    wl_resource_post_error(resource,
                           ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INVALID_FORMAT,
                           "format not supported");
    return;
  }

  if (!ValidateLinuxBufferParams(resource, width, height,
                                 supported_format->buffer_format, flags))
    return;

  LinuxBufferParams* linux_buffer_params =
      GetUserDataAs<LinuxBufferParams>(resource);

  size_t num_planes =
      gfx::NumberOfPlanesForBufferFormat(supported_format->buffer_format);

  std::vector<gfx::NativePixmapPlane> planes;
  std::vector<base::ScopedFD> fds;

  for (uint32_t i = 0; i < num_planes; ++i) {
    auto plane_it = linux_buffer_params->planes.find(i);
    LinuxBufferParams::Plane& plane = plane_it->second;
    planes.emplace_back(plane.stride, plane.offset, 0);
    fds.push_back(std::move(plane.fd));
  }

  std::unique_ptr<Buffer> buffer =
      linux_buffer_params->display->CreateLinuxDMABufBuffer(
          gfx::Size(width, height), supported_format->buffer_format, planes,
          std::move(fds));
  if (!buffer) {
    zwp_linux_buffer_params_v1_send_failed(resource);
    return;
  }

  wl_resource* buffer_resource =
      wl_resource_create(client, &wl_buffer_interface, 1, 0);

  buffer->set_release_callback(base::Bind(&HandleBufferReleaseCallback,
                                          base::Unretained(buffer_resource)));

  SetImplementation(buffer_resource, &buffer_implementation, std::move(buffer));

  zwp_linux_buffer_params_v1_send_created(resource, buffer_resource);
}

void linux_buffer_params_create_immed(wl_client* client,
                                      wl_resource* resource,
                                      uint32_t buffer_id,
                                      int32_t width,
                                      int32_t height,
                                      uint32_t format,
                                      uint32_t flags) {
  const auto* supported_format = std::find_if(
      std::begin(dmabuf_supported_formats), std::end(dmabuf_supported_formats),
      [format](const dmabuf_supported_format& supported_format) {
        return supported_format.dmabuf_format == format;
      });
  if (supported_format == std::end(dmabuf_supported_formats)) {
    wl_resource_post_error(resource,
                           ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INVALID_FORMAT,
                           "format not supported");
    return;
  }

  if (!ValidateLinuxBufferParams(resource, width, height,
                                 supported_format->buffer_format, flags))
    return;

  LinuxBufferParams* linux_buffer_params =
      GetUserDataAs<LinuxBufferParams>(resource);

  size_t num_planes =
      gfx::NumberOfPlanesForBufferFormat(supported_format->buffer_format);

  std::vector<gfx::NativePixmapPlane> planes;
  std::vector<base::ScopedFD> fds;

  for (uint32_t i = 0; i < num_planes; ++i) {
    auto plane_it = linux_buffer_params->planes.find(i);
    LinuxBufferParams::Plane& plane = plane_it->second;
    planes.emplace_back(plane.stride, plane.offset, 0);
    fds.push_back(std::move(plane.fd));
  }

  std::unique_ptr<Buffer> buffer =
      linux_buffer_params->display->CreateLinuxDMABufBuffer(
          gfx::Size(width, height), supported_format->buffer_format, planes,
          std::move(fds));
  if (!buffer) {
    // On import failure in case of a create_immed request, the protocol
    // allows us to raise a fatal error from zwp_linux_dmabuf_v1 version 2+.
    wl_resource_post_error(resource,
                           ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INVALID_WL_BUFFER,
                           "dmabuf import failed");
    return;
  }

  wl_resource* buffer_resource =
      wl_resource_create(client, &wl_buffer_interface, 1, buffer_id);

  buffer->set_release_callback(base::Bind(&HandleBufferReleaseCallback,
                                          base::Unretained(buffer_resource)));

  SetImplementation(buffer_resource, &buffer_implementation, std::move(buffer));
}

const struct zwp_linux_buffer_params_v1_interface
    linux_buffer_params_implementation = {
        linux_buffer_params_destroy, linux_buffer_params_add,
        linux_buffer_params_create, linux_buffer_params_create_immed};

////////////////////////////////////////////////////////////////////////////////
// linux_dmabuf_interface:

void linux_dmabuf_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void linux_dmabuf_create_params(wl_client* client,
                                wl_resource* resource,
                                uint32_t id) {
  std::unique_ptr<LinuxBufferParams> linux_buffer_params =
      std::make_unique<LinuxBufferParams>(GetUserDataAs<Display>(resource));

  wl_resource* linux_buffer_params_resource =
      wl_resource_create(client, &zwp_linux_buffer_params_v1_interface,
                         wl_resource_get_version(resource), id);

  SetImplementation(linux_buffer_params_resource,
                    &linux_buffer_params_implementation,
                    std::move(linux_buffer_params));
}

const struct zwp_linux_dmabuf_v1_interface linux_dmabuf_implementation = {
    linux_dmabuf_destroy, linux_dmabuf_create_params};

const uint32_t linux_dmabuf_version = 2;

void bind_linux_dmabuf(wl_client* client,
                       void* data,
                       uint32_t version,
                       uint32_t id) {
  wl_resource* resource =
      wl_resource_create(client, &zwp_linux_dmabuf_v1_interface,
                         std::min(version, linux_dmabuf_version), id);

  wl_resource_set_implementation(resource, &linux_dmabuf_implementation, data,
                                 nullptr);

  for (const auto& supported_format : dmabuf_supported_formats)
    zwp_linux_dmabuf_v1_send_format(resource, supported_format.dmabuf_format);
}

#endif

////////////////////////////////////////////////////////////////////////////////
// wl_subsurface_interface:

void subsurface_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void subsurface_set_position(wl_client* client,
                             wl_resource* resource,
                             int32_t x,
                             int32_t y) {
  GetUserDataAs<SubSurface>(resource)->SetPosition(gfx::Point(x, y));
}

void subsurface_place_above(wl_client* client,
                            wl_resource* resource,
                            wl_resource* reference_resource) {
  GetUserDataAs<SubSurface>(resource)
      ->PlaceAbove(GetUserDataAs<Surface>(reference_resource));
}

void subsurface_place_below(wl_client* client,
                            wl_resource* resource,
                            wl_resource* sibling_resource) {
  GetUserDataAs<SubSurface>(resource)
      ->PlaceBelow(GetUserDataAs<Surface>(sibling_resource));
}

void subsurface_set_sync(wl_client* client, wl_resource* resource) {
  GetUserDataAs<SubSurface>(resource)->SetCommitBehavior(true);
}

void subsurface_set_desync(wl_client* client, wl_resource* resource) {
  GetUserDataAs<SubSurface>(resource)->SetCommitBehavior(false);
}

const struct wl_subsurface_interface subsurface_implementation = {
    subsurface_destroy,     subsurface_set_position, subsurface_place_above,
    subsurface_place_below, subsurface_set_sync,     subsurface_set_desync};

////////////////////////////////////////////////////////////////////////////////
// wl_subcompositor_interface:

void subcompositor_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void subcompositor_get_subsurface(wl_client* client,
                                  wl_resource* resource,
                                  uint32_t id,
                                  wl_resource* surface,
                                  wl_resource* parent) {
  std::unique_ptr<SubSurface> subsurface =
      GetUserDataAs<Display>(resource)->CreateSubSurface(
          GetUserDataAs<Surface>(surface), GetUserDataAs<Surface>(parent));
  if (!subsurface) {
    wl_resource_post_error(resource, WL_SUBCOMPOSITOR_ERROR_BAD_SURFACE,
                           "invalid surface");
    return;
  }

  wl_resource* subsurface_resource =
      wl_resource_create(client, &wl_subsurface_interface, 1, id);

  SetImplementation(subsurface_resource, &subsurface_implementation,
                    std::move(subsurface));
}

const struct wl_subcompositor_interface subcompositor_implementation = {
    subcompositor_destroy, subcompositor_get_subsurface};

void bind_subcompositor(wl_client* client,
                        void* data,
                        uint32_t version,
                        uint32_t id) {
  wl_resource* resource =
      wl_resource_create(client, &wl_subcompositor_interface, 1, id);

  wl_resource_set_implementation(resource, &subcompositor_implementation, data,
                                 nullptr);
}

////////////////////////////////////////////////////////////////////////////////
// wl_shell_surface_interface:

void shell_surface_pong(wl_client* client,
                        wl_resource* resource,
                        uint32_t serial) {
  NOTIMPLEMENTED();
}

void shell_surface_move(wl_client* client,
                        wl_resource* resource,
                        wl_resource* seat_resource,
                        uint32_t serial) {
  GetUserDataAs<ShellSurface>(resource)->StartMove();
}

void shell_surface_resize(wl_client* client,
                          wl_resource* resource,
                          wl_resource* seat_resource,
                          uint32_t serial,
                          uint32_t edges) {
  NOTIMPLEMENTED();
}

void shell_surface_set_toplevel(wl_client* client, wl_resource* resource) {
  GetUserDataAs<ShellSurface>(resource)->SetEnabled(true);
}

void shell_surface_set_transient(wl_client* client,
                                 wl_resource* resource,
                                 wl_resource* parent_resource,
                                 int x,
                                 int y,
                                 uint32_t flags) {
  ShellSurface* shell_surface = GetUserDataAs<ShellSurface>(resource);
  if (shell_surface->enabled())
    return;

  if (flags & WL_SHELL_SURFACE_TRANSIENT_INACTIVE) {
    shell_surface->SetContainer(ash::kShellWindowId_SystemModalContainer);
    shell_surface->SetActivatable(false);
  }

  shell_surface->SetEnabled(true);
}

void shell_surface_set_fullscreen(wl_client* client,
                                  wl_resource* resource,
                                  uint32_t method,
                                  uint32_t framerate,
                                  wl_resource* output_resource) {
  ShellSurface* shell_surface = GetUserDataAs<ShellSurface>(resource);
  if (shell_surface->enabled())
    return;

  shell_surface->SetEnabled(true);
  shell_surface->SetFullscreen(true);
}

void shell_surface_set_popup(wl_client* client,
                             wl_resource* resource,
                             wl_resource* seat_resource,
                             uint32_t serial,
                             wl_resource* parent_resource,
                             int32_t x,
                             int32_t y,
                             uint32_t flags) {
  NOTIMPLEMENTED();
}

void shell_surface_set_maximized(wl_client* client,
                                 wl_resource* resource,
                                 wl_resource* output_resource) {
  ShellSurface* shell_surface = GetUserDataAs<ShellSurface>(resource);
  if (shell_surface->enabled())
    return;

  shell_surface->SetEnabled(true);
  shell_surface->Maximize();
}

void shell_surface_set_title(wl_client* client,
                             wl_resource* resource,
                             const char* title) {
  GetUserDataAs<ShellSurface>(resource)
      ->SetTitle(base::string16(base::UTF8ToUTF16(title)));
}

void shell_surface_set_class(wl_client* client,
                             wl_resource* resource,
                             const char* clazz) {
  GetUserDataAs<ShellSurface>(resource)->SetApplicationId(clazz);
}

const struct wl_shell_surface_interface shell_surface_implementation = {
    shell_surface_pong,          shell_surface_move,
    shell_surface_resize,        shell_surface_set_toplevel,
    shell_surface_set_transient, shell_surface_set_fullscreen,
    shell_surface_set_popup,     shell_surface_set_maximized,
    shell_surface_set_title,     shell_surface_set_class};

////////////////////////////////////////////////////////////////////////////////
// wl_shell_interface:

uint32_t HandleShellSurfaceConfigureCallback(
    wl_resource* resource,
    const gfx::Size& size,
    ash::mojom::WindowStateType state_type,
    bool resizing,
    bool activated,
    const gfx::Vector2d& origin_offset) {
  wl_shell_surface_send_configure(resource, WL_SHELL_SURFACE_RESIZE_NONE,
                                  size.width(), size.height());
  wl_client_flush(wl_resource_get_client(resource));
  return 0;
}

void shell_get_shell_surface(wl_client* client,
                             wl_resource* resource,
                             uint32_t id,
                             wl_resource* surface) {
  std::unique_ptr<ShellSurface> shell_surface =
      GetUserDataAs<Display>(resource)->CreateShellSurface(
          GetUserDataAs<Surface>(surface));
  if (!shell_surface) {
    wl_resource_post_error(resource, WL_SHELL_ERROR_ROLE,
                           "surface has already been assigned a role");
    return;
  }

  wl_resource* shell_surface_resource =
      wl_resource_create(client, &wl_shell_surface_interface, 1, id);

  // Shell surfaces are initially disabled and needs to be explicitly mapped
  // before they are enabled and can become visible.
  shell_surface->SetEnabled(false);

  shell_surface->set_configure_callback(
      base::Bind(&HandleShellSurfaceConfigureCallback,
                 base::Unretained(shell_surface_resource)));

  shell_surface->set_surface_destroyed_callback(base::BindOnce(
      &wl_resource_destroy, base::Unretained(shell_surface_resource)));

  SetImplementation(shell_surface_resource, &shell_surface_implementation,
                    std::move(shell_surface));
}

const struct wl_shell_interface shell_implementation = {
    shell_get_shell_surface};

void bind_shell(wl_client* client, void* data, uint32_t version, uint32_t id) {
  wl_resource* resource =
      wl_resource_create(client, &wl_shell_interface, 1, id);

  wl_resource_set_implementation(resource, &shell_implementation, data,
                                 nullptr);
}

////////////////////////////////////////////////////////////////////////////////
// wl_output_interface:

// Returns the transform that a compositor will apply to a surface to
// compensate for the rotation of an output device.
wl_output_transform OutputTransform(display::Display::Rotation rotation) {
  // Note: |rotation| describes the counter clockwise rotation that a
  // display's output is currently adjusted for, which is the inverse
  // of what we need to return.
  switch (rotation) {
    case display::Display::ROTATE_0:
      return WL_OUTPUT_TRANSFORM_NORMAL;
    case display::Display::ROTATE_90:
      return WL_OUTPUT_TRANSFORM_270;
    case display::Display::ROTATE_180:
      return WL_OUTPUT_TRANSFORM_180;
    case display::Display::ROTATE_270:
      return WL_OUTPUT_TRANSFORM_90;
  }
  NOTREACHED();
  return WL_OUTPUT_TRANSFORM_NORMAL;
}

class WaylandDisplayObserver : public display::DisplayObserver {
 public:
  class ScaleObserver : public base::SupportsWeakPtr<ScaleObserver> {
   public:
    ScaleObserver() {}

    virtual void OnDisplayScalesChanged(const display::Display& display) = 0;

   protected:
    virtual ~ScaleObserver() {}
  };

  WaylandDisplayObserver(int64_t id, wl_resource* output_resource)
      : id_(id), output_resource_(output_resource) {
    display::Screen::GetScreen()->AddObserver(this);
    SendDisplayMetrics();
  }
  ~WaylandDisplayObserver() override {
    display::Screen::GetScreen()->RemoveObserver(this);
  }

  void SetScaleObserver(base::WeakPtr<ScaleObserver> scale_observer) {
    scale_observer_ = scale_observer;
    SendDisplayMetrics();
  }

  bool HasScaleObserver() const { return !!scale_observer_; }

  // Overridden from display::DisplayObserver:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override {
    if (id_ != display.id())
      return;

    // There is no need to check DISPLAY_METRIC_PRIMARY because when primary
    // changes, bounds always changes. (new primary should have had non
    // 0,0 origin).
    // Only exception is when switching to newly connected primary with
    // the same bounds. This happens whenyou're in docked mode, suspend,
    // unplug the dislpay, then resume to the internal display which has
    // the same resolution. Since metrics does not change, there is no need
    // to notify clients.
    if (changed_metrics &
        (DISPLAY_METRIC_BOUNDS | DISPLAY_METRIC_DEVICE_SCALE_FACTOR |
         DISPLAY_METRIC_ROTATION)) {
      SendDisplayMetrics();
    }
  }

 private:
  void SendDisplayMetrics() {
    display::Display display;
    bool rv =
        display::Screen::GetScreen()->GetDisplayWithDisplayId(id_, &display);
    DCHECK(rv);

    const display::ManagedDisplayInfo& info =
        WMHelper::GetInstance()->GetDisplayInfo(display.id());

    const float kInchInMm = 25.4f;
    const char* kUnknown = "unknown";

    const std::string& make = info.manufacturer_id();
    const std::string& model = info.product_id();

    gfx::Rect bounds = info.bounds_in_native();
    wl_output_send_geometry(
        output_resource_, bounds.x(), bounds.y(),
        static_cast<int>(kInchInMm * bounds.width() / info.device_dpi()),
        static_cast<int>(kInchInMm * bounds.height() / info.device_dpi()),
        WL_OUTPUT_SUBPIXEL_UNKNOWN, make.empty() ? kUnknown : make.c_str(),
        model.empty() ? kUnknown : model.c_str(),
        OutputTransform(display.rotation()));

    if (wl_resource_get_version(output_resource_) >=
        WL_OUTPUT_SCALE_SINCE_VERSION) {
      wl_output_send_scale(output_resource_, display.device_scale_factor());
    }

    // TODO(reveman): Send real list of modes.
    wl_output_send_mode(
        output_resource_, WL_OUTPUT_MODE_CURRENT | WL_OUTPUT_MODE_PREFERRED,
        bounds.width(), bounds.height(), static_cast<int>(60000));

    if (HasScaleObserver())
      scale_observer_->OnDisplayScalesChanged(display);

    if (wl_resource_get_version(output_resource_) >=
        WL_OUTPUT_DONE_SINCE_VERSION) {
      wl_output_send_done(output_resource_);
    }

    wl_client_flush(wl_resource_get_client(output_resource_));
  }

  // The ID of the display being observed.
  const int64_t id_;

  // The output resource associated with the display.
  wl_resource* const output_resource_;

  base::WeakPtr<ScaleObserver> scale_observer_;

  DISALLOW_COPY_AND_ASSIGN(WaylandDisplayObserver);
};

const uint32_t output_version = 2;

void bind_output(wl_client* client, void* data, uint32_t version, uint32_t id) {
  Server::Output* output = static_cast<Server::Output*>(data);

  wl_resource* resource = wl_resource_create(
      client, &wl_output_interface, std::min(version, output_version), id);

  SetImplementation(
      resource, nullptr,
      std::make_unique<WaylandDisplayObserver>(output->id(), resource));
}

////////////////////////////////////////////////////////////////////////////////
// xdg_positioner_interface:

uint32_t InvertBitfield(uint32_t bitfield, uint32_t mask) {
  return (bitfield & ~mask) | ((bitfield & mask) ^ mask);
}

// TODO(oshima): propagate x/y flip state to children.
struct WaylandPositioner {
  static constexpr uint32_t kHorizontalAnchors =
      ZXDG_POSITIONER_V6_ANCHOR_LEFT | ZXDG_POSITIONER_V6_ANCHOR_RIGHT;
  static constexpr uint32_t kVerticalAnchors =
      ZXDG_POSITIONER_V6_ANCHOR_TOP | ZXDG_POSITIONER_V6_ANCHOR_BOTTOM;
  static constexpr uint32_t kHorizontalGravities =
      ZXDG_POSITIONER_V6_GRAVITY_LEFT | ZXDG_POSITIONER_V6_GRAVITY_RIGHT;
  static constexpr uint32_t kVerticalGravities =
      ZXDG_POSITIONER_V6_GRAVITY_TOP | ZXDG_POSITIONER_V6_GRAVITY_BOTTOM;

  static int CalculateX(const gfx::Size& size,
                        const gfx::Rect& anchor_rect,
                        uint32_t anchor,
                        uint32_t gravity,
                        int offset,
                        bool flipped) {
    if (flipped) {
      anchor = InvertBitfield(anchor, kHorizontalAnchors);
      gravity = InvertBitfield(gravity, kHorizontalGravities);
      offset = -offset;
    }

    int x = offset;
    if (anchor & ZXDG_POSITIONER_V6_ANCHOR_LEFT)
      x += anchor_rect.x();
    else if (anchor & ZXDG_POSITIONER_V6_ANCHOR_RIGHT)
      x += anchor_rect.right();
    else
      x += anchor_rect.CenterPoint().x();

    if (gravity & ZXDG_POSITIONER_V6_GRAVITY_LEFT)
      return x - size.width();
    if (gravity & ZXDG_POSITIONER_V6_GRAVITY_RIGHT)
      return x;
    return x - size.width() / 2;
  }

  static int CalculateY(const gfx::Size& size,
                        const gfx::Rect& anchor_rect,
                        uint32_t anchor,
                        uint32_t gravity,
                        int offset,
                        bool flipped) {
    if (flipped) {
      anchor = InvertBitfield(anchor, kVerticalAnchors);
      gravity = InvertBitfield(gravity, kVerticalGravities);
      offset = -offset;
    }

    int y = offset;
    if (anchor & ZXDG_POSITIONER_V6_ANCHOR_TOP)
      y += anchor_rect.y();
    else if (anchor & ZXDG_POSITIONER_V6_ANCHOR_BOTTOM)
      y += anchor_rect.bottom();
    else
      y += anchor_rect.CenterPoint().y();

    if (gravity & ZXDG_POSITIONER_V6_GRAVITY_TOP)
      return y - size.height();
    if (gravity & ZXDG_POSITIONER_V6_GRAVITY_BOTTOM)
      return y;
    return y - size.height() / 2;
  }

  // Calculate and return position from current state.
  gfx::Point CalculatePosition(const gfx::Rect& work_area) {
    // TODO(oshima): The size must be smaller than work area.

    gfx::Rect bounds(gfx::Point(CalculateX(size, anchor_rect, anchor, gravity,
                                           offset.x(), x_flipped),
                                CalculateY(size, anchor_rect, anchor, gravity,
                                           offset.y(), y_flipped)),
                     size);

    // Adjust x position if the bounds are not fully contained by the work area.
    if (work_area.x() > bounds.x() || work_area.right() < bounds.right()) {
      // Allow sliding horizontally if the surface is attached below
      // or above the parent surface.
      bool can_slide_x = (anchor & ZXDG_POSITIONER_V6_ANCHOR_BOTTOM &&
                          gravity & ZXDG_POSITIONER_V6_GRAVITY_BOTTOM) ||
                         (anchor & ZXDG_POSITIONER_V6_ANCHOR_TOP &&
                          gravity & ZXDG_POSITIONER_V6_GRAVITY_TOP);
      if (adjustment & ZXDG_POSITIONER_V6_CONSTRAINT_ADJUSTMENT_SLIDE_X &&
          can_slide_x) {
        if (bounds.x() < work_area.x())
          bounds.set_x(work_area.x());
        else if (bounds.right() > work_area.right())
          bounds.set_x(work_area.right() - size.width());
      } else if (adjustment & ZXDG_POSITIONER_V6_CONSTRAINT_ADJUSTMENT_FLIP_X) {
        x_flipped = !x_flipped;
        bounds.set_x(CalculateX(size, anchor_rect, anchor, gravity, offset.x(),
                                x_flipped));
      }
    }

    // Adjust y position if the bounds are not fully contained by the work area.
    if (work_area.y() > bounds.y() || work_area.bottom() < bounds.bottom()) {
      // Allow sliding vertically if the surface is attached left or
      // right of the parent surface.
      bool can_slide_y = (anchor & ZXDG_POSITIONER_V6_ANCHOR_LEFT &&
                          gravity & ZXDG_POSITIONER_V6_GRAVITY_LEFT) ||
                         (anchor & ZXDG_POSITIONER_V6_ANCHOR_RIGHT &&
                          gravity & ZXDG_POSITIONER_V6_GRAVITY_RIGHT);
      if (adjustment & ZXDG_POSITIONER_V6_CONSTRAINT_ADJUSTMENT_SLIDE_Y &&
          can_slide_y) {
        if (bounds.y() < work_area.y())
          bounds.set_y(work_area.y());
        else if (bounds.bottom() > work_area.bottom())
          bounds.set_y(work_area.bottom() - size.height());
      } else if (adjustment & ZXDG_POSITIONER_V6_CONSTRAINT_ADJUSTMENT_FLIP_Y) {
        y_flipped = !y_flipped;
        bounds.set_y(CalculateY(size, anchor_rect, anchor, gravity, offset.y(),
                                y_flipped));
      }
    }
    return bounds.origin();
  }

  gfx::Size size;
  gfx::Rect anchor_rect;
  uint32_t anchor = ZXDG_POSITIONER_V6_ANCHOR_NONE;
  uint32_t gravity = ZXDG_POSITIONER_V6_GRAVITY_NONE;
  uint32_t adjustment = ZXDG_POSITIONER_V6_CONSTRAINT_ADJUSTMENT_NONE;
  gfx::Vector2d offset;
  bool y_flipped = false;
  bool x_flipped = false;
};

void xdg_positioner_v6_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void xdg_positioner_v6_set_size(wl_client* client,
                                wl_resource* resource,
                                int32_t width,
                                int32_t height) {
  if (width < 1 || height < 1) {
    wl_resource_post_error(resource, ZXDG_POSITIONER_V6_ERROR_INVALID_INPUT,
                           "width and height must be positive and non-zero");
    return;
  }

  GetUserDataAs<WaylandPositioner>(resource)->size = gfx::Size(width, height);
}

void xdg_positioner_v6_set_anchor_rect(wl_client* client,
                                       wl_resource* resource,
                                       int32_t x,
                                       int32_t y,
                                       int32_t width,
                                       int32_t height) {
  if (width < 1 || height < 1) {
    wl_resource_post_error(resource, ZXDG_POSITIONER_V6_ERROR_INVALID_INPUT,
                           "width and height must be positive and non-zero");
    return;
  }

  GetUserDataAs<WaylandPositioner>(resource)->anchor_rect =
      gfx::Rect(x, y, width, height);
}

void xdg_positioner_v6_set_anchor(wl_client* client,
                                  wl_resource* resource,
                                  uint32_t anchor) {
  if (((anchor & ZXDG_POSITIONER_V6_ANCHOR_LEFT) &&
       (anchor & ZXDG_POSITIONER_V6_ANCHOR_RIGHT)) ||
      ((anchor & ZXDG_POSITIONER_V6_ANCHOR_TOP) &&
       (anchor & ZXDG_POSITIONER_V6_ANCHOR_BOTTOM))) {
    wl_resource_post_error(resource, ZXDG_POSITIONER_V6_ERROR_INVALID_INPUT,
                           "same-axis values are not allowed");
    return;
  }

  GetUserDataAs<WaylandPositioner>(resource)->anchor = anchor;
}

void xdg_positioner_v6_set_gravity(wl_client* client,
                                   wl_resource* resource,
                                   uint32_t gravity) {
  if (((gravity & ZXDG_POSITIONER_V6_GRAVITY_LEFT) &&
       (gravity & ZXDG_POSITIONER_V6_GRAVITY_RIGHT)) ||
      ((gravity & ZXDG_POSITIONER_V6_GRAVITY_TOP) &&
       (gravity & ZXDG_POSITIONER_V6_GRAVITY_BOTTOM))) {
    wl_resource_post_error(resource, ZXDG_POSITIONER_V6_ERROR_INVALID_INPUT,
                           "same-axis values are not allowed");
    return;
  }

  GetUserDataAs<WaylandPositioner>(resource)->gravity = gravity;
}

void xdg_positioner_v6_set_constraint_adjustment(wl_client* client,
                                                 wl_resource* resource,
                                                 uint32_t adjustment) {
  GetUserDataAs<WaylandPositioner>(resource)->adjustment = adjustment;
}

void xdg_positioner_v6_set_offset(wl_client* client,
                                  wl_resource* resource,
                                  int32_t x,
                                  int32_t y) {
  GetUserDataAs<WaylandPositioner>(resource)->offset = gfx::Vector2d(x, y);
}

const struct zxdg_positioner_v6_interface xdg_positioner_v6_implementation = {
    xdg_positioner_v6_destroy,
    xdg_positioner_v6_set_size,
    xdg_positioner_v6_set_anchor_rect,
    xdg_positioner_v6_set_anchor,
    xdg_positioner_v6_set_gravity,
    xdg_positioner_v6_set_constraint_adjustment,
    xdg_positioner_v6_set_offset};

////////////////////////////////////////////////////////////////////////////////
// xdg_toplevel_interface:

int XdgToplevelV6ResizeComponent(uint32_t edges) {
  switch (edges) {
    case ZXDG_TOPLEVEL_V6_RESIZE_EDGE_TOP:
      return HTTOP;
    case ZXDG_TOPLEVEL_V6_RESIZE_EDGE_BOTTOM:
      return HTBOTTOM;
    case ZXDG_TOPLEVEL_V6_RESIZE_EDGE_LEFT:
      return HTLEFT;
    case ZXDG_TOPLEVEL_V6_RESIZE_EDGE_TOP_LEFT:
      return HTTOPLEFT;
    case ZXDG_TOPLEVEL_V6_RESIZE_EDGE_BOTTOM_LEFT:
      return HTBOTTOMLEFT;
    case ZXDG_TOPLEVEL_V6_RESIZE_EDGE_RIGHT:
      return HTRIGHT;
    case ZXDG_TOPLEVEL_V6_RESIZE_EDGE_TOP_RIGHT:
      return HTTOPRIGHT;
    case ZXDG_TOPLEVEL_V6_RESIZE_EDGE_BOTTOM_RIGHT:
      return HTBOTTOMRIGHT;
    default:
      return HTBOTTOMRIGHT;
  }
}

using XdgSurfaceConfigureCallback =
    base::Callback<void(const gfx::Size& size,
                        ash::mojom::WindowStateType state_type,
                        bool resizing,
                        bool activated)>;

uint32_t HandleXdgSurfaceV6ConfigureCallback(
    wl_resource* resource,
    const XdgSurfaceConfigureCallback& callback,
    const gfx::Size& size,
    ash::mojom::WindowStateType state_type,
    bool resizing,
    bool activated,
    const gfx::Vector2d& origin_offset) {
  uint32_t serial = wl_display_next_serial(
      wl_client_get_display(wl_resource_get_client(resource)));
  callback.Run(size, state_type, resizing, activated);
  zxdg_surface_v6_send_configure(resource, serial);
  wl_client_flush(wl_resource_get_client(resource));
  return serial;
}

// Wrapper around shell surface that allows us to handle the case where the
// xdg surface resource is destroyed before the toplevel resource.
class WaylandToplevel : public aura::WindowObserver {
 public:
  WaylandToplevel(wl_resource* resource, wl_resource* surface_resource)
      : resource_(resource),
        shell_surface_(GetUserDataAs<XdgShellSurface>(surface_resource)),
        weak_ptr_factory_(this) {
    shell_surface_->host_window()->AddObserver(this);
    shell_surface_->set_close_callback(
        base::Bind(&WaylandToplevel::OnClose, weak_ptr_factory_.GetWeakPtr()));
    shell_surface_->set_configure_callback(
        base::Bind(&HandleXdgSurfaceV6ConfigureCallback, surface_resource,
                   base::Bind(&WaylandToplevel::OnConfigure,
                              weak_ptr_factory_.GetWeakPtr())));
  }
  ~WaylandToplevel() override {
    if (shell_surface_)
      shell_surface_->host_window()->RemoveObserver(this);
  }

  // Overridden from aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override {
    shell_surface_ = nullptr;
  }

  void SetParent(WaylandToplevel* parent) {
    if (!shell_surface_)
      return;

    if (!parent) {
      shell_surface_->SetParent(nullptr);
      return;
    }

    // This is a no-op if parent is not mapped.
    if (parent->shell_surface_ && parent->shell_surface_->GetWidget())
      shell_surface_->SetParent(parent->shell_surface_);
  }

  void SetTitle(const base::string16& title) {
    if (shell_surface_)
      shell_surface_->SetTitle(title);
  }

  void SetApplicationId(const char* application_id) {
    if (shell_surface_)
      shell_surface_->SetApplicationId(application_id);
  }

  void Move() {
    if (shell_surface_)
      shell_surface_->StartMove();
  }

  void Resize(int component) {
    if (!shell_surface_)
      return;

    if (component != HTNOWHERE)
      shell_surface_->StartResize(component);
  }

  void SetMaximumSize(const gfx::Size& size) {
    if (shell_surface_)
      shell_surface_->SetMaximumSize(size);
  }

  void SetMinimumSize(const gfx::Size& size) {
    if (shell_surface_)
      shell_surface_->SetMinimumSize(size);
  }

  void Maximize() {
    if (shell_surface_)
      shell_surface_->Maximize();
  }

  void Restore() {
    if (shell_surface_)
      shell_surface_->Restore();
  }

  void SetFullscreen(bool fullscreen) {
    if (shell_surface_)
      shell_surface_->SetFullscreen(fullscreen);
  }

  void Minimize() {
    if (shell_surface_)
      shell_surface_->Minimize();
  }

 private:
  void OnClose() {
    zxdg_toplevel_v6_send_close(resource_);
    wl_client_flush(wl_resource_get_client(resource_));
  }

  static void AddState(wl_array* states, zxdg_toplevel_v6_state state) {
    zxdg_toplevel_v6_state* value = static_cast<zxdg_toplevel_v6_state*>(
        wl_array_add(states, sizeof(zxdg_toplevel_v6_state)));
    DCHECK(value);
    *value = state;
  }

  void OnConfigure(const gfx::Size& size,
                   ash::mojom::WindowStateType state_type,
                   bool resizing,
                   bool activated) {
    wl_array states;
    wl_array_init(&states);
    if (state_type == ash::mojom::WindowStateType::MAXIMIZED)
      AddState(&states, ZXDG_TOPLEVEL_V6_STATE_MAXIMIZED);
    if (state_type == ash::mojom::WindowStateType::FULLSCREEN)
      AddState(&states, ZXDG_TOPLEVEL_V6_STATE_FULLSCREEN);
    if (resizing)
      AddState(&states, ZXDG_TOPLEVEL_V6_STATE_RESIZING);
    if (activated)
      AddState(&states, ZXDG_TOPLEVEL_V6_STATE_ACTIVATED);
    zxdg_toplevel_v6_send_configure(resource_, size.width(), size.height(),
                                    &states);
    wl_array_release(&states);
  }

  wl_resource* const resource_;
  XdgShellSurface* shell_surface_;
  base::WeakPtrFactory<WaylandToplevel> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(WaylandToplevel);
};

void xdg_toplevel_v6_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void xdg_toplevel_v6_set_parent(wl_client* client,
                                wl_resource* resource,
                                wl_resource* parent) {
  WaylandToplevel* parent_surface = nullptr;
  if (parent)
    parent_surface = GetUserDataAs<WaylandToplevel>(parent);

  GetUserDataAs<WaylandToplevel>(resource)->SetParent(parent_surface);
}

void xdg_toplevel_v6_set_title(wl_client* client,
                               wl_resource* resource,
                               const char* title) {
  GetUserDataAs<WaylandToplevel>(resource)->SetTitle(
      base::string16(base::UTF8ToUTF16(title)));
}

void xdg_toplevel_v6_set_app_id(wl_client* client,
                                wl_resource* resource,
                                const char* app_id) {
  GetUserDataAs<WaylandToplevel>(resource)->SetApplicationId(app_id);
}

void xdg_toplevel_v6_show_window_menu(wl_client* client,
                                      wl_resource* resource,
                                      wl_resource* seat,
                                      uint32_t serial,
                                      int32_t x,
                                      int32_t y) {
  NOTIMPLEMENTED();
}

void xdg_toplevel_v6_move(wl_client* client,
                          wl_resource* resource,
                          wl_resource* seat,
                          uint32_t serial) {
  GetUserDataAs<WaylandToplevel>(resource)->Move();
}

void xdg_toplevel_v6_resize(wl_client* client,
                            wl_resource* resource,
                            wl_resource* seat,
                            uint32_t serial,
                            uint32_t edges) {
  GetUserDataAs<WaylandToplevel>(resource)->Resize(
      XdgToplevelV6ResizeComponent(edges));
}

void xdg_toplevel_v6_set_max_size(wl_client* client,
                                  wl_resource* resource,
                                  int32_t width,
                                  int32_t height) {
  GetUserDataAs<WaylandToplevel>(resource)->SetMaximumSize(
      gfx::Size(width, height));
}

void xdg_toplevel_v6_set_min_size(wl_client* client,
                                  wl_resource* resource,
                                  int32_t width,
                                  int32_t height) {
  GetUserDataAs<WaylandToplevel>(resource)->SetMinimumSize(
      gfx::Size(width, height));
}

void xdg_toplevel_v6_set_maximized(wl_client* client, wl_resource* resource) {
  GetUserDataAs<WaylandToplevel>(resource)->Maximize();
}

void xdg_toplevel_v6_unset_maximized(wl_client* client, wl_resource* resource) {
  GetUserDataAs<WaylandToplevel>(resource)->Restore();
}

void xdg_toplevel_v6_set_fullscreen(wl_client* client,
                                    wl_resource* resource,
                                    wl_resource* output) {
  GetUserDataAs<WaylandToplevel>(resource)->SetFullscreen(true);
}

void xdg_toplevel_v6_unset_fullscreen(wl_client* client,
                                      wl_resource* resource) {
  GetUserDataAs<WaylandToplevel>(resource)->SetFullscreen(false);
}

void xdg_toplevel_v6_set_minimized(wl_client* client, wl_resource* resource) {
  GetUserDataAs<WaylandToplevel>(resource)->Minimize();
}

const struct zxdg_toplevel_v6_interface xdg_toplevel_v6_implementation = {
    xdg_toplevel_v6_destroy,          xdg_toplevel_v6_set_parent,
    xdg_toplevel_v6_set_title,        xdg_toplevel_v6_set_app_id,
    xdg_toplevel_v6_show_window_menu, xdg_toplevel_v6_move,
    xdg_toplevel_v6_resize,           xdg_toplevel_v6_set_max_size,
    xdg_toplevel_v6_set_min_size,     xdg_toplevel_v6_set_maximized,
    xdg_toplevel_v6_unset_maximized,  xdg_toplevel_v6_set_fullscreen,
    xdg_toplevel_v6_unset_fullscreen, xdg_toplevel_v6_set_minimized};

////////////////////////////////////////////////////////////////////////////////
// xdg_popup_interface:

// Wrapper around shell surface that allows us to handle the case where the
// xdg surface resource is destroyed before the popup resource.
class WaylandPopup : aura::WindowObserver {
 public:
  WaylandPopup(wl_resource* resource, wl_resource* surface_resource)
      : resource_(resource),
        shell_surface_(GetUserDataAs<ShellSurface>(surface_resource)),
        weak_ptr_factory_(this) {
    shell_surface_->host_window()->AddObserver(this);
    shell_surface_->set_close_callback(
        base::Bind(&WaylandPopup::OnClose, weak_ptr_factory_.GetWeakPtr()));
    shell_surface_->set_configure_callback(
        base::Bind(&HandleXdgSurfaceV6ConfigureCallback, surface_resource,
                   base::Bind(&WaylandPopup::OnConfigure,
                              weak_ptr_factory_.GetWeakPtr())));
  }
  ~WaylandPopup() override {
    if (shell_surface_)
      shell_surface_->host_window()->RemoveObserver(this);
  }

  void Grab() {
    if (!shell_surface_) {
      wl_resource_post_error(resource_, ZXDG_POPUP_V6_ERROR_INVALID_GRAB,
                             "the surface has already been destroyed");
      return;
    }
    if (shell_surface_->GetWidget()) {
      wl_resource_post_error(resource_, ZXDG_POPUP_V6_ERROR_INVALID_GRAB,
                             "grab must be called before construction");
      return;
    }
    shell_surface_->Grab();
  }

  // Overridden from aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override {
    shell_surface_ = nullptr;
  }

 private:
  void OnClose() {
    zxdg_popup_v6_send_popup_done(resource_);
    wl_client_flush(wl_resource_get_client(resource_));
  }

  void OnConfigure(const gfx::Size& size,
                   ash::mojom::WindowStateType state_type,
                   bool resizing,
                   bool activated) {
    // Nothing to do here as popups don't have additional configure state.
  }

  wl_resource* const resource_;
  ShellSurface* shell_surface_;
  base::WeakPtrFactory<WaylandPopup> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(WaylandPopup);
};

void xdg_popup_v6_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void xdg_popup_v6_grab(wl_client* client,
                       wl_resource* resource,
                       wl_resource* seat,
                       uint32_t serial) {
  GetUserDataAs<WaylandPopup>(resource)->Grab();
}

const struct zxdg_popup_v6_interface xdg_popup_v6_implementation = {
    xdg_popup_v6_destroy, xdg_popup_v6_grab};

////////////////////////////////////////////////////////////////////////////////
// xdg_surface_interface:

void xdg_surface_v6_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void xdg_surface_v6_get_toplevel(wl_client* client,
                                 wl_resource* resource,
                                 uint32_t id) {
  ShellSurface* shell_surface = GetUserDataAs<ShellSurface>(resource);
  if (shell_surface->enabled()) {
    wl_resource_post_error(resource, ZXDG_SURFACE_V6_ERROR_ALREADY_CONSTRUCTED,
                           "surface has already been constructed");
    return;
  }

  shell_surface->SetCanMinimize(true);
  shell_surface->SetEnabled(true);

  wl_resource* xdg_toplevel_resource =
      wl_resource_create(client, &zxdg_toplevel_v6_interface, 1, id);

  SetImplementation(
      xdg_toplevel_resource, &xdg_toplevel_v6_implementation,
      std::make_unique<WaylandToplevel>(xdg_toplevel_resource, resource));
}

void xdg_surface_v6_get_popup(wl_client* client,
                              wl_resource* resource,
                              uint32_t id,
                              wl_resource* parent_resource,
                              wl_resource* positioner_resource) {
  XdgShellSurface* shell_surface = GetUserDataAs<XdgShellSurface>(resource);
  if (shell_surface->enabled()) {
    wl_resource_post_error(resource, ZXDG_SURFACE_V6_ERROR_ALREADY_CONSTRUCTED,
                           "surface has already been constructed");
    return;
  }

  XdgShellSurface* parent = GetUserDataAs<XdgShellSurface>(parent_resource);
  if (!parent->GetWidget()) {
    wl_resource_post_error(resource, ZXDG_SURFACE_V6_ERROR_NOT_CONSTRUCTED,
                           "popup parent not constructed");
    return;
  }

  if (shell_surface->GetWidget()) {
    wl_resource_post_error(resource, ZXDG_SURFACE_V6_ERROR_ALREADY_CONSTRUCTED,
                           "get_popup is called after constructed");
    return;
  }

  display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(
          parent->GetWidget()->GetNativeWindow());
  gfx::Rect work_area = display.work_area();
  wm::ConvertRectFromScreen(parent->GetWidget()->GetNativeWindow(), &work_area);

  WaylandPositioner* positioner =
      GetUserDataAs<WaylandPositioner>(positioner_resource);
  // Try layout using parent's flip state.
  positioner->x_flipped = parent->x_flipped();
  positioner->y_flipped = parent->y_flipped();

  gfx::Point position = positioner->CalculatePosition(work_area);

  // Remember the new flip state for its child popups.
  shell_surface->set_x_flipped(positioner->x_flipped);
  shell_surface->set_y_flipped(positioner->y_flipped);

  // |position| is relative to the parent's contents view origin, and |origin|
  // is in screen coordinates.
  gfx::Point origin = position;
  views::View::ConvertPointToScreen(
      parent->GetWidget()->widget_delegate()->GetContentsView(), &origin);
  shell_surface->SetOrigin(origin);
  shell_surface->SetContainer(ash::kShellWindowId_MenuContainer);
  shell_surface->DisableMovement();
  shell_surface->SetActivatable(false);
  shell_surface->SetCanMinimize(false);
  shell_surface->SetParent(parent);
  shell_surface->SetPopup();
  shell_surface->SetEnabled(true);

  wl_resource* xdg_popup_resource =
      wl_resource_create(client, &zxdg_popup_v6_interface, 1, id);

  SetImplementation(
      xdg_popup_resource, &xdg_popup_v6_implementation,
      std::make_unique<WaylandPopup>(xdg_popup_resource, resource));
}

void xdg_surface_v6_set_window_geometry(wl_client* client,
                                        wl_resource* resource,
                                        int32_t x,
                                        int32_t y,
                                        int32_t width,
                                        int32_t height) {
  GetUserDataAs<ShellSurface>(resource)->SetGeometry(
      gfx::Rect(x, y, width, height));
}

void xdg_surface_v6_ack_configure(wl_client* client,
                                  wl_resource* resource,
                                  uint32_t serial) {
  GetUserDataAs<ShellSurface>(resource)->AcknowledgeConfigure(serial);
}

const struct zxdg_surface_v6_interface xdg_surface_v6_implementation = {
    xdg_surface_v6_destroy, xdg_surface_v6_get_toplevel,
    xdg_surface_v6_get_popup, xdg_surface_v6_set_window_geometry,
    xdg_surface_v6_ack_configure};

////////////////////////////////////////////////////////////////////////////////
// xdg_shell_interface:

void xdg_shell_v6_destroy(wl_client* client, wl_resource* resource) {
  // Nothing to do here.
}

void xdg_shell_v6_create_positioner(wl_client* client,
                                    wl_resource* resource,
                                    uint32_t id) {
  wl_resource* positioner_resource =
      wl_resource_create(client, &zxdg_positioner_v6_interface, 1, id);

  SetImplementation(positioner_resource, &xdg_positioner_v6_implementation,
                    std::make_unique<WaylandPositioner>());
}

void xdg_shell_v6_get_xdg_surface(wl_client* client,
                                  wl_resource* resource,
                                  uint32_t id,
                                  wl_resource* surface) {
  std::unique_ptr<ShellSurface> shell_surface =
      GetUserDataAs<Display>(resource)->CreateXdgShellSurface(
          GetUserDataAs<Surface>(surface));
  if (!shell_surface) {
    wl_resource_post_error(resource, ZXDG_SHELL_V6_ERROR_ROLE,
                           "surface has already been assigned a role");
    return;
  }

  // Xdg shell v6 surfaces are initially disabled and needs to be explicitly
  // mapped before they are enabled and can become visible.
  shell_surface->SetEnabled(false);

  wl_resource* xdg_surface_resource =
      wl_resource_create(client, &zxdg_surface_v6_interface, 1, id);

  SetImplementation(xdg_surface_resource, &xdg_surface_v6_implementation,
                    std::move(shell_surface));
}

void xdg_shell_v6_pong(wl_client* client,
                       wl_resource* resource,
                       uint32_t serial) {
  NOTIMPLEMENTED();
}

const struct zxdg_shell_v6_interface xdg_shell_v6_implementation = {
    xdg_shell_v6_destroy, xdg_shell_v6_create_positioner,
    xdg_shell_v6_get_xdg_surface, xdg_shell_v6_pong};

void bind_xdg_shell_v6(wl_client* client,
                       void* data,
                       uint32_t version,
                       uint32_t id) {
  wl_resource* resource =
      wl_resource_create(client, &zxdg_shell_v6_interface, 1, id);

  wl_resource_set_implementation(resource, &xdg_shell_v6_implementation, data,
                                 nullptr);
}

////////////////////////////////////////////////////////////////////////////////
// remote_surface_interface:

SurfaceFrameType RemoteShellSurfaceFrameType(uint32_t frame_type) {
  switch (frame_type) {
    case ZCR_REMOTE_SURFACE_V1_FRAME_TYPE_NONE:
      return SurfaceFrameType::NONE;
    case ZCR_REMOTE_SURFACE_V1_FRAME_TYPE_NORMAL:
      return SurfaceFrameType::NORMAL;
    case ZCR_REMOTE_SURFACE_V1_FRAME_TYPE_SHADOW:
      return SurfaceFrameType::SHADOW;
    case ZCR_REMOTE_SURFACE_V1_FRAME_TYPE_AUTOHIDE:
      return SurfaceFrameType::AUTOHIDE;
    case ZCR_REMOTE_SURFACE_V1_FRAME_TYPE_OVERLAY:
      return SurfaceFrameType::OVERLAY;
    default:
      VLOG(2) << "Unknown remote-shell frame type: " << frame_type;
      return SurfaceFrameType::NONE;
  }
}

void remote_surface_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void remote_surface_set_app_id(wl_client* client,
                               wl_resource* resource,
                               const char* app_id) {
  GetUserDataAs<ShellSurfaceBase>(resource)->SetApplicationId(app_id);
}

void remote_surface_set_window_geometry(wl_client* client,
                                        wl_resource* resource,
                                        int32_t x,
                                        int32_t y,
                                        int32_t width,
                                        int32_t height) {
  GetUserDataAs<ShellSurfaceBase>(resource)->SetGeometry(
      gfx::Rect(x, y, width, height));
}

void remote_surface_set_orientation(wl_client* client,
                                    wl_resource* resource,
                                    int32_t orientation) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetOrientation(
      orientation == ZCR_REMOTE_SURFACE_V1_ORIENTATION_PORTRAIT
          ? Orientation::PORTRAIT
          : Orientation::LANDSCAPE);
}

void remote_surface_set_scale(wl_client* client,
                              wl_resource* resource,
                              wl_fixed_t scale) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetScale(
      wl_fixed_to_double(scale));
}

void remote_surface_set_rectangular_shadow_DEPRECATED(wl_client* client,
                                                      wl_resource* resource,
                                                      int32_t x,
                                                      int32_t y,
                                                      int32_t width,
                                                      int32_t height) {
  NOTIMPLEMENTED();
}

void remote_surface_set_rectangular_shadow_background_opacity_DEPRECATED(
    wl_client* client,
    wl_resource* resource,
    wl_fixed_t opacity) {
  NOTIMPLEMENTED();
}

void remote_surface_set_title(wl_client* client,
                              wl_resource* resource,
                              const char* title) {
  GetUserDataAs<ShellSurfaceBase>(resource)->SetTitle(
      base::string16(base::UTF8ToUTF16(title)));
}

void remote_surface_set_top_inset(wl_client* client,
                                  wl_resource* resource,
                                  int32_t height) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetTopInset(height);
}

void remote_surface_activate(wl_client* client,
                             wl_resource* resource,
                             uint32_t serial) {
  ShellSurfaceBase* shell_surface = GetUserDataAs<ShellSurfaceBase>(resource);
  shell_surface->Activate();
}

void remote_surface_maximize(wl_client* client, wl_resource* resource) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetMaximized();
}

void remote_surface_minimize(wl_client* client, wl_resource* resource) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetMinimized();
}

void remote_surface_restore(wl_client* client, wl_resource* resource) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetRestored();
}

void remote_surface_fullscreen(wl_client* client, wl_resource* resource) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetFullscreen(true);
}

void remote_surface_unfullscreen(wl_client* client, wl_resource* resource) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetFullscreen(false);
}

void remote_surface_pin(wl_client* client,
                        wl_resource* resource,
                        int32_t trusted) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetPinned(
      trusted ? ash::mojom::WindowPinType::TRUSTED_PINNED
              : ash::mojom::WindowPinType::PINNED);
}

void remote_surface_unpin(wl_client* client, wl_resource* resource) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetPinned(
      ash::mojom::WindowPinType::NONE);
}

void remote_surface_set_system_modal(wl_client* client, wl_resource* resource) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetSystemModal(true);
}

void remote_surface_unset_system_modal(wl_client* client,
                                       wl_resource* resource) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetSystemModal(false);
}

void remote_surface_set_rectangular_surface_shadow(wl_client* client,
                                                   wl_resource* resource,
                                                   int32_t x,
                                                   int32_t y,
                                                   int32_t width,
                                                   int32_t height) {
  ClientControlledShellSurface* shell_surface =
      GetUserDataAs<ClientControlledShellSurface>(resource);
  shell_surface->SetShadowBounds(gfx::Rect(x, y, width, height));
}

void remote_surface_set_systemui_visibility(wl_client* client,
                                            wl_resource* resource,
                                            uint32_t visibility) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetSystemUiVisibility(
      visibility != ZCR_REMOTE_SURFACE_V1_SYSTEMUI_VISIBILITY_STATE_VISIBLE);
}

void remote_surface_set_always_on_top(wl_client* client,
                                      wl_resource* resource) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetAlwaysOnTop(true);
}

void remote_surface_unset_always_on_top(wl_client* client,
                                        wl_resource* resource) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetAlwaysOnTop(false);
}

void remote_surface_ack_configure(wl_client* client,
                                  wl_resource* resource,
                                  uint32_t serial) {
  // DEPRECATED
}

void remote_surface_move(wl_client* client, wl_resource* resource) {
  // DEPRECATED
}

void remote_surface_set_window_type(wl_client* client,
                                    wl_resource* resource,
                                    uint32_t type) {
  if (type == ZCR_REMOTE_SURFACE_V1_WINDOW_TYPE_SYSTEM_UI) {
    auto* widget = GetUserDataAs<ShellSurfaceBase>(resource)->GetWidget();
    if (widget) {
      widget->GetNativeWindow()->SetProperty(ash::kHideInOverviewKey, true);

      wm::SetWindowVisibilityAnimationType(
          widget->GetNativeWindow(), wm::WINDOW_VISIBILITY_ANIMATION_TYPE_FADE);
    }
  }
}

void remote_surface_resize(wl_client* client, wl_resource* resource) {
  // DEPRECATED
}

void remote_surface_set_resize_outset(wl_client* client,
                                      wl_resource* resource,
                                      int32_t outset) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetResizeOutset(
      outset);
}

void remote_surface_start_move(wl_client* client,
                               wl_resource* resource,
                               int32_t x,
                               int32_t y) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->StartDrag(
      HTCAPTION, gfx::Point(x, y));
}

void remote_surface_set_can_maximize(wl_client* client, wl_resource* resource) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetCanMaximize(true);
}

void remote_surface_unset_can_maximize(wl_client* client,
                                       wl_resource* resource) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetCanMaximize(false);
}

void remote_surface_set_min_size(wl_client* client,
                                 wl_resource* resource,
                                 int32_t width,
                                 int32_t height) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetMinimumSize(
      gfx::Size(width, height));
}

void remote_surface_set_max_size(wl_client* client,
                                 wl_resource* resource,
                                 int32_t width,
                                 int32_t height) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetMaximumSize(
      gfx::Size(width, height));
}

void remote_surface_set_snapped_to_left(wl_client* client,
                                        wl_resource* resource) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetSnappedToLeft();
}

void remote_surface_set_snapped_to_right(wl_client* client,
                                         wl_resource* resource) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetSnappedToRight();
}

void remote_surface_start_resize(wl_client* client,
                                 wl_resource* resource,
                                 uint32_t direction,
                                 int32_t x,
                                 int32_t y) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->StartDrag(
      Component(direction), gfx::Point(x, y));
}

void remote_surface_set_frame(wl_client* client,
                              wl_resource* resource,
                              uint32_t type) {
  ClientControlledShellSurface* shell_surface =
      GetUserDataAs<ClientControlledShellSurface>(resource);
  shell_surface->root_surface()->SetFrame(RemoteShellSurfaceFrameType(type));
}

void remote_surface_set_frame_buttons(wl_client* client,
                                      wl_resource* resource,
                                      uint32_t visible_button_mask,
                                      uint32_t enabled_button_mask) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetFrameButtons(
      CaptionButtonMask(visible_button_mask),
      CaptionButtonMask(enabled_button_mask));
}

void remote_surface_set_extra_title(wl_client* client,
                                    wl_resource* resource,
                                    const char* extra_title) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetExtraTitle(
      base::string16(base::UTF8ToUTF16(extra_title)));
}

ash::OrientationLockType OrientationLock(uint32_t orientation_lock) {
  switch (orientation_lock) {
    case ZCR_REMOTE_SURFACE_V1_ORIENTATION_LOCK_NONE:
      return ash::OrientationLockType::kAny;
    case ZCR_REMOTE_SURFACE_V1_ORIENTATION_LOCK_CURRENT:
      return ash::OrientationLockType::kCurrent;
    case ZCR_REMOTE_SURFACE_V1_ORIENTATION_LOCK_PORTRAIT:
      return ash::OrientationLockType::kPortrait;
    case ZCR_REMOTE_SURFACE_V1_ORIENTATION_LOCK_LANDSCAPE:
      return ash::OrientationLockType::kLandscape;
    case ZCR_REMOTE_SURFACE_V1_ORIENTATION_LOCK_PORTRAIT_PRIMARY:
      return ash::OrientationLockType::kPortraitPrimary;
    case ZCR_REMOTE_SURFACE_V1_ORIENTATION_LOCK_PORTRAIT_SECONDARY:
      return ash::OrientationLockType::kPortraitSecondary;
    case ZCR_REMOTE_SURFACE_V1_ORIENTATION_LOCK_LANDSCAPE_PRIMARY:
      return ash::OrientationLockType::kLandscapePrimary;
    case ZCR_REMOTE_SURFACE_V1_ORIENTATION_LOCK_LANDSCAPE_SECONDARY:
      return ash::OrientationLockType::kLandscapeSecondary;
  }
  VLOG(2) << "Unexpected value of orientation_lock: " << orientation_lock;
  return ash::OrientationLockType::kAny;
}

void remote_surface_set_orientation_lock(wl_client* client,
                                         wl_resource* resource,
                                         uint32_t orientation_lock) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetOrientationLock(
      OrientationLock(orientation_lock));
}

void remote_surface_pip(wl_client* client, wl_resource* resource) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetPip();
}

void remote_surface_set_bounds(wl_client* client,
                               wl_resource* resource,
                               uint32_t display_id_hi,
                               uint32_t display_id_lo,
                               int32_t x,
                               int32_t y,
                               int32_t width,
                               int32_t height) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetBounds(
      static_cast<int64_t>(display_id_hi) << 32 | display_id_lo,
      gfx::Rect(x, y, width, height));
}

const struct zcr_remote_surface_v1_interface remote_surface_implementation = {
    remote_surface_destroy,
    remote_surface_set_app_id,
    remote_surface_set_window_geometry,
    remote_surface_set_scale,
    remote_surface_set_rectangular_shadow_DEPRECATED,
    remote_surface_set_rectangular_shadow_background_opacity_DEPRECATED,
    remote_surface_set_title,
    remote_surface_set_top_inset,
    remote_surface_activate,
    remote_surface_maximize,
    remote_surface_minimize,
    remote_surface_restore,
    remote_surface_fullscreen,
    remote_surface_unfullscreen,
    remote_surface_pin,
    remote_surface_unpin,
    remote_surface_set_system_modal,
    remote_surface_unset_system_modal,
    remote_surface_set_rectangular_surface_shadow,
    remote_surface_set_systemui_visibility,
    remote_surface_set_always_on_top,
    remote_surface_unset_always_on_top,
    remote_surface_ack_configure,
    remote_surface_move,
    remote_surface_set_orientation,
    remote_surface_set_window_type,
    remote_surface_resize,
    remote_surface_set_resize_outset,
    remote_surface_start_move,
    remote_surface_set_can_maximize,
    remote_surface_unset_can_maximize,
    remote_surface_set_min_size,
    remote_surface_set_max_size,
    remote_surface_set_snapped_to_left,
    remote_surface_set_snapped_to_right,
    remote_surface_start_resize,
    remote_surface_set_frame,
    remote_surface_set_frame_buttons,
    remote_surface_set_extra_title,
    remote_surface_set_orientation_lock,
    remote_surface_pip,
    remote_surface_set_bounds};

////////////////////////////////////////////////////////////////////////////////
// notification_surface_interface:

void notification_surface_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void notification_surface_set_app_id(wl_client* client,
                                     wl_resource* resource,
                                     const char* app_id) {
  GetUserDataAs<NotificationSurface>(resource)->SetApplicationId(app_id);
}

const struct zcr_notification_surface_v1_interface
    notification_surface_implementation = {notification_surface_destroy,
                                           notification_surface_set_app_id};

////////////////////////////////////////////////////////////////////////////////
// input_method_surface_interface:

void input_method_surface_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

const struct zcr_input_method_surface_v1_interface
    input_method_surface_implementation = {input_method_surface_destroy};

////////////////////////////////////////////////////////////////////////////////
// remote_shell_interface:

// Implements remote shell interface and monitors workspace state needed
// for the remote shell interface.
class WaylandRemoteShell : public ash::TabletModeObserver,
                           public wm::ActivationChangeObserver,
                           public display::DisplayObserver {
 public:
  WaylandRemoteShell(Display* display, wl_resource* remote_shell_resource)
      : display_(display),
        remote_shell_resource_(remote_shell_resource),
        weak_ptr_factory_(this) {
    auto* helper = WMHelper::GetInstance();
    helper->AddTabletModeObserver(this);
    helper->AddActivationObserver(this);
    display::Screen::GetScreen()->AddObserver(this);

    layout_mode_ = helper->IsTabletModeWindowManagerEnabled()
                       ? ZCR_REMOTE_SHELL_V1_LAYOUT_MODE_TABLET
                       : ZCR_REMOTE_SHELL_V1_LAYOUT_MODE_WINDOWED;

    if (wl_resource_get_version(remote_shell_resource_) >= 8) {
      double scale_factor = GetDefaultDeviceScaleFactor();
      // Send using 16.16 fixed point.
      const int kDecimalBits = 24;
      int32_t fixed_scale =
          static_cast<int32_t>(scale_factor * (1 << kDecimalBits));
      zcr_remote_shell_v1_send_default_device_scale_factor(
          remote_shell_resource_, fixed_scale);
    }

    SendDisplayMetrics();
    SendActivated(helper->GetActiveWindow(), nullptr);
  }
  ~WaylandRemoteShell() override {
    auto* helper = WMHelper::GetInstance();
    helper->RemoveTabletModeObserver(this);
    helper->RemoveActivationObserver(this);
    display::Screen::GetScreen()->RemoveObserver(this);
  }

  std::unique_ptr<ClientControlledShellSurface> CreateShellSurface(
      Surface* surface,
      int container,
      double default_device_scale_factor) {
    return display_->CreateClientControlledShellSurface(
        surface, container, default_device_scale_factor);
  }

  std::unique_ptr<NotificationSurface> CreateNotificationSurface(
      Surface* surface,
      const std::string& notification_key) {
    return display_->CreateNotificationSurface(surface, notification_key);
  }

  std::unique_ptr<InputMethodSurface> CreateInputMethodSurface(
      Surface* surface,
      double default_device_scale_factor) {
    return display_->CreateInputMethodSurface(surface,
                                              default_device_scale_factor);
  }

  // Overridden from display::DisplayObserver:
  void OnDisplayAdded(const display::Display& new_display) override {
    ScheduleSendDisplayMetrics(0);
  }

  void OnDisplayRemoved(const display::Display& old_display) override {
    ScheduleSendDisplayMetrics(0);
  }

  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override {
    // No need to update when a primary display has changed without bounds
    // change. See WaylandDisplayObserver::OnDisplayMetricsChanged
    // for more details.
    if (changed_metrics &
        (DISPLAY_METRIC_BOUNDS | DISPLAY_METRIC_DEVICE_SCALE_FACTOR |
         DISPLAY_METRIC_ROTATION | DISPLAY_METRIC_WORK_AREA)) {
      ScheduleSendDisplayMetrics(0);
    }
  }

  // Overridden from ash::TabletModeObserver:
  void OnTabletModeStarted() override {
    layout_mode_ = ZCR_REMOTE_SHELL_V1_LAYOUT_MODE_TABLET;
    ScheduleSendDisplayMetrics(kConfigureDelayAfterLayoutSwitchMs);
  }
  void OnTabletModeEnding() override {
    layout_mode_ = ZCR_REMOTE_SHELL_V1_LAYOUT_MODE_WINDOWED;
    ScheduleSendDisplayMetrics(kConfigureDelayAfterLayoutSwitchMs);
  }
  void OnTabletModeEnded() override {}

  // Overridden from wm::ActivationChangeObserver:
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override {
    SendActivated(gained_active, lost_active);
  }

 private:
  void ScheduleSendDisplayMetrics(int delay_ms) {
    needs_send_display_metrics_ = true;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&WaylandRemoteShell::SendDisplayMetrics,
                       weak_ptr_factory_.GetWeakPtr()),
        base::TimeDelta::FromMilliseconds(delay_ms));
  }

  // Returns the transform that a display's output is currently adjusted for.
  wl_output_transform DisplayTransform(display::Display::Rotation rotation) {
    switch (rotation) {
      case display::Display::ROTATE_0:
        return WL_OUTPUT_TRANSFORM_NORMAL;
      case display::Display::ROTATE_90:
        return WL_OUTPUT_TRANSFORM_90;
      case display::Display::ROTATE_180:
        return WL_OUTPUT_TRANSFORM_180;
      case display::Display::ROTATE_270:
        return WL_OUTPUT_TRANSFORM_270;
    }
    NOTREACHED();
    return WL_OUTPUT_TRANSFORM_NORMAL;
  }

  void SendDisplayMetrics() {
    if (!needs_send_display_metrics_)
      return;
    needs_send_display_metrics_ = false;

    const display::Screen* screen = display::Screen::GetScreen();

    for (const auto& display : screen->GetAllDisplays()) {
      const gfx::Rect& bounds = display.bounds();
      const gfx::Insets& insets = display.GetWorkAreaInsets();

      double device_scale_factor = display.device_scale_factor();

      uint32_t display_id_hi = static_cast<uint32_t>(display.id() >> 32);
      uint32_t display_id_lo = static_cast<uint32_t>(display.id());

      zcr_remote_shell_v1_send_workspace(
          remote_shell_resource_, display_id_hi, display_id_lo, bounds.x(),
          bounds.y(), bounds.width(), bounds.height(), insets.left(),
          insets.top(), insets.right(), insets.bottom(),
          DisplayTransform(display.rotation()),
          wl_fixed_from_double(device_scale_factor), display.IsInternal());

      if (wl_resource_get_version(remote_shell_resource_) >= 19) {
        gfx::Size size_in_pixel = display.GetSizeInPixel();

        wl_array data;
        wl_array_init(&data);

        const auto& bytes =
            WMHelper::GetInstance()->GetDisplayIdentificationData(display.id());
        for (uint8_t byte : bytes) {
          uint8_t* ptr =
              static_cast<uint8_t*>(wl_array_add(&data, sizeof(uint8_t)));
          DCHECK(ptr);
          *ptr = byte;
        }

        zcr_remote_shell_v1_send_display_info(
            remote_shell_resource_, display_id_hi, display_id_lo,
            size_in_pixel.width(), size_in_pixel.height(), &data);

        wl_array_release(&data);
      }
    }

    zcr_remote_shell_v1_send_configure(remote_shell_resource_, layout_mode_);
    wl_client_flush(wl_resource_get_client(remote_shell_resource_));
  }

  void SendActivated(aura::Window* gained_active, aura::Window* lost_active) {
    Surface* gained_active_surface =
        gained_active ? ShellSurface::GetMainSurface(gained_active) : nullptr;
    Surface* lost_active_surface =
        lost_active ? ShellSurface::GetMainSurface(lost_active) : nullptr;
    wl_resource* gained_active_surface_resource =
        gained_active_surface ? GetSurfaceResource(gained_active_surface)
                              : nullptr;
    wl_resource* lost_active_surface_resource =
        lost_active_surface ? GetSurfaceResource(lost_active_surface) : nullptr;

    wl_client* client = wl_resource_get_client(remote_shell_resource_);

    // If surface that gained active is not owned by remote shell client then
    // set it to null.
    if (gained_active_surface_resource &&
        wl_resource_get_client(gained_active_surface_resource) != client) {
      gained_active_surface_resource = nullptr;
    }

    // If surface that lost active is not owned by remote shell client then
    // set it to null.
    if (lost_active_surface_resource &&
        wl_resource_get_client(lost_active_surface_resource) != client) {
      lost_active_surface_resource = nullptr;
    }

    zcr_remote_shell_v1_send_activated(remote_shell_resource_,
                                       gained_active_surface_resource,
                                       lost_active_surface_resource);
    wl_client_flush(client);
  }

  // The exo display instance. Not owned.
  Display* const display_;

  // The remote shell resource associated with observer.
  wl_resource* const remote_shell_resource_;

  bool needs_send_display_metrics_ = true;

  int layout_mode_ = ZCR_REMOTE_SHELL_V1_LAYOUT_MODE_WINDOWED;

  base::WeakPtrFactory<WaylandRemoteShell> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(WaylandRemoteShell);
};

void remote_shell_destroy(wl_client* client, wl_resource* resource) {
  // Nothing to do here.
}

int RemoteSurfaceContainer(uint32_t container) {
  switch (container) {
    case ZCR_REMOTE_SHELL_V1_CONTAINER_DEFAULT:
      return ash::kShellWindowId_DefaultContainer;
    case ZCR_REMOTE_SHELL_V1_CONTAINER_OVERLAY:
      return ash::kShellWindowId_SystemModalContainer;
    default:
      DLOG(WARNING) << "Unsupported container: " << container;
      return ash::kShellWindowId_DefaultContainer;
  }
}

void HandleRemoteSurfaceCloseCallback(wl_resource* resource) {
  zcr_remote_surface_v1_send_close(resource);
  wl_client_flush(wl_resource_get_client(resource));
}

void HandleRemoteSurfaceStateChangedCallback(
    wl_resource* resource,
    ash::mojom::WindowStateType old_state_type,
    ash::mojom::WindowStateType new_state_type) {
  DCHECK_NE(old_state_type, new_state_type);

  uint32_t state_type = ZCR_REMOTE_SHELL_V1_STATE_TYPE_NORMAL;
  switch (new_state_type) {
    case ash::mojom::WindowStateType::MINIMIZED:
      state_type = ZCR_REMOTE_SHELL_V1_STATE_TYPE_MINIMIZED;
      break;
    case ash::mojom::WindowStateType::MAXIMIZED:
      state_type = ZCR_REMOTE_SHELL_V1_STATE_TYPE_MAXIMIZED;
      break;
    case ash::mojom::WindowStateType::FULLSCREEN:
      state_type = ZCR_REMOTE_SHELL_V1_STATE_TYPE_FULLSCREEN;
      break;
    case ash::mojom::WindowStateType::PINNED:
      state_type = ZCR_REMOTE_SHELL_V1_STATE_TYPE_PINNED;
      break;
    case ash::mojom::WindowStateType::TRUSTED_PINNED:
      state_type = ZCR_REMOTE_SHELL_V1_STATE_TYPE_TRUSTED_PINNED;
      break;
    case ash::mojom::WindowStateType::LEFT_SNAPPED:
      state_type = ZCR_REMOTE_SHELL_V1_STATE_TYPE_LEFT_SNAPPED;
      break;
    case ash::mojom::WindowStateType::RIGHT_SNAPPED:
      state_type = ZCR_REMOTE_SHELL_V1_STATE_TYPE_RIGHT_SNAPPED;
      break;
    case ash::mojom::WindowStateType::PIP:
      state_type = ZCR_REMOTE_SHELL_V1_STATE_TYPE_PIP;
      break;
    default:
      break;
  }

  zcr_remote_surface_v1_send_state_type_changed(resource, state_type);
  wl_client_flush(wl_resource_get_client(resource));
}

void HandleRemoteSurfaceBoundsChangedCallback(
    wl_resource* resource,
    ash::mojom::WindowStateType current_state,
    ash::mojom::WindowStateType requested_state,
    int64_t display_id,
    const gfx::Rect& bounds,
    bool resize,
    int bounds_change) {
  zcr_remote_surface_v1_bounds_change_reason reason =
      resize ? ZCR_REMOTE_SURFACE_V1_BOUNDS_CHANGE_REASON_RESIZE
             : ZCR_REMOTE_SURFACE_V1_BOUNDS_CHANGE_REASON_MOVE;
  if (bounds_change & ash::WindowResizer::kBoundsChange_Resizes) {
    reason = ZCR_REMOTE_SURFACE_V1_BOUNDS_CHANGE_REASON_DRAG_RESIZE;
  } else if (bounds_change & ash::WindowResizer::kBoundsChange_Repositions) {
    reason = ZCR_REMOTE_SURFACE_V1_BOUNDS_CHANGE_REASON_DRAG_MOVE;
  }
  // Override the reason only if the window enters snapped mode. If the window
  // resizes by dragging in snapped mode, we need to keep the original reason.
  if (requested_state != current_state) {
    if (requested_state == ash::mojom::WindowStateType::LEFT_SNAPPED) {
      reason = ZCR_REMOTE_SURFACE_V1_BOUNDS_CHANGE_REASON_SNAP_TO_LEFT;
    } else if (requested_state == ash::mojom::WindowStateType::RIGHT_SNAPPED) {
      reason = ZCR_REMOTE_SURFACE_V1_BOUNDS_CHANGE_REASON_SNAP_TO_RIGHT;
    }
  }
  zcr_remote_surface_v1_send_bounds_changed(
      resource, static_cast<uint32_t>(display_id >> 32),
      static_cast<uint32_t>(display_id), bounds.x(), bounds.y(), bounds.width(),
      bounds.height(), reason);
  wl_client_flush(wl_resource_get_client(resource));
}

void HandleRemoteSurfaceDragStartedCallback(wl_resource* resource,
                                            int component) {
  zcr_remote_surface_v1_send_drag_started(resource, ResizeDirection(component));
  wl_client_flush(wl_resource_get_client(resource));
}

void HandleRemoteSurfaceDragFinishedCallback(wl_resource* resource,
                                             int x,
                                             int y,
                                             bool canceled) {
  zcr_remote_surface_v1_send_drag_finished(resource, x, y, canceled ? 1 : 0);
  wl_client_flush(wl_resource_get_client(resource));
}

void HandleRemoteSurfaceGeometryChangedCallback(wl_resource* resource,
                                                const gfx::Rect& geometry) {
  zcr_remote_surface_v1_send_window_geometry_changed(
      resource, geometry.x(), geometry.y(), geometry.width(),
      geometry.height());
  wl_client_flush(wl_resource_get_client(resource));
}

void remote_shell_get_remote_surface(wl_client* client,
                                     wl_resource* resource,
                                     uint32_t id,
                                     wl_resource* surface,
                                     uint32_t container) {
  WaylandRemoteShell* shell = GetUserDataAs<WaylandRemoteShell>(resource);
  double default_scale_factor = wl_resource_get_version(resource) >= 8
                                    ? GetDefaultDeviceScaleFactor()
                                    : 1.0;

  std::unique_ptr<ClientControlledShellSurface> shell_surface =
      shell->CreateShellSurface(GetUserDataAs<Surface>(surface),
                                RemoteSurfaceContainer(container),
                                default_scale_factor);
  if (!shell_surface) {
    wl_resource_post_error(resource, ZCR_REMOTE_SHELL_V1_ERROR_ROLE,
                           "surface has already been assigned a role");
    return;
  }

  wl_resource* remote_surface_resource =
      wl_resource_create(client, &zcr_remote_surface_v1_interface,
                         wl_resource_get_version(resource), id);

  shell_surface->set_close_callback(
      base::Bind(&HandleRemoteSurfaceCloseCallback,
                 base::Unretained(remote_surface_resource)));
  shell_surface->set_state_changed_callback(
      base::Bind(&HandleRemoteSurfaceStateChangedCallback,
                 base::Unretained(remote_surface_resource)));
  shell_surface->set_geometry_changed_callback(
      base::BindRepeating(&HandleRemoteSurfaceGeometryChangedCallback,
                          base::Unretained(remote_surface_resource)));

  if (wl_resource_get_version(remote_surface_resource) >= 10) {
    shell_surface->set_client_controlled_move_resize(false);
    shell_surface->set_bounds_changed_callback(
        base::BindRepeating(&HandleRemoteSurfaceBoundsChangedCallback,
                            base::Unretained(remote_surface_resource)));
    shell_surface->set_drag_started_callback(
        base::BindRepeating(&HandleRemoteSurfaceDragStartedCallback,
                            base::Unretained(remote_surface_resource)));
    shell_surface->set_drag_finished_callback(
        base::BindRepeating(&HandleRemoteSurfaceDragFinishedCallback,
                            base::Unretained(remote_surface_resource)));
  }

  SetImplementation(remote_surface_resource, &remote_surface_implementation,
                    std::move(shell_surface));
}

void remote_shell_get_notification_surface(wl_client* client,
                                           wl_resource* resource,
                                           uint32_t id,
                                           wl_resource* surface,
                                           const char* notification_key) {
  if (GetUserDataAs<Surface>(surface)->HasSurfaceDelegate()) {
    wl_resource_post_error(resource, ZCR_REMOTE_SHELL_V1_ERROR_ROLE,
                           "surface has already been assigned a role");
    return;
  }

  std::unique_ptr<NotificationSurface> notification_surface =
      GetUserDataAs<WaylandRemoteShell>(resource)->CreateNotificationSurface(
          GetUserDataAs<Surface>(surface), std::string(notification_key));
  if (!notification_surface) {
    wl_resource_post_error(resource,
                           ZCR_REMOTE_SHELL_V1_ERROR_INVALID_NOTIFICATION_KEY,
                           "invalid notification key");
    return;
  }

  wl_resource* notification_surface_resource =
      wl_resource_create(client, &zcr_notification_surface_v1_interface,
                         wl_resource_get_version(resource), id);
  SetImplementation(notification_surface_resource,
                    &notification_surface_implementation,
                    std::move(notification_surface));
}

void remote_shell_get_input_method_surface(wl_client* client,
                                           wl_resource* resource,
                                           uint32_t id,
                                           wl_resource* surface) {
  if (GetUserDataAs<Surface>(surface)->HasSurfaceDelegate()) {
    wl_resource_post_error(resource, ZCR_REMOTE_SHELL_V1_ERROR_ROLE,
                           "surface has already been assigned a role");
    return;
  }

  std::unique_ptr<ClientControlledShellSurface> input_method_surface =
      GetUserDataAs<WaylandRemoteShell>(resource)->CreateInputMethodSurface(
          GetUserDataAs<Surface>(surface), GetDefaultDeviceScaleFactor());
  if (!input_method_surface) {
    wl_resource_post_error(resource, ZCR_REMOTE_SHELL_V1_ERROR_ROLE,
                           "Cannot create an IME surface");
    return;
  }

  wl_resource* input_method_surface_resource =
      wl_resource_create(client, &zcr_input_method_surface_v1_interface,
                         wl_resource_get_version(resource), id);
  SetImplementation(input_method_surface_resource,
                    &input_method_surface_implementation,
                    std::move(input_method_surface));
}

const struct zcr_remote_shell_v1_interface remote_shell_implementation = {
    remote_shell_destroy, remote_shell_get_remote_surface,
    remote_shell_get_notification_surface,
    remote_shell_get_input_method_surface};

const uint32_t remote_shell_version = 19;

void bind_remote_shell(wl_client* client,
                       void* data,
                       uint32_t version,
                       uint32_t id) {
  wl_resource* resource =
      wl_resource_create(client, &zcr_remote_shell_v1_interface,
                         std::min(version, remote_shell_version), id);

  SetImplementation(resource, &remote_shell_implementation,
                    std::make_unique<WaylandRemoteShell>(
                        static_cast<Display*>(data), resource));
}

////////////////////////////////////////////////////////////////////////////////
// aura_surface_interface:

class AuraSurface : public SurfaceObserver {
 public:
  explicit AuraSurface(Surface* surface) : surface_(surface) {
    surface_->AddSurfaceObserver(this);
    surface_->SetProperty(kSurfaceHasAuraSurfaceKey, true);
  }
  ~AuraSurface() override {
    if (surface_) {
      surface_->RemoveSurfaceObserver(this);
      surface_->SetProperty(kSurfaceHasAuraSurfaceKey, false);
    }
  }

  void SetFrame(SurfaceFrameType type) {
    if (surface_)
      surface_->SetFrame(type);
  }

  void SetFrameColors(SkColor active_frame_color,
                      SkColor inactive_frame_color) {
    if (surface_)
      surface_->SetFrameColors(active_frame_color, inactive_frame_color);
  }

  void SetParent(AuraSurface* parent, const gfx::Point& position) {
    if (surface_)
      surface_->SetParent(parent ? parent->surface_ : nullptr, position);
  }

  void SetStartupId(const char* startup_id) {
    if (surface_)
      surface_->SetStartupId(startup_id);
  }

  void SetApplicationId(const char* application_id) {
    if (surface_)
      surface_->SetApplicationId(application_id);
  }

  // Overridden from SurfaceObserver:
  void OnSurfaceDestroying(Surface* surface) override {
    surface->RemoveSurfaceObserver(this);
    surface_ = nullptr;
  }

 private:
  Surface* surface_;

  DISALLOW_COPY_AND_ASSIGN(AuraSurface);
};

SurfaceFrameType AuraSurfaceFrameType(uint32_t frame_type) {
  switch (frame_type) {
    case ZAURA_SURFACE_FRAME_TYPE_NONE:
      return SurfaceFrameType::NONE;
    case ZAURA_SURFACE_FRAME_TYPE_NORMAL:
      return SurfaceFrameType::NORMAL;
    case ZAURA_SURFACE_FRAME_TYPE_SHADOW:
      return SurfaceFrameType::SHADOW;
    default:
      VLOG(2) << "Unkonwn aura-shell frame type: " << frame_type;
      return SurfaceFrameType::NONE;
  }
}

void aura_surface_set_frame(wl_client* client, wl_resource* resource,
                            uint32_t type) {
  GetUserDataAs<AuraSurface>(resource)->SetFrame(AuraSurfaceFrameType(type));
}

void aura_surface_set_parent(wl_client* client,
                             wl_resource* resource,
                             wl_resource* parent_resource,
                             int32_t x,
                             int32_t y) {
  GetUserDataAs<AuraSurface>(resource)->SetParent(
      parent_resource ? GetUserDataAs<AuraSurface>(parent_resource) : nullptr,
      gfx::Point(x, y));
}

void aura_surface_set_frame_colors(wl_client* client,
                                   wl_resource* resource,
                                   uint32_t active_color,
                                   uint32_t inactive_color) {
  GetUserDataAs<AuraSurface>(resource)->SetFrameColors(active_color,
                                                       inactive_color);
}

void aura_surface_set_startup_id(wl_client* client,
                                 wl_resource* resource,
                                 const char* startup_id) {
  GetUserDataAs<AuraSurface>(resource)->SetStartupId(startup_id);
}

void aura_surface_set_application_id(wl_client* client,
                                     wl_resource* resource,
                                     const char* application_id) {
  GetUserDataAs<AuraSurface>(resource)->SetApplicationId(application_id);
}

const struct zaura_surface_interface aura_surface_implementation = {
    aura_surface_set_frame, aura_surface_set_parent,
    aura_surface_set_frame_colors, aura_surface_set_startup_id,
    aura_surface_set_application_id};

////////////////////////////////////////////////////////////////////////////////
// aura_output_interface:

class AuraOutput : public WaylandDisplayObserver::ScaleObserver {
 public:
  explicit AuraOutput(wl_resource* resource) : resource_(resource) {}

  // Overridden from WaylandDisplayObserver::ScaleObserver:
  void OnDisplayScalesChanged(const display::Display& display) override {
    display::DisplayManager* display_manager =
          ash::Shell::Get()->display_manager();
    const display::ManagedDisplayInfo& display_info =
          display_manager->GetDisplayInfo(display.id());

    if (wl_resource_get_version(resource_) >=
        ZAURA_OUTPUT_SCALE_SINCE_VERSION) {
      display::ManagedDisplayMode active_mode;
      bool rv = display_manager->GetActiveModeForDisplayId(display.id(),
                                                           &active_mode);
      DCHECK(rv);
      const int32_t current_output_scale =
          std::round(display_info.zoom_factor() * 1000.f);
      std::vector<float> zoom_factors =
          display::GetDisplayZoomFactors(active_mode);

      // Ensure that the current zoom factor is a part of the list.
      auto it = std::find_if(
          zoom_factors.begin(), zoom_factors.end(),
          [&display_info](float zoom_factor) -> bool {
            return std::abs(display_info.zoom_factor() - zoom_factor) <=
                   std::numeric_limits<float>::epsilon();
          });
      if (it == zoom_factors.end())
        zoom_factors.push_back(display_info.zoom_factor());

      for (float zoom_factor : zoom_factors) {
        int32_t output_scale = std::round(zoom_factor * 1000.f);
        uint32_t flags = 0;
        if (output_scale == 1000)
          flags |= ZAURA_OUTPUT_SCALE_PROPERTY_PREFERRED;
        if (current_output_scale == output_scale)
          flags |= ZAURA_OUTPUT_SCALE_PROPERTY_CURRENT;

        // TODO(malaykeshav): This can be removed in the future when client
        // has been updated.
        if (wl_resource_get_version(resource_) < 6)
          output_scale = std::round(1000.f / zoom_factor);

        zaura_output_send_scale(resource_, flags, output_scale);
      }
    }

    if (wl_resource_get_version(resource_) >=
        ZAURA_OUTPUT_CONNECTION_SINCE_VERSION) {
      zaura_output_send_connection(resource_,
                                   display.IsInternal()
                                       ? ZAURA_OUTPUT_CONNECTION_TYPE_INTERNAL
                                       : ZAURA_OUTPUT_CONNECTION_TYPE_UNKNOWN);
    }

    if (wl_resource_get_version(resource_) >=
        ZAURA_OUTPUT_DEVICE_SCALE_FACTOR_SINCE_VERSION) {
      zaura_output_send_device_scale_factor(resource_,
                                            display_info.device_scale_factor() *
                                            1000);
    }
  }

 private:
  wl_resource* const resource_;

  DISALLOW_COPY_AND_ASSIGN(AuraOutput);
};

////////////////////////////////////////////////////////////////////////////////
// aura_shell_interface:

void aura_shell_get_aura_surface(wl_client* client,
                                 wl_resource* resource,
                                 uint32_t id,
                                 wl_resource* surface_resource) {
  Surface* surface = GetUserDataAs<Surface>(surface_resource);
  if (surface->GetProperty(kSurfaceHasAuraSurfaceKey)) {
    wl_resource_post_error(
        resource,
        ZAURA_SHELL_ERROR_AURA_SURFACE_EXISTS,
        "an aura surface object for that surface already exists");
    return;
  }

  wl_resource* aura_surface_resource = wl_resource_create(
      client, &zaura_surface_interface, wl_resource_get_version(resource), id);

  SetImplementation(aura_surface_resource, &aura_surface_implementation,
                    std::make_unique<AuraSurface>(surface));
}

void aura_shell_get_aura_output(wl_client* client,
                                wl_resource* resource,
                                uint32_t id,
                                wl_resource* output_resource) {
  WaylandDisplayObserver* display_observer =
      GetUserDataAs<WaylandDisplayObserver>(output_resource);
  if (display_observer->HasScaleObserver()) {
    wl_resource_post_error(
        resource, ZAURA_SHELL_ERROR_AURA_OUTPUT_EXISTS,
        "an aura output object for that output already exists");
    return;
  }

  wl_resource* aura_output_resource = wl_resource_create(
      client, &zaura_output_interface, wl_resource_get_version(resource), id);

  auto aura_output = std::make_unique<AuraOutput>(aura_output_resource);
  display_observer->SetScaleObserver(aura_output->AsWeakPtr());

  SetImplementation(aura_output_resource, nullptr, std::move(aura_output));
}

const struct zaura_shell_interface aura_shell_implementation = {
    aura_shell_get_aura_surface, aura_shell_get_aura_output};

const uint32_t aura_shell_version = 6;

void bind_aura_shell(wl_client* client,
                     void* data,
                     uint32_t version,
                     uint32_t id) {
  wl_resource* resource =
      wl_resource_create(client, &zaura_shell_interface,
                         std::min(version, aura_shell_version), id);

  wl_resource_set_implementation(resource, &aura_shell_implementation,
                                 nullptr, nullptr);
}

////////////////////////////////////////////////////////////////////////////////
// vsync_timing_interface:

// Implements VSync timing interface by monitoring updates to VSync parameters.
class VSyncTiming final : public ui::CompositorVSyncManager::Observer {
 public:
  ~VSyncTiming() override {
    WMHelper::GetInstance()->RemoveVSyncObserver(this);
  }

  static std::unique_ptr<VSyncTiming> Create(wl_resource* timing_resource) {
    std::unique_ptr<VSyncTiming> vsync_timing(new VSyncTiming(timing_resource));
    // Note: AddObserver() will call OnUpdateVSyncParameters.
    WMHelper::GetInstance()->AddVSyncObserver(vsync_timing.get());
    return vsync_timing;
  }

  // Overridden from ui::CompositorVSyncManager::Observer:
  void OnUpdateVSyncParameters(base::TimeTicks timebase,
                               base::TimeDelta interval) override {
    uint64_t timebase_us = timebase.ToInternalValue();
    uint64_t interval_us = interval.ToInternalValue();

    // Ignore updates with interval 0.
    if (!interval_us)
      return;

    uint64_t offset_us = timebase_us % interval_us;

    // Avoid sending update events if interval did not change.
    if (interval_us == last_interval_us_) {
      int64_t offset_delta_us =
          static_cast<int64_t>(last_offset_us_ - offset_us);

      // Reduce the amount of events by only sending an update if the offset
      // changed compared to the last offset sent to the client by this amount.
      const int64_t kOffsetDeltaThresholdInMicroseconds = 25;

      if (std::abs(offset_delta_us) < kOffsetDeltaThresholdInMicroseconds)
        return;
    }

    zcr_vsync_timing_v1_send_update(timing_resource_, timebase_us & 0xffffffff,
                                    timebase_us >> 32, interval_us & 0xffffffff,
                                    interval_us >> 32);
    wl_client_flush(wl_resource_get_client(timing_resource_));

    last_interval_us_ = interval_us;
    last_offset_us_ = offset_us;
  }

 private:
  explicit VSyncTiming(wl_resource* timing_resource)
      : timing_resource_(timing_resource) {}

  // The VSync timing resource.
  wl_resource* const timing_resource_;

  uint64_t last_interval_us_{0};
  uint64_t last_offset_us_{0};

  DISALLOW_COPY_AND_ASSIGN(VSyncTiming);
};

void vsync_timing_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

const struct zcr_vsync_timing_v1_interface vsync_timing_implementation = {
    vsync_timing_destroy};

////////////////////////////////////////////////////////////////////////////////
// vsync_feedback_interface:

void vsync_feedback_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void vsync_feedback_get_vsync_timing(wl_client* client,
                                     wl_resource* resource,
                                     uint32_t id,
                                     wl_resource* output) {
  wl_resource* timing_resource =
      wl_resource_create(client, &zcr_vsync_timing_v1_interface, 1, id);
  SetImplementation(timing_resource, &vsync_timing_implementation,
                    VSyncTiming::Create(timing_resource));
}

const struct zcr_vsync_feedback_v1_interface vsync_feedback_implementation = {
    vsync_feedback_destroy, vsync_feedback_get_vsync_timing};

void bind_vsync_feedback(wl_client* client,
                         void* data,
                         uint32_t version,
                         uint32_t id) {
  wl_resource* resource =
      wl_resource_create(client, &zcr_vsync_feedback_v1_interface, 1, id);

  wl_resource_set_implementation(resource, &vsync_feedback_implementation,
                                 nullptr, nullptr);
}

////////////////////////////////////////////////////////////////////////////////
// wl_data_source_interface:

class WaylandDataSourceDelegate : public DataSourceDelegate {
 public:
  explicit WaylandDataSourceDelegate(wl_resource* source)
      : data_source_resource_(source) {}

  // Overridden from DataSourceDelegate:
  void OnDataSourceDestroying(DataSource* device) override { delete this; }
  void OnTarget(const std::string& mime_type) override {
    wl_data_source_send_target(data_source_resource_, mime_type.c_str());
    wl_client_flush(wl_resource_get_client(data_source_resource_));
  }
  void OnSend(const std::string& mime_type, base::ScopedFD fd) override {
    wl_data_source_send_send(data_source_resource_, mime_type.c_str(),
                             fd.get());
    wl_client_flush(wl_resource_get_client(data_source_resource_));
  }
  void OnCancelled() override {
    wl_data_source_send_cancelled(data_source_resource_);
    wl_client_flush(wl_resource_get_client(data_source_resource_));
  }
  void OnDndDropPerformed() override {
    if (wl_resource_get_version(data_source_resource_) >=
        WL_DATA_SOURCE_DND_DROP_PERFORMED_SINCE_VERSION) {
      wl_data_source_send_dnd_drop_performed(data_source_resource_);
      wl_client_flush(wl_resource_get_client(data_source_resource_));
    }
  }
  void OnDndFinished() override {
    if (wl_resource_get_version(data_source_resource_) >=
        WL_DATA_SOURCE_DND_FINISHED_SINCE_VERSION) {
      wl_data_source_send_dnd_finished(data_source_resource_);
      wl_client_flush(wl_resource_get_client(data_source_resource_));
    }
  }
  void OnAction(DndAction dnd_action) override {
    if (wl_resource_get_version(data_source_resource_) >=
        WL_DATA_SOURCE_ACTION_SINCE_VERSION) {
      wl_data_source_send_action(data_source_resource_,
                                 WaylandDataDeviceManagerDndAction(dnd_action));
      wl_client_flush(wl_resource_get_client(data_source_resource_));
    }
  }

 private:
  wl_resource* const data_source_resource_;

  DISALLOW_COPY_AND_ASSIGN(WaylandDataSourceDelegate);
};

void data_source_offer(wl_client* client,
                       wl_resource* resource,
                       const char* mime_type) {
  GetUserDataAs<DataSource>(resource)->Offer(mime_type);
}

void data_source_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void data_source_set_actions(wl_client* client,
                             wl_resource* resource,
                             uint32_t dnd_actions) {
  GetUserDataAs<DataSource>(resource)->SetActions(
      DataDeviceManagerDndActions(dnd_actions));
}

const struct wl_data_source_interface data_source_implementation = {
    data_source_offer, data_source_destroy, data_source_set_actions};

////////////////////////////////////////////////////////////////////////////////
// wl_data_offer_interface:

class WaylandDataOfferDelegate : public DataOfferDelegate {
 public:
  explicit WaylandDataOfferDelegate(wl_resource* offer)
      : data_offer_resource_(offer) {}

  // Overridden from DataOfferDelegate:
  void OnDataOfferDestroying(DataOffer* device) override { delete this; }
  void OnOffer(const std::string& mime_type) override {
    wl_data_offer_send_offer(data_offer_resource_, mime_type.c_str());
    wl_client_flush(wl_resource_get_client(data_offer_resource_));
  }
  void OnSourceActions(
      const base::flat_set<DndAction>& source_actions) override {
    if (wl_resource_get_version(data_offer_resource_) >=
        WL_DATA_OFFER_SOURCE_ACTIONS_SINCE_VERSION) {
      wl_data_offer_send_source_actions(
          data_offer_resource_,
          WaylandDataDeviceManagerDndActions(source_actions));
      wl_client_flush(wl_resource_get_client(data_offer_resource_));
    }
  }
  void OnAction(DndAction action) override {
    if (wl_resource_get_version(data_offer_resource_) >=
        WL_DATA_OFFER_ACTION_SINCE_VERSION) {
      wl_data_offer_send_action(data_offer_resource_,
                                WaylandDataDeviceManagerDndAction(action));
      wl_client_flush(wl_resource_get_client(data_offer_resource_));
    }
  }

 private:
  wl_resource* const data_offer_resource_;

  DISALLOW_COPY_AND_ASSIGN(WaylandDataOfferDelegate);
};

void data_offer_accept(wl_client* client,
                       wl_resource* resource,
                       uint32_t serial,
                       const char* mime_type) {
  GetUserDataAs<DataOffer>(resource)->Accept(mime_type);
}

void data_offer_receive(wl_client* client,
                        wl_resource* resource,
                        const char* mime_type,
                        int fd) {
  GetUserDataAs<DataOffer>(resource)->Receive(mime_type, base::ScopedFD(fd));
}

void data_offer_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void data_offer_finish(wl_client* client, wl_resource* resource) {
  GetUserDataAs<DataOffer>(resource)->Finish();
}

void data_offer_set_actions(wl_client* client,
                            wl_resource* resource,
                            uint32_t dnd_actions,
                            uint32_t preferred_action) {
  GetUserDataAs<DataOffer>(resource)->SetActions(
      DataDeviceManagerDndActions(dnd_actions),
      DataDeviceManagerDndAction(preferred_action));
}

const struct wl_data_offer_interface data_offer_implementation = {
    data_offer_accept, data_offer_receive, data_offer_finish,
    data_offer_destroy, data_offer_set_actions};

////////////////////////////////////////////////////////////////////////////////
// wl_data_device_interface:

class WaylandDataDeviceDelegate : public DataDeviceDelegate {
 public:
  WaylandDataDeviceDelegate(wl_client* client, wl_resource* device_resource)
      : client_(client), data_device_resource_(device_resource) {}

  // Overridden from DataDeviceDelegate:
  void OnDataDeviceDestroying(DataDevice* device) override { delete this; }
  bool CanAcceptDataEventsForSurface(Surface* surface) override {
    return surface &&
           wl_resource_get_client(GetSurfaceResource(surface)) == client_;
  }
  DataOffer* OnDataOffer() override {
    wl_resource* data_offer_resource =
        wl_resource_create(client_, &wl_data_offer_interface,
                           wl_resource_get_version(data_device_resource_), 0);
    std::unique_ptr<DataOffer> data_offer = std::make_unique<DataOffer>(
        new WaylandDataOfferDelegate(data_offer_resource));
    data_offer->SetProperty(kDataOfferResourceKey, data_offer_resource);
    SetImplementation(data_offer_resource, &data_offer_implementation,
                      std::move(data_offer));

    wl_data_device_send_data_offer(data_device_resource_, data_offer_resource);
    wl_client_flush(client_);

    return GetUserDataAs<DataOffer>(data_offer_resource);
  }
  void OnEnter(Surface* surface,
               const gfx::PointF& point,
               const DataOffer& data_offer) override {
    wl_data_device_send_enter(
        data_device_resource_,
        wl_display_next_serial(wl_client_get_display(client_)),
        GetSurfaceResource(surface), wl_fixed_from_double(point.x()),
        wl_fixed_from_double(point.y()), GetDataOfferResource(&data_offer));
    wl_client_flush(client_);
  }
  void OnLeave() override {
    wl_data_device_send_leave(data_device_resource_);
    wl_client_flush(client_);
  }
  void OnMotion(base::TimeTicks time_stamp, const gfx::PointF& point) override {
    wl_data_device_send_motion(
        data_device_resource_, TimeTicksToMilliseconds(time_stamp),
        wl_fixed_from_double(point.x()), wl_fixed_from_double(point.y()));
    wl_client_flush(client_);
  }
  void OnDrop() override {
    wl_data_device_send_drop(data_device_resource_);
    wl_client_flush(client_);
  }
  void OnSelection(const DataOffer& data_offer) override {
    wl_data_device_send_selection(data_device_resource_,
                                  GetDataOfferResource(&data_offer));
    wl_client_flush(client_);
  }

 private:
  wl_client* const client_;
  wl_resource* const data_device_resource_;

  DISALLOW_COPY_AND_ASSIGN(WaylandDataDeviceDelegate);
};

void data_device_start_drag(wl_client* client,
                            wl_resource* resource,
                            wl_resource* source_resource,
                            wl_resource* origin_resource,
                            wl_resource* icon_resource,
                            uint32_t serial) {
  GetUserDataAs<DataDevice>(resource)->StartDrag(
      source_resource ? GetUserDataAs<DataSource>(source_resource) : nullptr,
      GetUserDataAs<Surface>(origin_resource),
      icon_resource ? GetUserDataAs<Surface>(icon_resource) : nullptr, serial);
}

void data_device_set_selection(wl_client* client,
                               wl_resource* resource,
                               wl_resource* data_source,
                               uint32_t serial) {
  GetUserDataAs<DataDevice>(resource)->SetSelection(
      data_source ? GetUserDataAs<DataSource>(data_source) : nullptr, serial);
}

void data_device_release(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

const struct wl_data_device_interface data_device_implementation = {
    data_device_start_drag, data_device_set_selection, data_device_release};

////////////////////////////////////////////////////////////////////////////////
// wl_data_device_manager_interface:

void data_device_manager_create_data_source(wl_client* client,
                                            wl_resource* resource,
                                            uint32_t id) {
  wl_resource* data_source_resource = wl_resource_create(
      client, &wl_data_source_interface, wl_resource_get_version(resource), id);
  SetImplementation(data_source_resource, &data_source_implementation,
                    std::make_unique<DataSource>(
                        new WaylandDataSourceDelegate(data_source_resource)));
}

void data_device_manager_get_data_device(wl_client* client,
                                         wl_resource* resource,
                                         uint32_t id,
                                         wl_resource* seat_resource) {
  Display* display = GetUserDataAs<Display>(resource);
  wl_resource* data_device_resource = wl_resource_create(
      client, &wl_data_device_interface, wl_resource_get_version(resource), id);
  SetImplementation(data_device_resource, &data_device_implementation,
                    display->CreateDataDevice(new WaylandDataDeviceDelegate(
                        client, data_device_resource)));
}

const struct wl_data_device_manager_interface
    data_device_manager_implementation = {
        data_device_manager_create_data_source,
        data_device_manager_get_data_device};

const uint32_t data_device_manager_version = 3;

void bind_data_device_manager(wl_client* client,
                              void* data,
                              uint32_t version,
                              uint32_t id) {
  wl_resource* resource =
      wl_resource_create(client, &wl_data_device_manager_interface,
                         std::min(version, data_device_manager_version), id);
  wl_resource_set_implementation(resource, &data_device_manager_implementation,
                                 data, nullptr);
}

////////////////////////////////////////////////////////////////////////////////
// wl_pointer_interface:

// Pointer delegate class that accepts events for surfaces owned by the same
// client as a pointer resource.
class WaylandPointerDelegate : public WaylandInputDelegate,
                               public PointerDelegate {
 public:
  explicit WaylandPointerDelegate(wl_resource* pointer_resource)
      : pointer_resource_(pointer_resource) {}

  // Overridden from PointerDelegate:
  void OnPointerDestroying(Pointer* pointer) override { delete this; }
  bool CanAcceptPointerEventsForSurface(Surface* surface) const override {
    wl_resource* surface_resource = GetSurfaceResource(surface);
    // We can accept events for this surface if the client is the same as the
    // pointer.
    return surface_resource &&
           wl_resource_get_client(surface_resource) == client();
  }
  void OnPointerEnter(Surface* surface,
                      const gfx::PointF& location,
                      int button_flags) override {
    wl_resource* surface_resource = GetSurfaceResource(surface);
    DCHECK(surface_resource);
    // Should we be sending button events to the client before the enter event
    // if client's pressed button state is different from |button_flags|?
    wl_pointer_send_enter(pointer_resource_, next_serial(), surface_resource,
                          wl_fixed_from_double(location.x()),
                          wl_fixed_from_double(location.y()));
  }
  void OnPointerLeave(Surface* surface) override {
    wl_resource* surface_resource = GetSurfaceResource(surface);
    DCHECK(surface_resource);
    wl_pointer_send_leave(pointer_resource_, next_serial(), surface_resource);
  }
  void OnPointerMotion(base::TimeTicks time_stamp,
                       const gfx::PointF& location) override {
    SendTimestamp(time_stamp);
    wl_pointer_send_motion(
        pointer_resource_, TimeTicksToMilliseconds(time_stamp),
        wl_fixed_from_double(location.x()), wl_fixed_from_double(location.y()));
  }
  void OnPointerButton(base::TimeTicks time_stamp,
                       int button_flags,
                       bool pressed) override {
    struct {
      ui::EventFlags flag;
      uint32_t value;
    } buttons[] = {
        {ui::EF_LEFT_MOUSE_BUTTON, BTN_LEFT},
        {ui::EF_RIGHT_MOUSE_BUTTON, BTN_RIGHT},
        {ui::EF_MIDDLE_MOUSE_BUTTON, BTN_MIDDLE},
        {ui::EF_FORWARD_MOUSE_BUTTON, BTN_FORWARD},
        {ui::EF_BACK_MOUSE_BUTTON, BTN_BACK},
    };
    uint32_t serial = next_serial();
    for (auto button : buttons) {
      if (button_flags & button.flag) {
        SendTimestamp(time_stamp);
        wl_pointer_send_button(
            pointer_resource_, serial, TimeTicksToMilliseconds(time_stamp),
            button.value, pressed ? WL_POINTER_BUTTON_STATE_PRESSED
                                  : WL_POINTER_BUTTON_STATE_RELEASED);
      }
    }
  }
  void OnPointerScroll(base::TimeTicks time_stamp,
                       const gfx::Vector2dF& offset,
                       bool discrete) override {
    // Same as Weston, the reference compositor.
    const double kAxisStepDistance = 10.0 / ui::MouseWheelEvent::kWheelDelta;

    if (wl_resource_get_version(pointer_resource_) >=
        WL_POINTER_AXIS_SOURCE_SINCE_VERSION) {
      int32_t axis_source = discrete ? WL_POINTER_AXIS_SOURCE_WHEEL
                                     : WL_POINTER_AXIS_SOURCE_FINGER;
      wl_pointer_send_axis_source(pointer_resource_, axis_source);
    }

    double x_value = offset.x() * kAxisStepDistance;
    SendTimestamp(time_stamp);
    wl_pointer_send_axis(pointer_resource_, TimeTicksToMilliseconds(time_stamp),
                         WL_POINTER_AXIS_HORIZONTAL_SCROLL,
                         wl_fixed_from_double(-x_value));

    double y_value = offset.y() * kAxisStepDistance;
    SendTimestamp(time_stamp);
    wl_pointer_send_axis(pointer_resource_, TimeTicksToMilliseconds(time_stamp),
                         WL_POINTER_AXIS_VERTICAL_SCROLL,
                         wl_fixed_from_double(-y_value));
  }
  void OnPointerScrollStop(base::TimeTicks time_stamp) override {
    if (wl_resource_get_version(pointer_resource_) >=
        WL_POINTER_AXIS_STOP_SINCE_VERSION) {
      SendTimestamp(time_stamp);
      wl_pointer_send_axis_stop(pointer_resource_,
                                TimeTicksToMilliseconds(time_stamp),
                                WL_POINTER_AXIS_HORIZONTAL_SCROLL);
      SendTimestamp(time_stamp);
      wl_pointer_send_axis_stop(pointer_resource_,
                                TimeTicksToMilliseconds(time_stamp),
                                WL_POINTER_AXIS_VERTICAL_SCROLL);
    }
  }
  void OnPointerFrame() override {
    if (wl_resource_get_version(pointer_resource_) >=
        WL_POINTER_FRAME_SINCE_VERSION) {
      wl_pointer_send_frame(pointer_resource_);
    }
    wl_client_flush(client());
  }

 private:
  // The client who own this pointer instance.
  wl_client* client() const {
    return wl_resource_get_client(pointer_resource_);
  }

  // Returns the next serial to use for pointer events.
  uint32_t next_serial() const {
    return wl_display_next_serial(wl_client_get_display(client()));
  }

  // The pointer resource associated with the pointer.
  wl_resource* const pointer_resource_;

  DISALLOW_COPY_AND_ASSIGN(WaylandPointerDelegate);
};

void pointer_set_cursor(wl_client* client,
                        wl_resource* resource,
                        uint32_t serial,
                        wl_resource* surface_resource,
                        int32_t hotspot_x,
                        int32_t hotspot_y) {
  GetUserDataAs<Pointer>(resource)->SetCursor(
      surface_resource ? GetUserDataAs<Surface>(surface_resource) : nullptr,
      gfx::Point(hotspot_x, hotspot_y));
}

void pointer_release(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

const struct wl_pointer_interface pointer_implementation = {pointer_set_cursor,
                                                            pointer_release};


////////////////////////////////////////////////////////////////////////////////
// wl_keyboard_interface:

// Keyboard delegate class that accepts events for surfaces owned by the same
// client as a keyboard resource.
class WaylandKeyboardDelegate : public WaylandInputDelegate,
                                public KeyboardDelegate,
                                public KeyboardObserver
#if defined(OS_CHROMEOS)
    ,
                                public ash::ImeController::Observer
#endif
{
#if BUILDFLAG(USE_XKBCOMMON)
 public:
  explicit WaylandKeyboardDelegate(wl_resource* keyboard_resource)
      : keyboard_resource_(keyboard_resource),
        xkb_context_(xkb_context_new(XKB_CONTEXT_NO_FLAGS)) {
#if defined(OS_CHROMEOS)
    ash::ImeController* ime_controller = ash::Shell::Get()->ime_controller();
    ime_controller->AddObserver(this);
    SendNamedLayout(ime_controller->keyboard_layout_name());
#else
    SendLayout(nullptr);
#endif
  }
#if defined(OS_CHROMEOS)
  ~WaylandKeyboardDelegate() override {
    ash::Shell::Get()->ime_controller()->RemoveObserver(this);
  }
#endif

  // Overridden from KeyboardDelegate:
  void OnKeyboardDestroying(Keyboard* keyboard) override { delete this; }
  bool CanAcceptKeyboardEventsForSurface(Surface* surface) const override {
    wl_resource* surface_resource = GetSurfaceResource(surface);
    // We can accept events for this surface if the client is the same as the
    // keyboard.
    return surface_resource &&
           wl_resource_get_client(surface_resource) == client();
  }
  void OnKeyboardEnter(
      Surface* surface,
      const base::flat_map<ui::DomCode, ui::DomCode>& pressed_keys) override {
    wl_resource* surface_resource = GetSurfaceResource(surface);
    DCHECK(surface_resource);
    wl_array keys;
    wl_array_init(&keys);
    for (const auto& entry : pressed_keys) {
      uint32_t* value =
          static_cast<uint32_t*>(wl_array_add(&keys, sizeof(uint32_t)));
      DCHECK(value);
      *value = DomCodeToKey(entry.second);
    }
    wl_keyboard_send_enter(keyboard_resource_, next_serial(), surface_resource,
                           &keys);
    wl_array_release(&keys);
    wl_client_flush(client());
  }
  void OnKeyboardLeave(Surface* surface) override {
    wl_resource* surface_resource = GetSurfaceResource(surface);
    DCHECK(surface_resource);
    wl_keyboard_send_leave(keyboard_resource_, next_serial(), surface_resource);
    wl_client_flush(client());
  }
  uint32_t OnKeyboardKey(base::TimeTicks time_stamp,
                         ui::DomCode key,
                         bool pressed) override {
    uint32_t serial = next_serial();
    SendTimestamp(time_stamp);
    wl_keyboard_send_key(keyboard_resource_, serial,
                         TimeTicksToMilliseconds(time_stamp), DomCodeToKey(key),
                         pressed ? WL_KEYBOARD_KEY_STATE_PRESSED
                                 : WL_KEYBOARD_KEY_STATE_RELEASED);
    wl_client_flush(client());
    return serial;
  }
  void OnKeyboardModifiers(int modifier_flags) override {
    xkb_state_update_mask(xkb_state_.get(),
                          ModifierFlagsToXkbModifiers(modifier_flags), 0, 0, 0,
                          0, 0);
    wl_keyboard_send_modifiers(
        keyboard_resource_, next_serial(),
        xkb_state_serialize_mods(xkb_state_.get(), XKB_STATE_MODS_DEPRESSED),
        xkb_state_serialize_mods(xkb_state_.get(), XKB_STATE_MODS_LOCKED),
        xkb_state_serialize_mods(xkb_state_.get(), XKB_STATE_MODS_LATCHED),
        xkb_state_serialize_layout(xkb_state_.get(),
                                   XKB_STATE_LAYOUT_EFFECTIVE));
    wl_client_flush(client());
  }

#if defined(OS_CHROMEOS)
  // Overridden from ImeController::Observer:
  void OnCapsLockChanged(bool enabled) override {}
  void OnKeyboardLayoutNameChanged(const std::string& layout_name) override {
    SendNamedLayout(layout_name);
  }
#endif

 private:
  // Returns the corresponding key given a dom code.
  uint32_t DomCodeToKey(ui::DomCode code) const {
    // This assumes KeycodeConverter has been built with evdev/xkb codes.
    xkb_keycode_t xkb_keycode = static_cast<xkb_keycode_t>(
        ui::KeycodeConverter::DomCodeToNativeKeycode(code));

    // Keycodes are offset by 8 in Xkb.
    DCHECK_GE(xkb_keycode, 8u);
    return xkb_keycode - 8;
  }

  // Returns a set of Xkb modififers given a set of modifier flags.
  uint32_t ModifierFlagsToXkbModifiers(int modifier_flags) {
    struct {
      ui::EventFlags flag;
      const char* xkb_name;
    } modifiers[] = {
        {ui::EF_SHIFT_DOWN, XKB_MOD_NAME_SHIFT},
        {ui::EF_CONTROL_DOWN, XKB_MOD_NAME_CTRL},
        {ui::EF_ALT_DOWN, XKB_MOD_NAME_ALT},
        {ui::EF_COMMAND_DOWN, XKB_MOD_NAME_LOGO},
        {ui::EF_ALTGR_DOWN, "Mod5"},
        {ui::EF_MOD3_DOWN, "Mod3"},
        {ui::EF_NUM_LOCK_ON, XKB_MOD_NAME_NUM},
        {ui::EF_CAPS_LOCK_ON, XKB_MOD_NAME_CAPS},
    };
    uint32_t xkb_modifiers = 0;
    for (auto modifier : modifiers) {
      if (modifier_flags & modifier.flag) {
        xkb_modifiers |=
            1 << xkb_keymap_mod_get_index(xkb_keymap_.get(), modifier.xkb_name);
      }
    }
    return xkb_modifiers;
  }

#if defined(OS_CHROMEOS)
  // Send the named keyboard layout to the client.
  void SendNamedLayout(const std::string& layout_name) {
    std::string layout_id, layout_variant;
    ui::XkbKeyboardLayoutEngine::ParseLayoutName(layout_name, &layout_id,
                                                 &layout_variant);
    xkb_rule_names names = {.rules = nullptr,
                            .model = "pc101",
                            .layout = layout_id.c_str(),
                            .variant = layout_variant.c_str(),
                            .options = ""};
    SendLayout(&names);
  }
#endif

  // Send the keyboard layout named by XKB rules to the client.
  void SendLayout(const xkb_rule_names* names) {
    xkb_keymap_.reset(xkb_keymap_new_from_names(xkb_context_.get(), names,
                                                XKB_KEYMAP_COMPILE_NO_FLAGS));
    xkb_state_.reset(xkb_state_new(xkb_keymap_.get()));
    std::unique_ptr<char, base::FreeDeleter> keymap_string(
        xkb_keymap_get_as_string(xkb_keymap_.get(), XKB_KEYMAP_FORMAT_TEXT_V1));
    DCHECK(keymap_string.get());
    size_t keymap_size = strlen(keymap_string.get()) + 1;
    base::SharedMemory shared_keymap;
    bool rv = shared_keymap.CreateAndMapAnonymous(keymap_size);
    DCHECK(rv);
    memcpy(shared_keymap.memory(), keymap_string.get(), keymap_size);
    wl_keyboard_send_keymap(keyboard_resource_,
                            WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1,
                            shared_keymap.handle().GetHandle(), keymap_size);
    wl_client_flush(client());
  }

  // The client who own this keyboard instance.
  wl_client* client() const {
    return wl_resource_get_client(keyboard_resource_);
  }

  // Returns the next serial to use for keyboard events.
  uint32_t next_serial() const {
    return wl_display_next_serial(wl_client_get_display(client()));
  }

  // The keyboard resource associated with the keyboard.
  wl_resource* const keyboard_resource_;

  // The Xkb state used for the keyboard.
  std::unique_ptr<xkb_context, ui::XkbContextDeleter> xkb_context_;
  std::unique_ptr<xkb_keymap, ui::XkbKeymapDeleter> xkb_keymap_;
  std::unique_ptr<xkb_state, ui::XkbStateDeleter> xkb_state_;

  DISALLOW_COPY_AND_ASSIGN(WaylandKeyboardDelegate);
#endif
};

#if BUILDFLAG(USE_XKBCOMMON)

void keyboard_release(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

const struct wl_keyboard_interface keyboard_implementation = {keyboard_release};

#endif

////////////////////////////////////////////////////////////////////////////////
// wl_touch_interface:

// Touch delegate class that accepts events for surfaces owned by the same
// client as a touch resource.
class WaylandTouchDelegate : public WaylandInputDelegate, public TouchDelegate {
 public:
  explicit WaylandTouchDelegate(wl_resource* touch_resource)
      : touch_resource_(touch_resource) {}

  // Overridden from TouchDelegate:
  void OnTouchDestroying(Touch* touch) override { delete this; }
  bool CanAcceptTouchEventsForSurface(Surface* surface) const override {
    wl_resource* surface_resource = GetSurfaceResource(surface);
    // We can accept events for this surface if the client is the same as the
    // touch resource.
    return surface_resource &&
           wl_resource_get_client(surface_resource) == client();
  }
  void OnTouchDown(Surface* surface,
                   base::TimeTicks time_stamp,
                   int id,
                   const gfx::PointF& location) override {
    wl_resource* surface_resource = GetSurfaceResource(surface);
    DCHECK(surface_resource);
    SendTimestamp(time_stamp);
    wl_touch_send_down(touch_resource_, next_serial(),
                       TimeTicksToMilliseconds(time_stamp), surface_resource,
                       id, wl_fixed_from_double(location.x()),
                       wl_fixed_from_double(location.y()));
  }
  void OnTouchUp(base::TimeTicks time_stamp, int id) override {
    SendTimestamp(time_stamp);
    wl_touch_send_up(touch_resource_, next_serial(),
                     TimeTicksToMilliseconds(time_stamp), id);
  }
  void OnTouchMotion(base::TimeTicks time_stamp,
                     int id,
                     const gfx::PointF& location) override {
    SendTimestamp(time_stamp);
    wl_touch_send_motion(touch_resource_, TimeTicksToMilliseconds(time_stamp),
                         id, wl_fixed_from_double(location.x()),
                         wl_fixed_from_double(location.y()));
  }
  void OnTouchShape(int id, float major, float minor) override {
    if (wl_resource_get_version(touch_resource_) >=
        WL_TOUCH_SHAPE_SINCE_VERSION) {
      wl_touch_send_shape(touch_resource_, id, wl_fixed_from_double(major),
                          wl_fixed_from_double(minor));
    }
  }
  void OnTouchFrame() override {
    if (wl_resource_get_version(touch_resource_) >=
        WL_TOUCH_FRAME_SINCE_VERSION) {
      wl_touch_send_frame(touch_resource_);
    }
    wl_client_flush(client());
  }
  void OnTouchCancel() override {
    wl_touch_send_cancel(touch_resource_);
  }

 private:
  // The client who own this touch instance.
  wl_client* client() const { return wl_resource_get_client(touch_resource_); }

  // Returns the next serial to use for keyboard events.
  uint32_t next_serial() const {
    return wl_display_next_serial(wl_client_get_display(client()));
  }

  // The touch resource associated with the touch.
  wl_resource* const touch_resource_;

  DISALLOW_COPY_AND_ASSIGN(WaylandTouchDelegate);
};

void touch_release(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

const struct wl_touch_interface touch_implementation = {touch_release};

////////////////////////////////////////////////////////////////////////////////
// wl_seat_interface:

void seat_get_pointer(wl_client* client, wl_resource* resource, uint32_t id) {
  wl_resource* pointer_resource = wl_resource_create(
      client, &wl_pointer_interface, wl_resource_get_version(resource), id);

  SetImplementation(
      pointer_resource, &pointer_implementation,
      std::make_unique<Pointer>(new WaylandPointerDelegate(pointer_resource)));
}

void seat_get_keyboard(wl_client* client, wl_resource* resource, uint32_t id) {
#if BUILDFLAG(USE_XKBCOMMON)
  uint32_t version = wl_resource_get_version(resource);
  wl_resource* keyboard_resource =
      wl_resource_create(client, &wl_keyboard_interface, version, id);

  WaylandKeyboardDelegate* delegate =
      new WaylandKeyboardDelegate(keyboard_resource);
  std::unique_ptr<Keyboard> keyboard =
      std::make_unique<Keyboard>(delegate, GetUserDataAs<Seat>(resource));
  keyboard->AddObserver(delegate);
  SetImplementation(keyboard_resource, &keyboard_implementation,
                    std::move(keyboard));

  // TODO(reveman): Keep repeat info synchronized with chromium and the host OS.
  if (version >= WL_KEYBOARD_REPEAT_INFO_SINCE_VERSION)
    wl_keyboard_send_repeat_info(keyboard_resource, 40, 500);
#else
  NOTIMPLEMENTED();
#endif
}

void seat_get_touch(wl_client* client, wl_resource* resource, uint32_t id) {
  wl_resource* touch_resource = wl_resource_create(
      client, &wl_touch_interface, wl_resource_get_version(resource), id);

  SetImplementation(
      touch_resource, &touch_implementation,
      std::make_unique<Touch>(new WaylandTouchDelegate(touch_resource)));
}

void seat_release(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

const struct wl_seat_interface seat_implementation = {
    seat_get_pointer, seat_get_keyboard, seat_get_touch, seat_release};

const uint32_t seat_version = 6;

void bind_seat(wl_client* client, void* data, uint32_t version, uint32_t id) {
  wl_resource* resource = wl_resource_create(
      client, &wl_seat_interface, std::min(version, seat_version), id);

  wl_resource_set_implementation(resource, &seat_implementation, data, nullptr);

  if (version >= WL_SEAT_NAME_SINCE_VERSION)
    wl_seat_send_name(resource, "default");

  uint32_t capabilities = WL_SEAT_CAPABILITY_POINTER | WL_SEAT_CAPABILITY_TOUCH;
#if BUILDFLAG(USE_XKBCOMMON)
  capabilities |= WL_SEAT_CAPABILITY_KEYBOARD;
#endif
  wl_seat_send_capabilities(resource, capabilities);
}

////////////////////////////////////////////////////////////////////////////////
// wp_viewport_interface:

// Implements the viewport interface to a Surface. The "viewport"-state is set
// to null upon destruction. A window property will be set during the lifetime
// of this class to prevent multiple instances from being created for the same
// Surface.
class Viewport : public SurfaceObserver {
 public:
  explicit Viewport(Surface* surface) : surface_(surface) {
    surface_->AddSurfaceObserver(this);
    surface_->SetProperty(kSurfaceHasViewportKey, true);
  }
  ~Viewport() override {
    if (surface_) {
      surface_->RemoveSurfaceObserver(this);
      surface_->SetCrop(gfx::RectF());
      surface_->SetViewport(gfx::Size());
      surface_->SetProperty(kSurfaceHasViewportKey, false);
    }
  }

  void SetSource(const gfx::RectF& rect) {
    if (surface_)
      surface_->SetCrop(rect);
  }

  void SetDestination(const gfx::Size& size) {
    if (surface_)
      surface_->SetViewport(size);
  }

  // Overridden from SurfaceObserver:
  void OnSurfaceDestroying(Surface* surface) override {
    surface->RemoveSurfaceObserver(this);
    surface_ = nullptr;
  }

 private:
  Surface* surface_;

  DISALLOW_COPY_AND_ASSIGN(Viewport);
};

void viewport_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void viewport_set_source(wl_client* client,
                         wl_resource* resource,
                         wl_fixed_t x,
                         wl_fixed_t y,
                         wl_fixed_t width,
                         wl_fixed_t height) {
  if (x == wl_fixed_from_int(-1) && y == wl_fixed_from_int(-1) &&
      width == wl_fixed_from_int(-1) && height == wl_fixed_from_int(-1)) {
    GetUserDataAs<Viewport>(resource)->SetSource(gfx::RectF());
    return;
  }

  if (x < 0 || y < 0 || width <= 0 || height <= 0) {
    wl_resource_post_error(resource, WP_VIEWPORT_ERROR_BAD_VALUE,
                           "source rectangle must be non-empty (%dx%d) and"
                           "have positive origin (%d,%d)",
                           width, height, x, y);
    return;
  }

  GetUserDataAs<Viewport>(resource)->SetSource(
      gfx::RectF(wl_fixed_to_double(x), wl_fixed_to_double(y),
                 wl_fixed_to_double(width), wl_fixed_to_double(height)));
}

void viewport_set_destination(wl_client* client,
                              wl_resource* resource,
                              int32_t width,
                              int32_t height) {
  if (width == -1 && height == -1) {
    GetUserDataAs<Viewport>(resource)->SetDestination(gfx::Size());
    return;
  }

  if (width <= 0 || height <= 0) {
    wl_resource_post_error(resource, WP_VIEWPORT_ERROR_BAD_VALUE,
                           "destination size must be positive (%dx%d)", width,
                           height);
    return;
  }

  GetUserDataAs<Viewport>(resource)->SetDestination(gfx::Size(width, height));
}

const struct wp_viewport_interface viewport_implementation = {
    viewport_destroy, viewport_set_source, viewport_set_destination};

////////////////////////////////////////////////////////////////////////////////
// wp_viewporter_interface:

void viewporter_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void viewporter_get_viewport(wl_client* client,
                             wl_resource* resource,
                             uint32_t id,
                             wl_resource* surface_resource) {
  Surface* surface = GetUserDataAs<Surface>(surface_resource);
  if (surface->GetProperty(kSurfaceHasViewportKey)) {
    wl_resource_post_error(resource, WP_VIEWPORTER_ERROR_VIEWPORT_EXISTS,
                           "a viewport for that surface already exists");
    return;
  }

  wl_resource* viewport_resource = wl_resource_create(
      client, &wp_viewport_interface, wl_resource_get_version(resource), id);

  SetImplementation(viewport_resource, &viewport_implementation,
                    std::make_unique<Viewport>(surface));
}

const struct wp_viewporter_interface viewporter_implementation = {
    viewporter_destroy, viewporter_get_viewport};

void bind_viewporter(wl_client* client,
                     void* data,
                     uint32_t version,
                     uint32_t id) {
  wl_resource* resource =
      wl_resource_create(client, &wp_viewporter_interface, 1, id);

  wl_resource_set_implementation(resource, &viewporter_implementation, data,
                                 nullptr);
}

////////////////////////////////////////////////////////////////////////////////
// presentation_interface:

void HandleSurfacePresentationCallback(
    wl_resource* resource,
    const gfx::PresentationFeedback& feedback) {
  if (feedback.timestamp.is_null()) {
    wp_presentation_feedback_send_discarded(resource);
  } else {
    int64_t presentation_time_us = feedback.timestamp.ToInternalValue();
    int64_t seconds = presentation_time_us / base::Time::kMicrosecondsPerSecond;
    int64_t microseconds =
        presentation_time_us % base::Time::kMicrosecondsPerSecond;
    static_assert(
        static_cast<uint32_t>(gfx::PresentationFeedback::Flags::kVSync) ==
            static_cast<uint32_t>(WP_PRESENTATION_FEEDBACK_KIND_VSYNC),
        "gfx::PresentationFlags::VSync don't match!");
    static_assert(
        static_cast<uint32_t>(gfx::PresentationFeedback::Flags::kHWClock) ==
            static_cast<uint32_t>(WP_PRESENTATION_FEEDBACK_KIND_HW_CLOCK),
        "gfx::PresentationFlags::HWClock don't match!");
    static_assert(
        static_cast<uint32_t>(
            gfx::PresentationFeedback::Flags::kHWCompletion) ==
            static_cast<uint32_t>(WP_PRESENTATION_FEEDBACK_KIND_HW_COMPLETION),
        "gfx::PresentationFlags::HWCompletion don't match!");
    static_assert(
        static_cast<uint32_t>(gfx::PresentationFeedback::Flags::kZeroCopy) ==
            static_cast<uint32_t>(WP_PRESENTATION_FEEDBACK_KIND_ZERO_COPY),
        "gfx::PresentationFlags::ZeroCopy don't match!");
    wp_presentation_feedback_send_presented(
        resource, seconds >> 32, seconds & 0xffffffff,
        microseconds * base::Time::kNanosecondsPerMicrosecond,
        feedback.interval.InMicroseconds() *
            base::Time::kNanosecondsPerMicrosecond,
        0, 0, feedback.flags);
  }
  wl_client_flush(wl_resource_get_client(resource));
}

void presentation_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void presentation_feedback(wl_client* client,
                           wl_resource* resource,
                           wl_resource* surface_resource,
                           uint32_t id) {
  wl_resource* presentation_feedback_resource =
      wl_resource_create(client, &wp_presentation_feedback_interface,
                         wl_resource_get_version(resource), id);

  // base::Unretained is safe as the resource owns the callback.
  auto cancelable_callback = std::make_unique<
      base::CancelableCallback<void(const gfx::PresentationFeedback&)>>(
      base::Bind(&HandleSurfacePresentationCallback,
                 base::Unretained(presentation_feedback_resource)));

  GetUserDataAs<Surface>(surface_resource)
      ->RequestPresentationCallback(cancelable_callback->callback());

  SetImplementation(presentation_feedback_resource, nullptr,
                    std::move(cancelable_callback));
}

const struct wp_presentation_interface presentation_implementation = {
    presentation_destroy, presentation_feedback};

void bind_presentation(wl_client* client,
                       void* data,
                       uint32_t version,
                       uint32_t id) {
  wl_resource* resource =
      wl_resource_create(client, &wp_presentation_interface, 1, id);

  wl_resource_set_implementation(resource, &presentation_implementation, data,
                                 nullptr);

  wp_presentation_send_clock_id(resource, CLOCK_MONOTONIC);
}

////////////////////////////////////////////////////////////////////////////////
// security_interface:

// Implements the security interface to a Surface. The "only visible on secure
// output"-state is set to false upon destruction. A window property will be set
// during the lifetime of this class to prevent multiple instances from being
// created for the same Surface.
class Security : public SurfaceObserver {
 public:
  explicit Security(Surface* surface) : surface_(surface) {
    surface_->AddSurfaceObserver(this);
    surface_->SetProperty(kSurfaceHasSecurityKey, true);
  }
  ~Security() override {
    if (surface_) {
      surface_->RemoveSurfaceObserver(this);
      surface_->SetOnlyVisibleOnSecureOutput(false);
      surface_->SetProperty(kSurfaceHasSecurityKey, false);
    }
  }

  void OnlyVisibleOnSecureOutput() {
    if (surface_)
      surface_->SetOnlyVisibleOnSecureOutput(true);
  }

  // Overridden from SurfaceObserver:
  void OnSurfaceDestroying(Surface* surface) override {
    surface->RemoveSurfaceObserver(this);
    surface_ = nullptr;
  }

 private:
  Surface* surface_;

  DISALLOW_COPY_AND_ASSIGN(Security);
};

void security_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void security_only_visible_on_secure_output(wl_client* client,
                                            wl_resource* resource) {
  GetUserDataAs<Security>(resource)->OnlyVisibleOnSecureOutput();
}

const struct zcr_security_v1_interface security_implementation = {
    security_destroy, security_only_visible_on_secure_output};

////////////////////////////////////////////////////////////////////////////////
// secure_output_interface:

void secure_output_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void secure_output_get_security(wl_client* client,
                                wl_resource* resource,
                                uint32_t id,
                                wl_resource* surface_resource) {
  Surface* surface = GetUserDataAs<Surface>(surface_resource);
  if (surface->GetProperty(kSurfaceHasSecurityKey)) {
    wl_resource_post_error(resource, ZCR_SECURE_OUTPUT_V1_ERROR_SECURITY_EXISTS,
                           "a security object for that surface already exists");
    return;
  }

  wl_resource* security_resource =
      wl_resource_create(client, &zcr_security_v1_interface, 1, id);

  SetImplementation(security_resource, &security_implementation,
                    std::make_unique<Security>(surface));
}

const struct zcr_secure_output_v1_interface secure_output_implementation = {
    secure_output_destroy, secure_output_get_security};

void bind_secure_output(wl_client* client,
                        void* data,
                        uint32_t version,
                        uint32_t id) {
  wl_resource* resource =
      wl_resource_create(client, &zcr_secure_output_v1_interface, 1, id);

  wl_resource_set_implementation(resource, &secure_output_implementation, data,
                                 nullptr);
}

////////////////////////////////////////////////////////////////////////////////
// blending_interface:

// Implements the blending interface to a Surface. The "blend mode" and
// "alpha"-state is set to SrcOver and 1 upon destruction. A window property
// will be set during the lifetime of this class to prevent multiple instances
// from being created for the same Surface.
class Blending : public SurfaceObserver {
 public:
  explicit Blending(Surface* surface) : surface_(surface) {
    surface_->AddSurfaceObserver(this);
    surface_->SetProperty(kSurfaceHasBlendingKey, true);
  }
  ~Blending() override {
    if (surface_) {
      surface_->RemoveSurfaceObserver(this);
      surface_->SetBlendMode(SkBlendMode::kSrcOver);
      surface_->SetAlpha(1.0f);
      surface_->SetProperty(kSurfaceHasBlendingKey, false);
    }
  }

  void SetBlendMode(SkBlendMode blend_mode) {
    if (surface_)
      surface_->SetBlendMode(blend_mode);
  }

  void SetAlpha(float value) {
    if (surface_)
      surface_->SetAlpha(value);
  }

  // Overridden from SurfaceObserver:
  void OnSurfaceDestroying(Surface* surface) override {
    surface->RemoveSurfaceObserver(this);
    surface_ = nullptr;
  }

 private:
  Surface* surface_;

  DISALLOW_COPY_AND_ASSIGN(Blending);
};

void blending_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void blending_set_blending(wl_client* client,
                           wl_resource* resource,
                           uint32_t equation) {
  switch (equation) {
    case ZCR_BLENDING_V1_BLENDING_EQUATION_NONE:
      GetUserDataAs<Blending>(resource)->SetBlendMode(SkBlendMode::kSrc);
      break;
    case ZCR_BLENDING_V1_BLENDING_EQUATION_PREMULT:
      GetUserDataAs<Blending>(resource)->SetBlendMode(SkBlendMode::kSrcOver);
      break;
    case ZCR_BLENDING_V1_BLENDING_EQUATION_COVERAGE:
      NOTIMPLEMENTED();
      break;
    default:
      DLOG(WARNING) << "Unsupported blending equation: " << equation;
      break;
  }
}

void blending_set_alpha(wl_client* client,
                        wl_resource* resource,
                        wl_fixed_t alpha) {
  GetUserDataAs<Blending>(resource)->SetAlpha(wl_fixed_to_double(alpha));
}

const struct zcr_blending_v1_interface blending_implementation = {
    blending_destroy, blending_set_blending, blending_set_alpha};

////////////////////////////////////////////////////////////////////////////////
// alpha_compositing_interface:

void alpha_compositing_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void alpha_compositing_get_blending(wl_client* client,
                                    wl_resource* resource,
                                    uint32_t id,
                                    wl_resource* surface_resource) {
  Surface* surface = GetUserDataAs<Surface>(surface_resource);
  if (surface->GetProperty(kSurfaceHasBlendingKey)) {
    wl_resource_post_error(resource,
                           ZCR_ALPHA_COMPOSITING_V1_ERROR_BLENDING_EXISTS,
                           "a blending object for that surface already exists");
    return;
  }

  wl_resource* blending_resource =
      wl_resource_create(client, &zcr_blending_v1_interface, 1, id);

  SetImplementation(blending_resource, &blending_implementation,
                    std::make_unique<Blending>(surface));
}

const struct zcr_alpha_compositing_v1_interface
    alpha_compositing_implementation = {alpha_compositing_destroy,
                                        alpha_compositing_get_blending};

void bind_alpha_compositing(wl_client* client,
                            void* data,
                            uint32_t version,
                            uint32_t id) {
  wl_resource* resource =
      wl_resource_create(client, &zcr_alpha_compositing_v1_interface, 1, id);

  wl_resource_set_implementation(resource, &alpha_compositing_implementation,
                                 data, nullptr);
}

////////////////////////////////////////////////////////////////////////////////
// gaming_input_interface:

// Gamepad delegate class that forwards gamepad events to the client resource.
class WaylandGamepadDelegate : public GamepadDelegate {
 public:
  explicit WaylandGamepadDelegate(wl_resource* gamepad_resource)
      : gamepad_resource_(gamepad_resource) {}

  // If gamepad_resource_ is destroyed first, ResetGamepadResource will
  // be called to remove the resource from delegate, and delegate won't
  // do anything after that. If delegate is destructed first, it will
  // set the data to null in the gamepad_resource_, then the resource
  // destroy won't reset the delegate (cause it's gone).
  static void ResetGamepadResource(wl_resource* resource) {
    WaylandGamepadDelegate* delegate =
        GetUserDataAs<WaylandGamepadDelegate>(resource);
    if (delegate) {
      delegate->gamepad_resource_ = nullptr;
    }
  }

  // Override from GamepadDelegate:
  void OnRemoved() override {
    if (!gamepad_resource_) {
      return;
    }
    zcr_gamepad_v2_send_removed(gamepad_resource_);
    wl_client_flush(client());
    // Reset the user data in gamepad_resource.
    wl_resource_set_user_data(gamepad_resource_, nullptr);
    delete this;
  }
  void OnAxis(int axis, double value) override {
    if (!gamepad_resource_) {
      return;
    }
    zcr_gamepad_v2_send_axis(gamepad_resource_, NowInMilliseconds(), axis,
                             wl_fixed_from_double(value));
  }
  void OnButton(int button, bool pressed, double value) override {
    if (!gamepad_resource_) {
      return;
    }
    uint32_t state = pressed ? ZCR_GAMEPAD_V2_BUTTON_STATE_PRESSED
                             : ZCR_GAMEPAD_V2_BUTTON_STATE_RELEASED;
    zcr_gamepad_v2_send_button(gamepad_resource_, NowInMilliseconds(), button,
                               state, wl_fixed_from_double(value));
  }
  void OnFrame() override {
    if (!gamepad_resource_) {
      return;
    }
    zcr_gamepad_v2_send_frame(gamepad_resource_, NowInMilliseconds());
    wl_client_flush(client());
  }

 private:
  // The object should be deleted by OnRemoved().
  ~WaylandGamepadDelegate() override {}

  // The client who own this gamepad instance.
  wl_client* client() const {
    return wl_resource_get_client(gamepad_resource_);
  }

  // The gamepad resource associated with the gamepad.
  wl_resource* gamepad_resource_;

  DISALLOW_COPY_AND_ASSIGN(WaylandGamepadDelegate);
};

void gamepad_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

const struct zcr_gamepad_v2_interface gamepad_implementation = {
    gamepad_destroy};

// GamingSeat delegate that provide gamepad added.
class WaylandGamingSeatDelegate : public GamingSeatDelegate {
 public:
  explicit WaylandGamingSeatDelegate(wl_resource* gaming_seat_resource)
      : gaming_seat_resource_{gaming_seat_resource} {}

  // Override from GamingSeatDelegate:
  void OnGamingSeatDestroying(GamingSeat*) override { delete this; }
  bool CanAcceptGamepadEventsForSurface(Surface* surface) const override {
    wl_resource* surface_resource = GetSurfaceResource(surface);
    return surface_resource &&
           wl_resource_get_client(surface_resource) ==
               wl_resource_get_client(gaming_seat_resource_);
  }
  GamepadDelegate* GamepadAdded() override {
    wl_resource* gamepad_resource =
        wl_resource_create(wl_resource_get_client(gaming_seat_resource_),
                           &zcr_gamepad_v2_interface,
                           wl_resource_get_version(gaming_seat_resource_), 0);

    GamepadDelegate* gamepad_delegate =
        new WaylandGamepadDelegate(gamepad_resource);

    wl_resource_set_implementation(
        gamepad_resource, &gamepad_implementation, gamepad_delegate,
        &WaylandGamepadDelegate::ResetGamepadResource);

    zcr_gaming_seat_v2_send_gamepad_added(gaming_seat_resource_,
                                          gamepad_resource);
    wl_client_flush(wl_resource_get_client(gaming_seat_resource_));

    return gamepad_delegate;
  }

 private:
  // The gaming seat resource associated with the gaming seat.
  wl_resource* const gaming_seat_resource_;

  DISALLOW_COPY_AND_ASSIGN(WaylandGamingSeatDelegate);
};

void gaming_seat_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

const struct zcr_gaming_seat_v2_interface gaming_seat_implementation = {
    gaming_seat_destroy};

void gaming_input_get_gaming_seat(wl_client* client,
                                  wl_resource* resource,
                                  uint32_t id,
                                  wl_resource* seat) {
  wl_resource* gaming_seat_resource =
      wl_resource_create(client, &zcr_gaming_seat_v2_interface,
                         wl_resource_get_version(resource), id);

  SetImplementation(gaming_seat_resource, &gaming_seat_implementation,
                    std::make_unique<GamingSeat>(
                        new WaylandGamingSeatDelegate(gaming_seat_resource)));
}

void gaming_input_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

const struct zcr_gaming_input_v2_interface gaming_input_implementation = {
    gaming_input_get_gaming_seat, gaming_input_destroy};

void bind_gaming_input(wl_client* client,
                       void* data,
                       uint32_t version,
                       uint32_t id) {
  wl_resource* resource =
      wl_resource_create(client, &zcr_gaming_input_v2_interface, version, id);

  wl_resource_set_implementation(resource, &gaming_input_implementation,
                                 nullptr, nullptr);
}

////////////////////////////////////////////////////////////////////////////////
// touch_stylus interface:

class WaylandTouchStylusDelegate : public TouchStylusDelegate {
 public:
  WaylandTouchStylusDelegate(wl_resource* resource, Touch* touch)
      : resource_(resource), touch_(touch) {
    touch_->SetStylusDelegate(this);
  }
  ~WaylandTouchStylusDelegate() override {
    if (touch_ != nullptr)
      touch_->SetStylusDelegate(nullptr);
  }
  void OnTouchDestroying(Touch* touch) override { touch_ = nullptr; }
  void OnTouchTool(int touch_id, ui::EventPointerType type) override {
    uint wayland_type = ZCR_TOUCH_STYLUS_V2_TOOL_TYPE_TOUCH;
    if (type == ui::EventPointerType::POINTER_TYPE_PEN)
      wayland_type = ZCR_TOUCH_STYLUS_V2_TOOL_TYPE_PEN;
    else if (type == ui::EventPointerType::POINTER_TYPE_ERASER)
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
  wl_resource* resource_;
  Touch* touch_;

  DISALLOW_COPY_AND_ASSIGN(WaylandTouchStylusDelegate);
};

void touch_stylus_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

const struct zcr_touch_stylus_v2_interface touch_stylus_implementation = {
    touch_stylus_destroy};

////////////////////////////////////////////////////////////////////////////////
// stylus_v2 interface:

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
      wl_resource_create(client, &zcr_touch_stylus_v2_interface, 1, id);

  SetImplementation(
      stylus_resource, &touch_stylus_implementation,
      std::make_unique<WaylandTouchStylusDelegate>(stylus_resource, touch));
}

const struct zcr_stylus_v2_interface stylus_v2_implementation = {
    stylus_get_touch_stylus};

void bind_stylus_v2(wl_client* client,
                    void* data,
                    uint32_t version,
                    uint32_t id) {
  wl_resource* resource =
      wl_resource_create(client, &zcr_stylus_v2_interface, version, id);
  wl_resource_set_implementation(resource, &stylus_v2_implementation, data,
                                 nullptr);
}

////////////////////////////////////////////////////////////////////////////////
// pointer_gesture_swipe_v1 interface:

void pointer_gestures_get_swipe_gesture(wl_client* client,
                                        wl_resource* resource,
                                        uint32_t id,
                                        wl_resource* pointer_resource) {
  NOTIMPLEMENTED();
}

////////////////////////////////////////////////////////////////////////////////
// pointer_gesture_pinch_v1 interface:

class WaylandPointerGesturePinchDelegate : public PointerGesturePinchDelegate {
 public:
  WaylandPointerGesturePinchDelegate(wl_resource* resource, Pointer* pointer)
      : resource_(resource), pointer_(pointer) {
    pointer_->SetGesturePinchDelegate(this);
  }

  ~WaylandPointerGesturePinchDelegate() override {
    if (pointer_)
      pointer_->SetGesturePinchDelegate(nullptr);
  }
  void OnPointerDestroying(Pointer* pointer) override { pointer_ = nullptr; }
  void OnPointerPinchBegin(uint32_t unique_touch_event_id,
                           base::TimeTicks time_stamp,
                           Surface* surface) override {
    wl_resource* surface_resource = GetSurfaceResource(surface);
    DCHECK(surface_resource);
    zwp_pointer_gesture_pinch_v1_send_begin(resource_, unique_touch_event_id,
                                            TimeTicksToMilliseconds(time_stamp),
                                            surface_resource, 2);
  }
  void OnPointerPinchUpdate(base::TimeTicks time_stamp, float scale) override {
    zwp_pointer_gesture_pinch_v1_send_update(
        resource_, TimeTicksToMilliseconds(time_stamp), 0, 0,
        wl_fixed_from_double(scale), 0);
  }
  void OnPointerPinchEnd(uint32_t unique_touch_event_id,
                         base::TimeTicks time_stamp) override {
    zwp_pointer_gesture_pinch_v1_send_end(resource_, unique_touch_event_id,
                                          TimeTicksToMilliseconds(time_stamp),
                                          0);
  }

 private:
  wl_resource* const resource_;
  Pointer* pointer_;

  DISALLOW_COPY_AND_ASSIGN(WaylandPointerGesturePinchDelegate);
};

void pointer_gesture_pinch_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

const struct zwp_pointer_gesture_pinch_v1_interface
    pointer_gesture_pinch_implementation = {pointer_gesture_pinch_destroy};

void pointer_gestures_get_pinch_gesture(wl_client* client,
                                        wl_resource* resource,
                                        uint32_t id,
                                        wl_resource* pointer_resource) {
  Pointer* pointer = GetUserDataAs<Pointer>(pointer_resource);
  wl_resource* pointer_gesture_pinch_resource = wl_resource_create(
      client, &zwp_pointer_gesture_pinch_v1_interface, 1, id);
  SetImplementation(pointer_gesture_pinch_resource,
                    &pointer_gesture_pinch_implementation,
                    std::make_unique<WaylandPointerGesturePinchDelegate>(
                        pointer_gesture_pinch_resource, pointer));
}

////////////////////////////////////////////////////////////////////////////////
// pointer_gestures_v1 interface:

const struct zwp_pointer_gestures_v1_interface pointer_gestures_implementation =
    {pointer_gestures_get_swipe_gesture, pointer_gestures_get_pinch_gesture};

void bind_pointer_gestures(wl_client* client,
                           void* data,
                           uint32_t version,
                           uint32_t id) {
  wl_resource* resource = wl_resource_create(
      client, &zwp_pointer_gestures_v1_interface, version, id);
  wl_resource_set_implementation(resource, &pointer_gestures_implementation,
                                 data, nullptr);
}

////////////////////////////////////////////////////////////////////////////////
// keyboard_device_configuration interface:

class WaylandKeyboardDeviceConfigurationDelegate
    : public KeyboardDeviceConfigurationDelegate,
      public KeyboardObserver {
 public:
  WaylandKeyboardDeviceConfigurationDelegate(wl_resource* resource,
                                             Keyboard* keyboard)
      : resource_(resource), keyboard_(keyboard) {
    keyboard_->SetDeviceConfigurationDelegate(this);
    keyboard_->AddObserver(this);
  }
  ~WaylandKeyboardDeviceConfigurationDelegate() override {
    if (keyboard_) {
      keyboard_->SetDeviceConfigurationDelegate(nullptr);
      keyboard_->RemoveObserver(this);
    }
  }

  // Overridden from KeyboardObserver:
  void OnKeyboardDestroying(Keyboard* keyboard) override {
    keyboard_ = nullptr;
  }

  // Overridden from KeyboardDeviceConfigurationDelegate:
  void OnKeyboardTypeChanged(bool is_physical) override {
    zcr_keyboard_device_configuration_v1_send_type_change(
        resource_,
        is_physical
            ? ZCR_KEYBOARD_DEVICE_CONFIGURATION_V1_KEYBOARD_TYPE_PHYSICAL
            : ZCR_KEYBOARD_DEVICE_CONFIGURATION_V1_KEYBOARD_TYPE_VIRTUAL);
  }

 private:
  wl_resource* resource_;
  Keyboard* keyboard_;

  DISALLOW_COPY_AND_ASSIGN(WaylandKeyboardDeviceConfigurationDelegate);
};

void keyboard_device_configuration_destroy(wl_client* client,
                                           wl_resource* resource) {
  wl_resource_destroy(resource);
}

const struct zcr_keyboard_device_configuration_v1_interface
    keyboard_device_configuration_implementation = {
        keyboard_device_configuration_destroy};

////////////////////////////////////////////////////////////////////////////////
// keyboard_configuration interface:

void keyboard_configuration_get_keyboard_device_configuration(
    wl_client* client,
    wl_resource* resource,
    uint32_t id,
    wl_resource* keyboard_resource) {
  Keyboard* keyboard = GetUserDataAs<Keyboard>(keyboard_resource);
  if (keyboard->HasDeviceConfigurationDelegate()) {
    wl_resource_post_error(
        resource,
        ZCR_KEYBOARD_CONFIGURATION_V1_ERROR_DEVICE_CONFIGURATION_EXISTS,
        "keyboard has already been associated with a device configuration "
        "object");
    return;
  }

  wl_resource* keyboard_device_configuration_resource = wl_resource_create(
      client, &zcr_keyboard_device_configuration_v1_interface,
      wl_resource_get_version(resource), id);

  SetImplementation(
      keyboard_device_configuration_resource,
      &keyboard_device_configuration_implementation,
      std::make_unique<WaylandKeyboardDeviceConfigurationDelegate>(
          keyboard_device_configuration_resource, keyboard));
}

const struct zcr_keyboard_configuration_v1_interface
    keyboard_configuration_implementation = {
        keyboard_configuration_get_keyboard_device_configuration};

const uint32_t keyboard_configuration_version = 2;

void bind_keyboard_configuration(wl_client* client,
                                 void* data,
                                 uint32_t version,
                                 uint32_t id) {
  wl_resource* resource =
      wl_resource_create(client, &zcr_keyboard_configuration_v1_interface,
                         std::min(version, keyboard_configuration_version), id);
  wl_resource_set_implementation(
      resource, &keyboard_configuration_implementation, data, nullptr);
}

////////////////////////////////////////////////////////////////////////////////
// stylus_tool interface:

class StylusTool : public SurfaceObserver {
 public:
  explicit StylusTool(Surface* surface) : surface_(surface) {
    surface_->AddSurfaceObserver(this);
    surface_->SetProperty(kSurfaceHasStylusToolKey, true);
  }
  ~StylusTool() override {
    if (surface_) {
      surface_->RemoveSurfaceObserver(this);
      surface_->SetProperty(kSurfaceHasStylusToolKey, false);
    }
  }

  void SetStylusOnly() { surface_->SetStylusOnly(); }

  // Overridden from SurfaceObserver:
  void OnSurfaceDestroying(Surface* surface) override {
    surface->RemoveSurfaceObserver(this);
    surface_ = nullptr;
  }

 private:
  Surface* surface_;

  DISALLOW_COPY_AND_ASSIGN(StylusTool);
};

void stylus_tool_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void stylus_tool_set_stylus_only(wl_client* client, wl_resource* resource) {
  GetUserDataAs<StylusTool>(resource)->SetStylusOnly();
}

const struct zcr_stylus_tool_v1_interface stylus_tool_implementation = {
    stylus_tool_destroy, stylus_tool_set_stylus_only};

////////////////////////////////////////////////////////////////////////////////
// stylus_tools interface:

void stylus_tools_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void stylus_tools_get_stylus_tool(wl_client* client,
                                  wl_resource* resource,
                                  uint32_t id,
                                  wl_resource* surface_resource) {
  Surface* surface = GetUserDataAs<Surface>(surface_resource);
  if (surface->GetProperty(kSurfaceHasStylusToolKey)) {
    wl_resource_post_error(
        resource, ZCR_STYLUS_TOOLS_V1_ERROR_STYLUS_TOOL_EXISTS,
        "a stylus_tool object for that surface already exists");
    return;
  }

  wl_resource* stylus_tool_resource =
      wl_resource_create(client, &zcr_stylus_tool_v1_interface, 1, id);

  SetImplementation(stylus_tool_resource, &stylus_tool_implementation,
                    std::make_unique<StylusTool>(surface));
}

const struct zcr_stylus_tools_v1_interface stylus_tools_implementation = {
    stylus_tools_destroy, stylus_tools_get_stylus_tool};

void bind_stylus_tools(wl_client* client,
                       void* data,
                       uint32_t version,
                       uint32_t id) {
  wl_resource* resource =
      wl_resource_create(client, &zcr_stylus_tools_v1_interface, 1, id);

  wl_resource_set_implementation(resource, &stylus_tools_implementation, data,
                                 nullptr);
}

////////////////////////////////////////////////////////////////////////////////
// extended_keyboard interface:

class WaylandExtendedKeyboardImpl : public KeyboardObserver {
 public:
  explicit WaylandExtendedKeyboardImpl(Keyboard* keyboard)
      : keyboard_(keyboard) {
    keyboard_->AddObserver(this);
    keyboard_->SetNeedKeyboardKeyAcks(true);
  }
  ~WaylandExtendedKeyboardImpl() override {
    if (keyboard_) {
      keyboard_->RemoveObserver(this);
      keyboard_->SetNeedKeyboardKeyAcks(false);
    }
  }

  // Overridden from KeyboardObserver:
  void OnKeyboardDestroying(Keyboard* keyboard) override {
    DCHECK(keyboard_ == keyboard);
    keyboard_ = nullptr;
  }

  void AckKeyboardKey(uint32_t serial, bool handled) {
    if (keyboard_)
      keyboard_->AckKeyboardKey(serial, handled);
  }

 private:
  Keyboard* keyboard_;

  DISALLOW_COPY_AND_ASSIGN(WaylandExtendedKeyboardImpl);
};

void extended_keyboard_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void extended_keyboard_ack_key(wl_client* client,
                               wl_resource* resource,
                               uint32_t serial,
                               uint32_t handled_state) {
  GetUserDataAs<WaylandExtendedKeyboardImpl>(resource)->AckKeyboardKey(
      serial, handled_state == ZCR_EXTENDED_KEYBOARD_V1_HANDLED_STATE_HANDLED);
}

const struct zcr_extended_keyboard_v1_interface
    extended_keyboard_implementation = {extended_keyboard_destroy,
                                        extended_keyboard_ack_key};

////////////////////////////////////////////////////////////////////////////////
// keyboard_extension interface:

void keyboard_extension_get_extended_keyboard(wl_client* client,
                                              wl_resource* resource,
                                              uint32_t id,
                                              wl_resource* keyboard_resource) {
  Keyboard* keyboard = GetUserDataAs<Keyboard>(keyboard_resource);
  if (keyboard->AreKeyboardKeyAcksNeeded()) {
    wl_resource_post_error(
        resource, ZCR_KEYBOARD_EXTENSION_V1_ERROR_EXTENDED_KEYBOARD_EXISTS,
        "keyboard has already been associated with a extended_keyboard object");
    return;
  }

  wl_resource* extended_keyboard_resource =
      wl_resource_create(client, &zcr_extended_keyboard_v1_interface, 1, id);

  SetImplementation(extended_keyboard_resource,
                    &extended_keyboard_implementation,
                    std::make_unique<WaylandExtendedKeyboardImpl>(keyboard));
}

const struct zcr_keyboard_extension_v1_interface
    keyboard_extension_implementation = {
        keyboard_extension_get_extended_keyboard};

void bind_keyboard_extension(wl_client* client,
                             void* data,
                             uint32_t version,
                             uint32_t id) {
  wl_resource* resource = wl_resource_create(
      client, &zcr_keyboard_extension_v1_interface, version, id);

  wl_resource_set_implementation(resource, &keyboard_extension_implementation,
                                 data, nullptr);
}

////////////////////////////////////////////////////////////////////////////////
// cursor_shapes interface:

static ui::CursorType GetCursorType(int32_t cursor_shape) {
  switch (cursor_shape) {
#define ADD_CASE(wayland, chrome)                        \
  case ZCR_CURSOR_SHAPES_V1_CURSOR_SHAPE_TYPE_##wayland: \
    return ui::CursorType::chrome

    ADD_CASE(POINTER, kPointer);
    ADD_CASE(CROSS, kCross);
    ADD_CASE(HAND, kHand);
    ADD_CASE(IBEAM, kIBeam);
    ADD_CASE(WAIT, kWait);
    ADD_CASE(HELP, kHelp);
    ADD_CASE(EAST_RESIZE, kEastResize);
    ADD_CASE(NORTH_RESIZE, kNorthResize);
    ADD_CASE(NORTH_EAST_RESIZE, kNorthEastResize);
    ADD_CASE(NORTH_WEST_RESIZE, kNorthWestResize);
    ADD_CASE(SOUTH_RESIZE, kSouthResize);
    ADD_CASE(SOUTH_EAST_RESIZE, kSouthEastResize);
    ADD_CASE(SOUTH_WEST_RESIZE, kSouthWestResize);
    ADD_CASE(WEST_RESIZE, kWestResize);
    ADD_CASE(NORTH_SOUTH_RESIZE, kNorthSouthResize);
    ADD_CASE(EAST_WEST_RESIZE, kEastWestResize);
    ADD_CASE(NORTH_EAST_SOUTH_WEST_RESIZE, kNorthEastSouthWestResize);
    ADD_CASE(NORTH_WEST_SOUTH_EAST_RESIZE, kNorthWestSouthEastResize);
    ADD_CASE(COLUMN_RESIZE, kColumnResize);
    ADD_CASE(ROW_RESIZE, kRowResize);
    ADD_CASE(MIDDLE_PANNING, kMiddlePanning);
    ADD_CASE(EAST_PANNING, kEastPanning);
    ADD_CASE(NORTH_PANNING, kNorthPanning);
    ADD_CASE(NORTH_EAST_PANNING, kNorthEastPanning);
    ADD_CASE(NORTH_WEST_PANNING, kNorthWestPanning);
    ADD_CASE(SOUTH_PANNING, kSouthPanning);
    ADD_CASE(SOUTH_EAST_PANNING, kSouthEastPanning);
    ADD_CASE(SOUTH_WEST_PANNING, kSouthWestPanning);
    ADD_CASE(WEST_PANNING, kWestPanning);
    ADD_CASE(MOVE, kMove);
    ADD_CASE(VERTICAL_TEXT, kVerticalText);
    ADD_CASE(CELL, kCell);
    ADD_CASE(CONTEXT_MENU, kContextMenu);
    ADD_CASE(ALIAS, kAlias);
    ADD_CASE(PROGRESS, kProgress);
    ADD_CASE(NO_DROP, kNoDrop);
    ADD_CASE(COPY, kCopy);
    ADD_CASE(NONE, kNone);
    ADD_CASE(NOT_ALLOWED, kNotAllowed);
    ADD_CASE(ZOOM_IN, kZoomIn);
    ADD_CASE(ZOOM_OUT, kZoomOut);
    ADD_CASE(GRAB, kGrab);
    ADD_CASE(GRABBING, kGrabbing);
    ADD_CASE(DND_NONE, kDndNone);
    ADD_CASE(DND_MOVE, kDndMove);
    ADD_CASE(DND_COPY, kDndCopy);
    ADD_CASE(DND_LINK, kDndLink);
#undef ADD_CASE
    default:
      return ui::CursorType::kNull;
  }
}

void cursor_shapes_set_cursor_shape(wl_client* client,
                                    wl_resource* resource,
                                    wl_resource* pointer_resource,
                                    int32_t shape) {
  ui::CursorType cursor_type = GetCursorType(shape);
  if (cursor_type == ui::CursorType::kNull) {
    wl_resource_post_error(resource, ZCR_CURSOR_SHAPES_V1_ERROR_INVALID_SHAPE,
                           "Unrecognized shape %d", shape);
    return;
  }

  Pointer* pointer = GetUserDataAs<Pointer>(pointer_resource);
  pointer->SetCursorType(cursor_type);
}

const struct zcr_cursor_shapes_v1_interface cursor_shapes_implementation = {
    cursor_shapes_set_cursor_shape};

void bind_cursor_shapes(wl_client* client,
                        void* data,
                        uint32_t version,
                        uint32_t id) {
  wl_resource* resource =
      wl_resource_create(client, &zcr_cursor_shapes_v1_interface, version, id);

  wl_resource_set_implementation(resource, &cursor_shapes_implementation, data,
                                 nullptr);
}

////////////////////////////////////////////////////////////////////////////////
// input_timestamps_v1 interface:

class WaylandInputTimestamps : public WaylandInputDelegate::Observer {
 public:
  WaylandInputTimestamps(wl_resource* resource, WaylandInputDelegate* delegate)
      : resource_(resource), delegate_(delegate) {
    delegate_->AddObserver(this);
  }

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
  wl_resource* const resource_;
  WaylandInputDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(WaylandInputTimestamps);
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

void bind_input_timestamps_manager(wl_client* client,
                                   void* data,
                                   uint32_t version,
                                   uint32_t id) {
  wl_resource* resource = wl_resource_create(
      client, &zwp_input_timestamps_manager_v1_interface, 1, id);

  wl_resource_set_implementation(
      resource, &input_timestamps_manager_implementation, nullptr, nullptr);
}

////////////////////////////////////////////////////////////////////////////////
// text_input_v1 interface:

size_t OffsetFromUTF8Offset(const base::StringPiece& text, uint32_t offset) {
  return base::UTF8ToUTF16(text.substr(0, offset)).size();
}

size_t OffsetFromUTF16Offset(const base::StringPiece16& text, uint32_t offset) {
  return base::UTF16ToUTF8(text.substr(0, offset)).size();
}

class WaylandTextInputDelegate : public TextInput::Delegate {
 public:
  WaylandTextInputDelegate(wl_resource* text_input) : text_input_(text_input) {}
  ~WaylandTextInputDelegate() override = default;

  void set_surface(wl_resource* surface) { surface_ = surface; }

 private:
  wl_client* client() { return wl_resource_get_client(text_input_); }

  uint32_t next_serial() {
    return wl_display_next_serial(wl_client_get_display(client()));
  }

  // TextInput::Delegate:
  void Activated() override {
    zwp_text_input_v1_send_enter(text_input_, surface_);
    wl_client_flush(client());
  }

  void Deactivated() override {
    zwp_text_input_v1_send_leave(text_input_);
    wl_client_flush(client());
  }

  void OnVirtualKeyboardVisibilityChanged(bool is_visible) override {
    zwp_text_input_v1_send_input_panel_state(text_input_, is_visible);
    wl_client_flush(client());
  }

  void SetCompositionText(const ui::CompositionText& composition) override {
    for (const auto& span : composition.ime_text_spans) {
      uint32_t style = ZWP_TEXT_INPUT_V1_PREEDIT_STYLE_DEFAULT;
      switch (span.type) {
        case ui::ImeTextSpan::Type::kComposition:
          if (span.thickness == ui::ImeTextSpan::Thickness::kThick) {
            style = ZWP_TEXT_INPUT_V1_PREEDIT_STYLE_HIGHLIGHT;
          } else {
            style = ZWP_TEXT_INPUT_V1_PREEDIT_STYLE_UNDERLINE;
          }
          break;
        case ui::ImeTextSpan::Type::kSuggestion:
          style = ZWP_TEXT_INPUT_V1_PREEDIT_STYLE_SELECTION;
          break;
        case ui::ImeTextSpan::Type::kMisspellingSuggestion:
          style = ZWP_TEXT_INPUT_V1_PREEDIT_STYLE_INCORRECT;
          break;
      }
      const size_t start =
          OffsetFromUTF16Offset(composition.text, span.start_offset);
      const size_t end =
          OffsetFromUTF16Offset(composition.text, span.end_offset);
      zwp_text_input_v1_send_preedit_styling(text_input_, start, end - start,
                                             style);
    }

    const size_t pos =
        OffsetFromUTF16Offset(composition.text, composition.selection.start());
    zwp_text_input_v1_send_preedit_cursor(text_input_, pos);

    const std::string utf8 = base::UTF16ToUTF8(composition.text);
    zwp_text_input_v1_send_preedit_string(text_input_, next_serial(),
                                          utf8.c_str(), utf8.c_str());

    wl_client_flush(client());
  }

  void Commit(const base::string16& text) override {
    zwp_text_input_v1_send_commit_string(text_input_, next_serial(),
                                         base::UTF16ToUTF8(text).c_str());
    wl_client_flush(client());
  }

  void SetCursor(const gfx::Range& selection) override {
    // TODO(mukai): compute the utf8 offset for |selection| and call
    // zwp_text_input_v1_send_cursor_position.
    NOTIMPLEMENTED();
  }

  void DeleteSurroundingText(const gfx::Range& range) override {
    // TODO(mukai): compute the utf8 offset for |range| and call
    // zwp_text_input_send_delete_surrounding_text.
    NOTIMPLEMENTED();
  }

  void SendKey(const ui::KeyEvent& event) override {
    uint32_t code = ui::KeycodeConverter::DomCodeToNativeKeycode(event.code());
    bool pressed = event.flags() | ui::ET_KEY_PRESSED;
    // TODO(mukai): consolidate the definition of this modifiers_mask with other
    // similar ones in components/exo/keyboard.cc or arc_ime_service.cc.
    constexpr uint32_t modifiers_mask =
        ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN |
        ui::EF_COMMAND_DOWN | ui::EF_ALTGR_DOWN | ui::EF_MOD3_DOWN;
    // 1-bit shifts to adjust the bitpattern for the modifiers; see also
    // WaylandTextInputDelegate::SendModifiers().
    uint32_t modifiers = (event.flags() & modifiers_mask) >> 1;
    zwp_text_input_v1_send_keysym(text_input_,
                                  TimeTicksToMilliseconds(event.time_stamp()),
                                  next_serial(), code,
                                  pressed ? WL_KEYBOARD_KEY_STATE_PRESSED
                                          : WL_KEYBOARD_KEY_STATE_RELEASED,
                                  modifiers);
    wl_client_flush(client());
  }

  void OnTextDirectionChanged(base::i18n::TextDirection direction) override {
    uint32_t wayland_direction = ZWP_TEXT_INPUT_V1_TEXT_DIRECTION_AUTO;
    switch (direction) {
      case base::i18n::RIGHT_TO_LEFT:
        wayland_direction = ZWP_TEXT_INPUT_V1_TEXT_DIRECTION_LTR;
        break;
      case base::i18n::LEFT_TO_RIGHT:
        wayland_direction = ZWP_TEXT_INPUT_V1_TEXT_DIRECTION_RTL;
        break;
      case base::i18n::UNKNOWN_DIRECTION:
        LOG(ERROR) << "Unrecognized direction: " << direction;
    }
    zwp_text_input_v1_send_text_direction(text_input_, next_serial(),
                                          wayland_direction);
  }

  wl_resource* text_input_;
  wl_resource* surface_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(WaylandTextInputDelegate);
};

void text_input_activate(wl_client* client,
                         wl_resource* resource,
                         wl_resource* seat,
                         wl_resource* surface_resource) {
  TextInput* text_input = GetUserDataAs<TextInput>(resource);
  Surface* surface = GetUserDataAs<Surface>(surface_resource);
  static_cast<WaylandTextInputDelegate*>(text_input->delegate())
      ->set_surface(surface_resource);
  text_input->Activate(surface);

  // Sending modifiers.
  constexpr const char* kModifierNames[] = {
      XKB_MOD_NAME_SHIFT,
      XKB_MOD_NAME_CTRL,
      XKB_MOD_NAME_ALT,
      XKB_MOD_NAME_LOGO,
      "Mod5",
      "Mod3",
  };
  wl_array modifiers;
  wl_array_init(&modifiers);
  for (const char* modifier : kModifierNames) {
    char* p =
        static_cast<char*>(wl_array_add(&modifiers, ::strlen(modifier) + 1));
    ::strcpy(p, modifier);
  }
  zwp_text_input_v1_send_modifiers_map(resource, &modifiers);
  wl_array_release(&modifiers);
}

void text_input_deactivate(wl_client* client,
                           wl_resource* resource,
                           wl_resource* seat) {
  TextInput* text_input = GetUserDataAs<TextInput>(resource);
  text_input->Deactivate();
}

void text_input_show_input_panel(wl_client* client, wl_resource* resource) {
  GetUserDataAs<TextInput>(resource)->ShowVirtualKeyboardIfEnabled();
}

void text_input_hide_input_panel(wl_client* client, wl_resource* resource) {
  GetUserDataAs<TextInput>(resource)->HideVirtualKeyboard();
}

void text_input_reset(wl_client* client, wl_resource* resource) {
  GetUserDataAs<TextInput>(resource)->Resync();
}

void text_input_set_surrounding_text(wl_client* client,
                                     wl_resource* resource,
                                     const char* text,
                                     uint32_t cursor,
                                     uint32_t anchor) {
  TextInput* text_input = GetUserDataAs<TextInput>(resource);
  text_input->SetSurroundingText(base::UTF8ToUTF16(text),
                                 OffsetFromUTF8Offset(text, cursor));
}

void text_input_set_content_type(wl_client* client,
                                 wl_resource* resource,
                                 uint32_t hint,
                                 uint32_t purpose) {
  TextInput* text_input = GetUserDataAs<TextInput>(resource);
  ui::TextInputType type = ui::TEXT_INPUT_TYPE_TEXT;
  ui::TextInputMode mode = ui::TEXT_INPUT_MODE_DEFAULT;
  int flags = ui::TEXT_INPUT_FLAG_NONE;
  bool should_do_learning = true;
  if (hint & ZWP_TEXT_INPUT_V1_CONTENT_HINT_AUTO_COMPLETION)
    flags |= ui::TEXT_INPUT_FLAG_AUTOCOMPLETE_ON;
  if (hint & ZWP_TEXT_INPUT_V1_CONTENT_HINT_AUTO_CORRECTION)
    flags |= ui::TEXT_INPUT_FLAG_AUTOCORRECT_ON;
  if (hint & ZWP_TEXT_INPUT_V1_CONTENT_HINT_AUTO_CAPITALIZATION)
    flags |= ui::TEXT_INPUT_FLAG_AUTOCAPITALIZE_SENTENCES;
  if (hint & ZWP_TEXT_INPUT_V1_CONTENT_HINT_LOWERCASE)
    flags |= ui::TEXT_INPUT_FLAG_AUTOCAPITALIZE_NONE;
  if (hint & ZWP_TEXT_INPUT_V1_CONTENT_HINT_UPPERCASE)
    flags |= ui::TEXT_INPUT_FLAG_AUTOCAPITALIZE_CHARACTERS;
  if (hint & ZWP_TEXT_INPUT_V1_CONTENT_HINT_TITLECASE)
    flags |= ui::TEXT_INPUT_FLAG_AUTOCAPITALIZE_WORDS;
  if (hint & ZWP_TEXT_INPUT_V1_CONTENT_HINT_HIDDEN_TEXT) {
    flags |= ui::TEXT_INPUT_FLAG_AUTOCOMPLETE_OFF |
             ui::TEXT_INPUT_FLAG_AUTOCORRECT_OFF;
  }
  if (hint & ZWP_TEXT_INPUT_V1_CONTENT_HINT_SENSITIVE_DATA)
    should_do_learning = false;
  // Unused hints: LATIN, MULTILINE.

  switch (purpose) {
    case ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_DIGITS:
      mode = ui::TEXT_INPUT_MODE_DECIMAL;
      type = ui::TEXT_INPUT_TYPE_NUMBER;
      break;
    case ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_NUMBER:
      mode = ui::TEXT_INPUT_MODE_NUMERIC;
      type = ui::TEXT_INPUT_TYPE_NUMBER;
      break;
    case ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_PHONE:
      mode = ui::TEXT_INPUT_MODE_TEL;
      type = ui::TEXT_INPUT_TYPE_TELEPHONE;
      break;
    case ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_URL:
      mode = ui::TEXT_INPUT_MODE_URL;
      type = ui::TEXT_INPUT_TYPE_URL;
      break;
    case ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_EMAIL:
      mode = ui::TEXT_INPUT_MODE_EMAIL;
      type = ui::TEXT_INPUT_TYPE_EMAIL;
      break;
    case ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_PASSWORD:
      DCHECK(!should_do_learning);
      type = ui::TEXT_INPUT_TYPE_PASSWORD;
      break;
    case ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_DATE:
      type = ui::TEXT_INPUT_TYPE_DATE;
      break;
    case ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_TIME:
      type = ui::TEXT_INPUT_TYPE_TIME;
      break;
    case ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_DATETIME:
      type = ui::TEXT_INPUT_TYPE_DATE_TIME;
      break;
    case ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_NORMAL:
    case ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_ALPHA:
    case ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_NAME:
    case ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_TERMINAL:
      // No special type / mode are set.
      break;
  }

  text_input->SetTypeModeFlags(type, mode, flags, should_do_learning);
}

void text_input_set_cursor_rectangle(wl_client* client,
                                     wl_resource* resource,
                                     int32_t x,
                                     int32_t y,
                                     int32_t width,
                                     int32_t height) {
  GetUserDataAs<TextInput>(resource)->SetCaretBounds(
      gfx::Rect(x, y, width, height));
}

void text_input_set_preferred_language(wl_client* client,
                                       wl_resource* resource,
                                       const char* language) {
  // Nothing needs to be done.
}

void text_input_commit_state(wl_client* client,
                             wl_resource* resource,
                             uint32_t serial) {
  // Nothing needs to be done.
}

void text_input_invoke_action(wl_client* client,
                              wl_resource* resource,
                              uint32_t button,
                              uint32_t index) {
  GetUserDataAs<TextInput>(resource)->Resync();
}

const struct zwp_text_input_v1_interface text_input_v1_implementation = {
    text_input_activate,
    text_input_deactivate,
    text_input_show_input_panel,
    text_input_hide_input_panel,
    text_input_reset,
    text_input_set_surrounding_text,
    text_input_set_content_type,
    text_input_set_cursor_rectangle,
    text_input_set_preferred_language,
    text_input_commit_state,
    text_input_invoke_action,
};

////////////////////////////////////////////////////////////////////////////////
// text_input_manager_v1 interface:

void text_input_manager_create_text_input(wl_client* client,
                                          wl_resource* resource,
                                          uint32_t id) {
  wl_resource* text_input_resource =
      wl_resource_create(client, &zwp_text_input_v1_interface, 1, id);

  SetImplementation(
      text_input_resource, &text_input_v1_implementation,
      std::make_unique<TextInput>(
          std::make_unique<WaylandTextInputDelegate>(text_input_resource)));
}

const struct zwp_text_input_manager_v1_interface
    text_input_manager_implementation = {
        text_input_manager_create_text_input,
};

void bind_text_input_manager(wl_client* client,
                             void* data,
                             uint32_t version,
                             uint32_t id) {
  wl_resource* resource =
      wl_resource_create(client, &zwp_text_input_manager_v1_interface, 1, id);
  wl_resource_set_implementation(resource, &text_input_manager_implementation,
                                 data, nullptr);
}

////////////////////////////////////////////////////////////////////////////////
// zcr_notification_shell_notification_v1 interface:

// Implements notification interface.
class WaylandNotificationShellNotification {
 public:
  WaylandNotificationShellNotification(const std::string& title,
                                       const std::string& message,
                                       const std::string& display_source,
                                       const std::string& notification_id,
                                       const std::vector<std::string>& buttons,
                                       wl_resource* resource)
      : resource_(resource), weak_ptr_factory_(this) {
    notification_ = std::make_unique<Notification>(
        title, message, display_source, notification_id,
        kNotificationShellNotifierId, buttons,
        base::BindRepeating(&WaylandNotificationShellNotification::OnClose,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindRepeating(&WaylandNotificationShellNotification::OnClick,
                            weak_ptr_factory_.GetWeakPtr()));
  }

  void Close() { notification_->Close(); }

 private:
  void OnClose(bool by_user) {
    zcr_notification_shell_notification_v1_send_closed(resource_, by_user);
    wl_client_flush(wl_resource_get_client(resource_));
  }

  void OnClick(const base::Optional<int>& button_index) {
    int32_t index = button_index ? *button_index : -1;
    zcr_notification_shell_notification_v1_send_clicked(resource_, index);
    wl_client_flush(wl_resource_get_client(resource_));
  }

  wl_resource* const resource_;
  std::unique_ptr<Notification> notification_;

  base::WeakPtrFactory<WaylandNotificationShellNotification> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(WaylandNotificationShellNotification);
};

void notification_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void notification_close(wl_client* client, wl_resource* resource) {
  GetUserDataAs<WaylandNotificationShellNotification>(resource)->Close();
}

const struct zcr_notification_shell_notification_v1_interface
    notification_implementation = {notification_destroy, notification_close};

////////////////////////////////////////////////////////////////////////////////
// zcr_notification_shell_v1 interface:

// Implements notification shell interface.
class WaylandNotificationShell {
 public:
  WaylandNotificationShell() : id_(g_next_notification_shell_id.GetNext()) {}

  ~WaylandNotificationShell() = default;

  // Creates a notification on message center from textual information.
  std::unique_ptr<WaylandNotificationShellNotification> CreateNotification(
      const std::string& title,
      const std::string& message,
      const std::string& display_source,
      const std::string& notification_key,
      const std::vector<std::string>& buttons,
      wl_resource* notification_resource) {
    auto notification_id = base::StringPrintf(
        kNotificationShellNotificationIdFormat, id_, notification_key.c_str());

    return std::make_unique<WaylandNotificationShellNotification>(
        title, message, display_source, notification_id, buttons,
        notification_resource);
  }

 private:
  // Id for this notification shell instance.
  const uint32_t id_;

  DISALLOW_COPY_AND_ASSIGN(WaylandNotificationShell);
};

void notification_shell_create_notification(wl_client* client,
                                            wl_resource* resource,
                                            uint32_t id,
                                            const char* title,
                                            const char* message,
                                            const char* display_source,
                                            const char* notification_key,
                                            wl_array* buttons) {
  wl_resource* notification_resource = wl_resource_create(
      client, &zcr_notification_shell_notification_v1_interface,
      wl_resource_get_version(resource), id);

  // Converts wl_array of strings into std::vector<std::string>. All elements
  // are 0-terminated so we use it as the mark of the element's end.
  std::vector<std::string> button_strings;
  const char* data = static_cast<const char*>(buttons->data);
  int len = 0;
  for (const char *pos = data; pos < data + buttons->size; ++pos, ++len) {
    if (*pos == '\0') {
      button_strings.emplace_back(std::string(pos - len, len));
      len = 0;
    }
  }

  std::unique_ptr<WaylandNotificationShellNotification> notification =
      GetUserDataAs<WaylandNotificationShell>(resource)->CreateNotification(
          title, message, display_source, notification_key, button_strings,
          notification_resource);

  SetImplementation(notification_resource, &notification_implementation,
                    std::move(notification));
}

void notification_shell_get_notification_surface(wl_client* client,
                                                 wl_resource* resource,
                                                 uint32_t id,
                                                 wl_resource* surface,
                                                 const char* notification_key) {
  NOTIMPLEMENTED();
}

const struct zcr_notification_shell_v1_interface
    zcr_notification_shell_v1_implementation = {
        notification_shell_create_notification,
        notification_shell_get_notification_surface,
};

void bind_notification_shell(wl_client* client,
                             void* data,
                             uint32_t version,
                             uint32_t id) {
  wl_resource* resource =
      wl_resource_create(client, &zcr_notification_shell_v1_interface, 1, id);

  SetImplementation(resource, &zcr_notification_shell_v1_implementation,
                    std::make_unique<WaylandNotificationShell>());
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// Server::Output, public:

Server::Output::Output(int64_t id) : id_(id) {}

Server::Output::~Output() {
  if (global_)
    wl_global_destroy(global_);
}

////////////////////////////////////////////////////////////////////////////////
// Server, public:

Server::Server(Display* display)
    : display_(display), wl_display_(wl_display_create()) {
  wl_global_create(wl_display_.get(), &wl_compositor_interface,
                   compositor_version, display_, bind_compositor);
  wl_global_create(wl_display_.get(), &wl_shm_interface, 1, display_, bind_shm);
#if defined(USE_OZONE)
  wl_global_create(wl_display_.get(), &zwp_linux_dmabuf_v1_interface,
                   linux_dmabuf_version, display_, bind_linux_dmabuf);
#endif
  wl_global_create(wl_display_.get(), &wl_subcompositor_interface, 1, display_,
                   bind_subcompositor);
  wl_global_create(wl_display_.get(), &wl_shell_interface, 1, display_,
                   bind_shell);
  display::Screen::GetScreen()->AddObserver(this);
  for (const auto& display : display::Screen::GetScreen()->GetAllDisplays())
    OnDisplayAdded(display);
  wl_global_create(wl_display_.get(), &zxdg_shell_v6_interface, 1, display_,
                   bind_xdg_shell_v6);
  wl_global_create(wl_display_.get(), &zcr_vsync_feedback_v1_interface, 1,
                   display_, bind_vsync_feedback);
  wl_global_create(wl_display_.get(), &wl_data_device_manager_interface,
                   data_device_manager_version, display_,
                   bind_data_device_manager);
  wl_global_create(wl_display_.get(), &wl_seat_interface, seat_version,
                   display_->seat(), bind_seat);
  wl_global_create(wl_display_.get(), &wp_viewporter_interface, 1, display_,
                   bind_viewporter);
  wl_global_create(wl_display_.get(), &wp_presentation_interface, 1, display_,
                   bind_presentation);
  wl_global_create(wl_display_.get(), &zcr_secure_output_v1_interface, 1,
                   display_, bind_secure_output);
  wl_global_create(wl_display_.get(), &zcr_alpha_compositing_v1_interface, 1,
                   display_, bind_alpha_compositing);
  wl_global_create(wl_display_.get(), &zcr_remote_shell_v1_interface,
                   remote_shell_version, display_, bind_remote_shell);
  wl_global_create(wl_display_.get(), &zaura_shell_interface,
                   aura_shell_version, display_, bind_aura_shell);
  wl_global_create(wl_display_.get(), &zcr_gaming_input_v2_interface, 1,
                   display_, bind_gaming_input);
  wl_global_create(wl_display_.get(), &zcr_stylus_v2_interface, 1, display_,
                   bind_stylus_v2);
  wl_global_create(wl_display_.get(), &zwp_pointer_gestures_v1_interface, 1,
                   display_, bind_pointer_gestures);
  wl_global_create(wl_display_.get(), &zcr_keyboard_configuration_v1_interface,
                   keyboard_configuration_version, display_,
                   bind_keyboard_configuration);
  wl_global_create(wl_display_.get(), &zcr_stylus_tools_v1_interface, 1,
                   display_, bind_stylus_tools);
  wl_global_create(wl_display_.get(), &zcr_keyboard_extension_v1_interface, 1,
                   display_, bind_keyboard_extension);
  wl_global_create(wl_display_.get(), &zcr_cursor_shapes_v1_interface, 1,
                   display_, bind_cursor_shapes);
  wl_global_create(wl_display_.get(),
                   &zwp_input_timestamps_manager_v1_interface, 1, display_,
                   bind_input_timestamps_manager);
  wl_global_create(wl_display_.get(), &zwp_text_input_manager_v1_interface, 1,
                   display_, bind_text_input_manager);
  wl_global_create(wl_display_.get(), &zcr_notification_shell_v1_interface, 1,
                   display_, bind_notification_shell);
}

Server::~Server() {
  display::Screen::GetScreen()->RemoveObserver(this);
}

// static
std::unique_ptr<Server> Server::Create(Display* display) {
  std::unique_ptr<Server> server(new Server(display));

  char* runtime_dir = getenv("XDG_RUNTIME_DIR");
  if (!runtime_dir) {
    LOG(ERROR) << "XDG_RUNTIME_DIR not set in the environment";
    return nullptr;
  }

  std::string socket_name(kSocketName);
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kWaylandServerSocket)) {
    socket_name =
        command_line->GetSwitchValueASCII(switches::kWaylandServerSocket);
  }

  if (!server->AddSocket(socket_name.c_str())) {
    LOG(ERROR) << "Failed to add socket: " << socket_name;
    return nullptr;
  }

  base::FilePath socket_path = base::FilePath(runtime_dir).Append(socket_name);

  // Change permissions on the socket.
  struct group wayland_group;
  struct group* wayland_group_res = nullptr;
  char buf[10000];
  if (HANDLE_EINTR(getgrnam_r(kWaylandSocketGroup, &wayland_group, buf,
                              sizeof(buf), &wayland_group_res)) < 0) {
    PLOG(ERROR) << "getgrnam_r";
    return nullptr;
  }
  if (wayland_group_res) {
    if (HANDLE_EINTR(chown(socket_path.MaybeAsASCII().c_str(), -1,
                           wayland_group.gr_gid)) < 0) {
      PLOG(ERROR) << "chown";
      return nullptr;
    }
  } else {
    LOG(WARNING) << "Group '" << kWaylandSocketGroup << "' not found";
  }

  if (!base::SetPosixFilePermissions(socket_path, 0660)) {
    PLOG(ERROR) << "Could not set permissions: " << socket_path.value();
    return nullptr;
  }

  return server;
}

bool Server::AddSocket(const std::string name) {
  DCHECK(!name.empty());
  return !wl_display_add_socket(wl_display_.get(), name.c_str());
}

int Server::GetFileDescriptor() const {
  wl_event_loop* event_loop = wl_display_get_event_loop(wl_display_.get());
  DCHECK(event_loop);
  return wl_event_loop_get_fd(event_loop);
}

void Server::Dispatch(base::TimeDelta timeout) {
  wl_event_loop* event_loop = wl_display_get_event_loop(wl_display_.get());
  DCHECK(event_loop);
  wl_event_loop_dispatch(event_loop, timeout.InMilliseconds());
}

void Server::Flush() {
  wl_display_flush_clients(wl_display_.get());
}

void Server::OnDisplayAdded(const display::Display& new_display) {
  auto output = std::make_unique<Output>(new_display.id());
  output->set_global(wl_global_create(wl_display_.get(), &wl_output_interface,
                                      output_version, output.get(),
                                      bind_output));
  DCHECK_EQ(outputs_.count(new_display.id()), 0u);
  outputs_.insert(std::make_pair(new_display.id(), std::move(output)));
}

void Server::OnDisplayRemoved(const display::Display& old_display) {
  DCHECK_EQ(outputs_.count(old_display.id()), 1u);
  outputs_.erase(old_display.id());
}

}  // namespace wayland
}  // namespace exo
