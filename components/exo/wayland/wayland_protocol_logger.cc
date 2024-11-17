// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/exo/wayland/wayland_protocol_logger.h"

// We need wl_object declared below.
#undef WL_HIDE_DEPRECATED
#include <wayland-server.h>

#include <string>

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/trace_event/typed_macros.h"

namespace {

std::string StringifyWaylandArgument(const wl_interface* type,
                                     const wl_argument& arg,
                                     const char** signature_ptr) {
  while (**signature_ptr) {
    char s = **signature_ptr;

    // Always advance the pointer before returning, so that the next
    // call to this function will look at the next argument's signature.
    (*signature_ptr)++;

    // The type of `arg` is indicated by this character of the signature.
    switch (s) {
      case 'i':
        return base::NumberToString(arg.i);
      case 'u':
        return base::NumberToString(arg.u);
      case 'f':
        return base::NumberToString(wl_fixed_to_double(arg.f));
      case 's':
        return arg.s ? arg.s : "nil";
      case 'o':
        if (arg.o) {
          const wl_interface* interface = arg.o->interface;
          return base::StrCat({interface ? interface->name : "[unknown]", "@",
                               base::NumberToString(arg.o->id)});
        }
        return "nil";
      case 'n':
        // type should not normally be null, but this handles requests
        // from clients so we need to be robust against invalid data.
        return arg.n ? base::StrCat({"new id ", type ? type->name : "[unknown]",
                                     "@", base::NumberToString(arg.n)})
                     : "nil";
      case 'a':
        // Improve on WAYLAND_DEBUG by printing array contents as hex data.
        // If the array is "too long", truncate.
        static const size_t max_printed_bytes = 48;
        return base::StrCat(
            {"array[", base::NumberToString(arg.a->size), " bytes]{",
             base::HexEncode(arg.a->data,
                             std::min(arg.a->size, max_printed_bytes)),
             // Append an ellipsis if the content was truncated.
             max_printed_bytes < arg.a->size ? "...}" : "}"});
      case 'h':
        return base::StrCat({"fd ", base::NumberToString(arg.h)});
      case '?':
        // ? indicates the next argument is optional.
        // Pass through to next loop iteration.
        break;
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
        // Numbers indicate protocol versions.
        // Pass through to next loop iteration.
        break;
    }
  }
  // Reached a null character in the signature. This shouldn't happen.
  return std::string("<Wayland signature parse error>");
}

void LogToPerfetto(void* user_data,
                   wl_protocol_logger_type type,
                   const wl_protocol_logger_message* message) {
  // Early-out in the common case that tracing is not currently enabled.
  bool tracing_enabled = false;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED("exo", &tracing_enabled);
  if (!tracing_enabled) {
    return;
  }

  std::vector<std::string> msg{
      exo::wayland::WaylandProtocolLogger::FormatMessage(type, message)};

  TRACE_EVENT_INSTANT(
      "exo", perfetto::DynamicString{msg[0]}, [&](perfetto::EventContext ctx) {
        for (size_t i = 1; i < msg.size(); i++) {
          std::string arg_name = base::StrCat({"arg", base::NumberToString(i)});
          ctx.AddDebugAnnotation(perfetto::DynamicString{arg_name}, msg[i]);
        }
      });
}

}  // namespace

namespace exo::wayland {

WaylandProtocolLogger::WaylandProtocolLogger(struct wl_display* display) {
  logger_.reset(wl_display_add_protocol_logger(display, handler_, nullptr));
}

// Complex class/struct needs an explicit out-of-line destructor.
WaylandProtocolLogger::~WaylandProtocolLogger() = default;

// static
std::vector<std::string> WaylandProtocolLogger::FormatMessage(
    wl_protocol_logger_type type,
    const wl_protocol_logger_message* message) {
  std::vector<std::string> return_value;
  return_value.push_back(base::StrCat(
      {type == WL_PROTOCOL_LOGGER_EVENT ? "Sent event: " : "Received request: ",
       wl_resource_get_class(message->resource), "@",
       base::NumberToString(wl_resource_get_id(message->resource)), ".",
       message->message->name}));

  const char* signature = message->message->signature;
  for (int i = 0; i < message->arguments_count && *signature; i++) {
    return_value.push_back(StringifyWaylandArgument(
        message->message->types[i], message->arguments[i], &signature));
  }
  return return_value;
}

// static
void WaylandProtocolLogger::SetHandlerFuncForTesting(
    wl_protocol_logger_func_t handler) {
  handler_ = handler;
}

// static
wl_protocol_logger_func_t WaylandProtocolLogger::handler_ = LogToPerfetto;

void WaylandProtocolLogger::Deleter::operator()(wl_protocol_logger* logger) {
  wl_protocol_logger_destroy(logger);
}

}  // namespace exo::wayland
