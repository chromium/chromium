// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_WAYLAND_PROTOCOL_LOGGER_H_
#define COMPONENTS_EXO_WAYLAND_WAYLAND_PROTOCOL_LOGGER_H_

#include <memory>

struct wl_display;
struct wl_protocol_logger;

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

 private:
  class Deleter {
   public:
    void operator()(wl_protocol_logger* logger);
  };

  std::unique_ptr<wl_protocol_logger, Deleter> logger_;
};

}  // namespace exo::wayland

#endif  // COMPONENTS_EXO_WAYLAND_WAYLAND_PROTOCOL_LOGGER_H_
