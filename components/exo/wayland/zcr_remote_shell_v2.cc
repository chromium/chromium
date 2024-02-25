// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <aura-shell-server-protocol.h>

#include "ash/wm/desks/desks_util.h"
#include "components/exo/wayland/server_util.h"
#include "components/exo/wayland/zcr_remote_shell_impl.h"
#include "components/exo/wm_helper.h"

namespace exo::wayland {
namespace {

int RemoteSurfaceContainerV2(uint32_t container) {
  switch (container) {
    case ZCR_REMOTE_SHELL_V2_CONTAINER_DEFAULT:
      return ash::desks_util::GetActiveDeskContainerId();
    case ZCR_REMOTE_SHELL_V2_CONTAINER_OVERLAY:
      return ash::kShellWindowId_SystemModalContainer;
    default:
      DLOG(WARNING) << "Unsupported container: " << container;
      return ash::desks_util::GetActiveDeskContainerId();
  }
}

const struct WaylandRemoteOutputEventMapping remote_output_event_mapping_v2 = {
    zcr_remote_output_v2_send_identification_data,
    // deprecated but not removed due to lack of alternative for stable
    // identifier for wl_resource required on the ARC side.
    zcr_remote_output_v2_send_display_id,
    zcr_remote_output_v2_send_port,
    zcr_remote_output_v2_send_insets,
    zcr_remote_output_v2_send_stable_insets,
    zcr_remote_output_v2_send_systemui_behavior,
    ZCR_REMOTE_OUTPUT_V2_SYSTEMUI_BEHAVIOR_SINCE_VERSION,
    ZCR_REMOTE_OUTPUT_V2_STABLE_INSETS_SINCE_VERSION,
};

const WaylandRemoteShellEventMapping wayland_remote_shell_event_mapping_v2 = {
    zcr_remote_surface_v2_send_window_geometry_changed,
    zcr_remote_surface_v2_send_change_zoom_level,
    zcr_remote_surface_v2_send_state_type_changed,
    zcr_remote_surface_v2_send_bounds_changed_in_output,
    zcr_remote_surface_v2_send_bounds_changed,
    nullptr,  // moved to zaura-shell protocol
    zcr_remote_shell_v2_send_desktop_focus_state_changed,
    nullptr,  // event removed in v2
    zcr_remote_surface_v2_send_drag_finished,
    zcr_remote_surface_v2_send_drag_started,
    zcr_remote_shell_v2_send_layout_mode,
    zcr_remote_shell_v2_send_default_device_scale_factor,
    nullptr,  // event removed in v2
    ZCR_REMOTE_SURFACE_V2_BOUNDS_CHANGED_IN_OUTPUT_SINCE_VERSION,
    ZCR_REMOTE_SHELL_V2_DESKTOP_FOCUS_STATE_CHANGED_SINCE_VERSION,
    ZCR_REMOTE_SHELL_V2_LAYOUT_MODE_SINCE_VERSION,
    ZCR_REMOTE_SHELL_V2_DEFAULT_DEVICE_SCALE_FACTOR_SINCE_VERSION,
    1,
    1,
    1,
    /*has_bounds_change_reason_float=*/true,
};

const struct zcr_remote_surface_v2_interface remote_surface_implementation_v2 =
    {
        +[](wl_client* client, wl_resource* resource) {
          wl_resource_destroy(resource);
        },
        zcr_remote_shell::remote_surface_set_app_id,
        zcr_remote_shell::remote_surface_set_title,
        zcr_remote_shell::remote_surface_set_top_inset,
        zcr_remote_shell::remote_surface_maximize,
        zcr_remote_shell::remote_surface_minimize,
        zcr_remote_shell::remote_surface_restore,
        zcr_remote_shell::remote_surface_fullscreen,
        zcr_remote_shell::remote_surface_pin,
        zcr_remote_shell::remote_surface_unpin,
        zcr_remote_shell::remote_surface_set_system_modal,
        zcr_remote_shell::remote_surface_unset_system_modal,
        zcr_remote_shell::remote_surface_set_rectangular_surface_shadow,
        zcr_remote_shell::remote_surface_set_systemui_visibility,
        zcr_remote_shell::remote_surface_set_always_on_top,
        zcr_remote_shell::remote_surface_unset_always_on_top,
        zcr_remote_shell::remote_surface_set_orientation,
        zcr_remote_shell::remote_surface_start_move,
        zcr_remote_shell::remote_surface_set_can_maximize,
        zcr_remote_shell::remote_surface_unset_can_maximize,
        zcr_remote_shell::remote_surface_set_min_size,
        zcr_remote_shell::remote_surface_set_max_size,
        zcr_remote_shell::remote_surface_set_snapped_to_left,
        zcr_remote_shell::remote_surface_set_snapped_to_right,
        zcr_remote_shell::remote_surface_start_resize,
        zcr_remote_shell::remote_surface_set_frame,
        zcr_remote_shell::remote_surface_set_frame_buttons,
        zcr_remote_shell::remote_surface_set_extra_title,
        zcr_remote_shell::remote_surface_set_orientation_lock,
        zcr_remote_shell::remote_surface_pip,
        zcr_remote_shell::remote_surface_set_aspect_ratio,
        zcr_remote_shell::remote_surface_set_accessibility_id_DEPRECATED,
        zcr_remote_shell::remote_surface_set_pip_original_window,
        zcr_remote_shell::remote_surface_unset_pip_original_window,
        zcr_remote_shell::remote_surface_set_system_gesture_exclusion,
        zcr_remote_shell::remote_surface_set_resize_lock,
        zcr_remote_shell::remote_surface_unset_resize_lock,
        zcr_remote_shell::remote_surface_set_bounds_in_output,
        zcr_remote_shell::remote_surface_set_resize_lock_type,
        zcr_remote_shell::remote_surface_set_float,
        zcr_remote_shell::remote_surface_set_scale_factor,
        zcr_remote_shell::remote_surface_set_window_corner_radii,
        zcr_remote_shell::remote_surface_set_shadow_corner_radii
};

const struct zcr_notification_surface_v2_interface
    notification_surface_implementation_v2 = {
        +[](wl_client* client, wl_resource* resource) {
          wl_resource_destroy(resource);
        },
        zcr_remote_shell::notification_surface_set_app_id,
};

const struct zcr_input_method_surface_v2_interface
    input_method_surface_implementation_v2 = {
        +[](wl_client* client, wl_resource* resource) {
          wl_resource_destroy(resource);
        },
        zcr_remote_shell::input_method_surface_set_bounds_in_output,
};

const struct zcr_toast_surface_v2_interface toast_surface_implementation_v2 = {
    +[](wl_client* client, wl_resource* resource) {
      wl_resource_destroy(resource);
    },
    zcr_remote_shell::toast_surface_set_bounds_in_output,
    zcr_remote_shell::toast_surface_set_scale_factor,
};

const struct zcr_remote_output_v2_interface remote_output_implementation_v2 = {
    +[](wl_client* client, wl_resource* resource) {
      wl_resource_destroy(resource);
    },
};

void HandleRemoteSurfaceCloseCallbackV2(wl_resource* resource) {
  zcr_remote_surface_v2_send_close(resource);
  wl_client_flush(wl_resource_get_client(resource));
}

void remote_shell_get_remote_surface_v2(wl_client* client,
                                        wl_resource* resource,
                                        uint32_t id,
                                        wl_resource* surface,
                                        uint32_t container) {
  WaylandRemoteShell* shell = GetUserDataAs<WaylandRemoteShell>(resource);
  std::unique_ptr<ClientControlledShellSurface> shell_surface =
      shell->CreateShellSurface(GetUserDataAs<Surface>(surface),
                                RemoteSurfaceContainerV2(container));
  if (!shell_surface) {
    wl_resource_post_error(resource, ZCR_REMOTE_SHELL_V2_ERROR_ROLE,
                           "surface has already been assigned a role");
    return;
  }

  wl_resource* remote_surface_resource =
      wl_resource_create(client, &zcr_remote_surface_v2_interface,
                         wl_resource_get_version(resource), id);

  shell_surface->SetSecurityDelegate(GetSecurityDelegate(client));

  shell_surface->set_delegate(
      shell->CreateShellSurfaceDelegate(remote_surface_resource));
  shell_surface->set_close_callback(
      base::BindRepeating(&HandleRemoteSurfaceCloseCallbackV2,
                          base::Unretained(remote_surface_resource)));
  shell_surface->set_surface_destroyed_callback(base::BindOnce(
      &wl_resource_destroy, base::Unretained(remote_surface_resource)));

  SetImplementation(remote_surface_resource, &remote_surface_implementation_v2,
                    std::move(shell_surface));
}

void remote_shell_get_notification_surface_v2(wl_client* client,
                                              wl_resource* resource,
                                              uint32_t id,
                                              wl_resource* surface,
                                              const char* notification_key) {
  if (GetUserDataAs<Surface>(surface)->HasSurfaceDelegate()) {
    wl_resource_post_error(resource, ZCR_REMOTE_SHELL_V2_ERROR_ROLE,
                           "surface has already been assigned a role");
    return;
  }

  std::unique_ptr<NotificationSurface> notification_surface =
      GetUserDataAs<WaylandRemoteShell>(resource)->CreateNotificationSurface(
          GetUserDataAs<Surface>(surface), std::string(notification_key));
  if (!notification_surface) {
    wl_resource_post_error(resource,
                           ZCR_REMOTE_SHELL_V2_ERROR_INVALID_NOTIFICATION_KEY,
                           "invalid notification key");
    return;
  }

  wl_resource* notification_surface_resource =
      wl_resource_create(client, &zcr_notification_surface_v2_interface,
                         wl_resource_get_version(resource), id);
  SetImplementation(notification_surface_resource,
                    &notification_surface_implementation_v2,
                    std::move(notification_surface));
}

void remote_shell_get_input_method_surface_v2(wl_client* client,
                                              wl_resource* resource,
                                              uint32_t id,
                                              wl_resource* surface) {
  if (GetUserDataAs<Surface>(surface)->HasSurfaceDelegate()) {
    wl_resource_post_error(resource, ZCR_REMOTE_SHELL_V2_ERROR_ROLE,
                           "surface has already been assigned a role");
    return;
  }

  std::unique_ptr<ClientControlledShellSurface> input_method_surface =
      GetUserDataAs<WaylandRemoteShell>(resource)->CreateInputMethodSurface(
          GetUserDataAs<Surface>(surface));
  if (!input_method_surface) {
    wl_resource_post_error(resource, ZCR_REMOTE_SHELL_V2_ERROR_ROLE,
                           "Cannot create an IME surface");
    return;
  }

  wl_resource* input_method_surface_resource =
      wl_resource_create(client, &zcr_input_method_surface_v2_interface,
                         wl_resource_get_version(resource), id);
  SetImplementation(input_method_surface_resource,
                    &input_method_surface_implementation_v2,
                    std::move(input_method_surface));
}

void remote_shell_get_toast_surface_v2(wl_client* client,
                                       wl_resource* resource,
                                       uint32_t id,
                                       wl_resource* surface) {
  if (GetUserDataAs<Surface>(surface)->HasSurfaceDelegate()) {
    wl_resource_post_error(resource, ZCR_REMOTE_SHELL_V2_ERROR_ROLE,
                           "surface has already been assigned a role");
    return;
  }

  std::unique_ptr<ClientControlledShellSurface> toast_surface =
      GetUserDataAs<WaylandRemoteShell>(resource)->CreateToastSurface(
          GetUserDataAs<Surface>(surface));
  if (!toast_surface) {
    wl_resource_post_error(resource, ZCR_REMOTE_SHELL_V2_ERROR_ROLE,
                           "Cannot create an toast surface");
    return;
  }

  wl_resource* toast_surface_resource =
      wl_resource_create(client, &zcr_toast_surface_v2_interface,
                         wl_resource_get_version(resource), id);
  SetImplementation(toast_surface_resource, &toast_surface_implementation_v2,
                    std::move(toast_surface));
}

void remote_shell_get_remote_output_v2(wl_client* client,
                                       wl_resource* resource,
                                       uint32_t id,
                                       wl_resource* output_resource) {
  WaylandDisplayHandler* display_handler =
      GetUserDataAs<WaylandDisplayHandler>(output_resource);

  wl_resource* remote_output_resource =
      wl_resource_create(client, &zcr_remote_output_v2_interface,
                         wl_resource_get_version(resource), id);

  auto remote_output = std::make_unique<WaylandRemoteOutput>(
      remote_output_resource, remote_output_event_mapping_v2, display_handler);

  SetImplementation(remote_output_resource, &remote_output_implementation_v2,
                    std::move(remote_output));
}

const struct zcr_remote_shell_v2_interface remote_shell_implementation_v2 = {
    +[](wl_client* client, wl_resource* resource) {
      // Nothing to do here.
    },
    remote_shell_get_remote_surface_v2,
    remote_shell_get_notification_surface_v2,
    remote_shell_get_input_method_surface_v2,
    remote_shell_get_toast_surface_v2,
    remote_shell_get_remote_output_v2};

}  // namespace

void bind_remote_shell_v2(wl_client* client,
                          void* data,
                          uint32_t version,
                          uint32_t id) {
  wl_resource* resource = wl_resource_create(
      client, &zcr_remote_shell_v2_interface,
      std::min<uint32_t>(version, zcr_remote_shell_v2_interface.version), id);

  auto* remote_shell_data = static_cast<WaylandRemoteShellData*>(data);
  SetImplementation(
      resource, &remote_shell_implementation_v2,
      std::make_unique<WaylandRemoteShell>(
          remote_shell_data->display, resource,
          base::BindRepeating(remote_shell_data->output_provider, client),
          wayland_remote_shell_event_mapping_v2,
          /*use_default_scale_cancellation_default=*/false));
}

}  // namespace exo::wayland
