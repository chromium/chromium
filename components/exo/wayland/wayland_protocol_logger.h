// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_WAYLAND_PROTOCOL_LOGGER_H_
#define COMPONENTS_EXO_WAYLAND_WAYLAND_PROTOCOL_LOGGER_H_

#include <wayland-server-core.h>

#include <memory>
#include <vector>

namespace exo::wayland {

// Manages the lifetime of a wl_protocol_logger struct.
class WaylandProtocolLogger {
 public:
  explicit WaylandProtocolLogger(wl_display* display);
  ~WaylandProtocolLogger();

  WaylandProtocolLogger(const WaylandProtocolLogger&) = delete;
  WaylandProtocolLogger(WaylandProtocolLogger&&) = delete;
  WaylandProtocolLogger& operator=(const WaylandProtocolLogger&) = delete;
  WaylandProtocolLogger& operator=(WaylandProtocolLogger&&) = delete;

  // Return one or more human-readable strings describing a Wayland message.
  //
  // The first string indicates the type of message (event or request),
  // and the class and numeric ID of the receiving Wayland object.
  // Further strings represent the values of message arguments.
  static std::vector<std::string> FormatMessage(
      wl_protocol_logger_type type,
      const wl_protocol_logger_message* message);

  // Allow overriding this wl_protocol_logger's logging function in tests.
  // Must be called before constructing exo::wayland::Server.
  static void SetHandlerFuncForTesting(wl_protocol_logger_func_t handler);

 private:
  static wl_protocol_logger_func_t handler_;
  class Deleter {
   public:
    void operator()(wl_protocol_logger* logger);
  };

  std::unique_ptr<wl_protocol_logger, Deleter> logger_;
};

}  // namespace exo::wayland

#endif  // COMPONENTS_EXO_WAYLAND_WAYLAND_PROTOCOL_LOGGER_H_
