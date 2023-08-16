// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/common/ppapi_utils.h"

#include <cstring>

#include "base/command_line.h"
#include "build/build_config.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/common/content_switches.h"
#include "ppapi/c/dev/ppb_audio_input_dev.h"
#include "ppapi/c/dev/ppb_audio_output_dev.h"
#include "ppapi/c/dev/ppb_buffer_dev.h"
#include "ppapi/c/dev/ppb_char_set_dev.h"
#include "ppapi/c/dev/ppb_crypto_dev.h"
#include "ppapi/c/dev/ppb_cursor_control_dev.h"
#include "ppapi/c/dev/ppb_device_ref_dev.h"
#include "ppapi/c/dev/ppb_file_chooser_dev.h"
#include "ppapi/c/dev/ppb_gles_chromium_texture_mapping_dev.h"
#include "ppapi/c/dev/ppb_ime_input_event_dev.h"
#include "ppapi/c/dev/ppb_memory_dev.h"
#include "ppapi/c/dev/ppb_opengles2ext_dev.h"
#include "ppapi/c/dev/ppb_printing_dev.h"
#include "ppapi/c/dev/ppb_text_input_dev.h"
#include "ppapi/c/dev/ppb_trace_event_dev.h"
#include "ppapi/c/dev/ppb_url_util_dev.h"
#include "ppapi/c/dev/ppb_var_deprecated.h"
#include "ppapi/c/dev/ppb_video_capture_dev.h"
#include "ppapi/c/dev/ppb_video_decoder_dev.h"
#include "ppapi/c/dev/ppb_view_dev.h"
#include "ppapi/c/ppb_audio.h"
#include "ppapi/c/ppb_audio_buffer.h"
#include "ppapi/c/ppb_audio_config.h"
#include "ppapi/c/ppb_console.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/c/ppb_file_io.h"
#include "ppapi/c/ppb_file_ref.h"
#include "ppapi/c/ppb_file_system.h"
#include "ppapi/c/ppb_fullscreen.h"
#include "ppapi/c/ppb_gamepad.h"
#include "ppapi/c/ppb_graphics_2d.h"
#include "ppapi/c/ppb_graphics_3d.h"
#include "ppapi/c/ppb_host_resolver.h"
#include "ppapi/c/ppb_image_data.h"
#include "ppapi/c/ppb_input_event.h"
#include "ppapi/c/ppb_instance.h"
#include "ppapi/c/ppb_media_stream_audio_track.h"
#include "ppapi/c/ppb_media_stream_video_track.h"
#include "ppapi/c/ppb_messaging.h"
#include "ppapi/c/ppb_mouse_cursor.h"
#include "ppapi/c/ppb_mouse_lock.h"
#include "ppapi/c/ppb_net_address.h"
#include "ppapi/c/ppb_network_list.h"
#include "ppapi/c/ppb_network_monitor.h"
#include "ppapi/c/ppb_network_proxy.h"
#include "ppapi/c/ppb_opengles2.h"
#include "ppapi/c/ppb_tcp_socket.h"
#include "ppapi/c/ppb_text_input_controller.h"
#include "ppapi/c/ppb_udp_socket.h"
#include "ppapi/c/ppb_url_loader.h"
#include "ppapi/c/ppb_url_request_info.h"
#include "ppapi/c/ppb_url_response_info.h"
#include "ppapi/c/ppb_var.h"
#include "ppapi/c/ppb_var_array.h"
#include "ppapi/c/ppb_var_array_buffer.h"
#include "ppapi/c/ppb_var_dictionary.h"
#include "ppapi/c/ppb_video_decoder.h"
#include "ppapi/c/ppb_video_encoder.h"
#include "ppapi/c/ppb_video_frame.h"
#include "ppapi/c/ppb_view.h"
#include "ppapi/c/ppb_vpn_provider.h"
#include "ppapi/c/ppb_websocket.h"
#include "ppapi/c/private/ppb_camera_capabilities_private.h"
#include "ppapi/c/private/ppb_camera_device_private.h"
#include "ppapi/c/private/ppb_ext_crx_file_system_private.h"
#include "ppapi/c/private/ppb_file_io_private.h"
#include "ppapi/c/private/ppb_file_ref_private.h"
#include "ppapi/c/private/ppb_host_resolver_private.h"
#include "ppapi/c/private/ppb_isolated_file_system_private.h"
#include "ppapi/c/private/ppb_proxy_private.h"
#include "ppapi/c/private/ppb_tcp_server_socket_private.h"
#include "ppapi/c/private/ppb_tcp_socket_private.h"
#include "ppapi/c/private/ppb_testing_private.h"
#include "ppapi/c/private/ppb_udp_socket_private.h"
#include "ppapi/c/private/ppb_uma_private.h"
#include "ppapi/c/private/ppb_x509_certificate_private.h"
#include "ppapi/c/trusted/ppb_browser_font_trusted.h"
#include "ppapi/c/trusted/ppb_char_set_trusted.h"
#include "ppapi/c/trusted/ppb_file_chooser_trusted.h"
#include "ppapi/c/trusted/ppb_url_loader_trusted.h"
#include "ppapi/thunk/thunk.h"

bool IsSupportedPepperInterface(const char* name) {
// TODO(brettw) put these in a hash map for better performance.
#define PROXIED_IFACE(iface_str, iface_struct) \
  if (strcmp(name, iface_str) == 0)            \
    return true;

#include "ppapi/thunk/interfaces_ppb_private.h"
#include "ppapi/thunk/interfaces_ppb_private_no_permissions.h"
#include "ppapi/thunk/interfaces_ppb_public_dev.h"
#include "ppapi/thunk/interfaces_ppb_public_dev_channel.h"
#include "ppapi/thunk/interfaces_ppb_public_socket.h"
#include "ppapi/thunk/interfaces_ppb_public_stable.h"

#undef PROXIED_IFACE

#define LEGACY_IFACE(iface_str, dummy) \
  if (strcmp(name, iface_str) == 0)    \
    return true;

#include "ppapi/thunk/interfaces_legacy.h"

#undef LEGACY_IFACE
  return false;
}

namespace {
bool g_allow_nacl = true;

bool IsBrowserProcess() {
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kProcessType);
}  // namespace
}  // namespace

void DisallowNacl() {
  CHECK(IsBrowserProcess());
  g_allow_nacl = false;
}

bool IsNaclAllowed() {
  // In the browser process we directly check g_allow_nacl and in other
  // processes we check for the command line switch. This is because:
  //   (1) It's discouraged to add switches to browser process.
  //   (2) We must use a command line switch for child processes to get the
  //       information early in startup.
  if (IsBrowserProcess()) {
    return g_allow_nacl;
  } else {
    return !base::CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kDisableNaCl);
  }
}

void AppendDisableNaclSwitchIfNecessary(base::CommandLine* command_line) {
  if (!IsNaclAllowed()) {
    command_line->AppendSwitch(switches::kDisableNaCl);
  }
}
