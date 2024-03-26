// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/wayland_protocol_logger.h"

#include <wayland-server-core.h>
#include <sstream>

#include "base/trace_event/typed_macros.h"

namespace {

void LogToPerfetto(void* user_data,
                   wl_protocol_logger_type type,
                   const wl_protocol_logger_message* message) {
  std::stringstream name;
  name << (type == WL_PROTOCOL_LOGGER_EVENT ? "Sent event: "
                                            : "Received request: ");
  name << wl_resource_get_class(message->resource);
  name << '@';
  name << wl_resource_get_id(message->resource);
  name << '.';
  name << message->message->name;
  TRACE_EVENT_INSTANT("exo", perfetto::DynamicString{name.str()});
}

}  // namespace

namespace exo::wayland {

WaylandProtocolLogger::WaylandProtocolLogger(struct wl_display* display) {
  logger_.reset(
      wl_display_add_protocol_logger(display, &LogToPerfetto, nullptr));
}

// Complex class/struct needs an explicit out-of-line destructor.
WaylandProtocolLogger::~WaylandProtocolLogger() {}

void WaylandProtocolLogger::Deleter::operator()(wl_protocol_logger* logger) {
  wl_protocol_logger_destroy(logger);
}

}  // namespace exo::wayland
