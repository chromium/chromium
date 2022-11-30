// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "components/exo/wayland/clients/client_helper.h"

// Client that retreives output related properties (modes, scales, etc.) from
// a compositor and prints them to standard output.

namespace {

// This struct contains all the fields that will be set by output
// interface listener callbacks.
struct Info {
  int32_t connection;
  int32_t device_scale_factor;
  struct {
    int32_t x, y;
    int32_t physical_width, physical_height;
    int32_t subpixel;
    std::string make;
    std::string model;
    int32_t transform;
  } geometry;
  struct Mode {
    uint32_t flags;
    int32_t width, height;
    int32_t refresh;
  };
  // |next_modes| are swapped with |modes| after receiving output done event.
  std::vector<Mode> modes, next_modes;
  struct Scale {
    uint32_t flags;
    int32_t scale;
  };
  // |next_scales| are swapped with |scales| after receiving output done event.
  std::vector<Scale> scales, next_scales;
  struct {
    int32_t top, left, bottom, right;
  } insets;
  int32_t logical_transform;
  std::unique_ptr<wl_output> output;
  std::unique_ptr<zaura_output> aura_output;
};

// This struct contains globals and all outputs.
struct Globals {
  std::unique_ptr<zaura_shell> aura_shell;
  std::vector<Info> outputs;
};

void RegistryHandler(void* data,
                     wl_registry* registry,
                     uint32_t id,
                     const char* interface,
                     uint32_t version) {
  Globals* globals = static_cast<Globals*>(data);

  if (strcmp(interface, "wl_output") == 0) {
    globals->outputs.push_back(
        {.connection = ZAURA_OUTPUT_CONNECTION_TYPE_UNKNOWN,
         .device_scale_factor = ZAURA_OUTPUT_SCALE_FACTOR_1000,
         .geometry = {.subpixel = WL_OUTPUT_SUBPIXEL_UNKNOWN,
                      .make = "unknown",
                      .model = "unknown",
                      .transform = WL_OUTPUT_TRANSFORM_NORMAL}});
    globals->outputs.back().output.reset(static_cast<wl_output*>(
        wl_registry_bind(registry, id, &wl_output_interface, 2)));
  } else if (strcmp(interface, "zaura_shell") == 0) {
    if (version >= 2) {
      globals->aura_shell.reset(static_cast<zaura_shell*>(
          wl_registry_bind(registry, id, &zaura_shell_interface, 5)));
    }
  }
}

void RegistryRemover(void* data, wl_registry* registry, uint32_t id) {
  LOG(WARNING) << "Got a registry losing event for " << id;
}

void OutputGeometry(void* data,
                    wl_output* output,
                    int x,
                    int y,
                    int physical_width,
                    int physical_height,
                    int subpixel,
                    const char* make,
                    const char* model,
                    int transform) {
  Info* info = static_cast<Info*>(data);

  info->geometry.x = x;
  info->geometry.y = y;
  info->geometry.physical_width = physical_width;
  info->geometry.physical_height = physical_height;
  info->geometry.subpixel = subpixel;
  info->geometry.make = make;
  info->geometry.model = model;
  info->geometry.transform = transform;
}

void OutputMode(void* data,
                wl_output* output,
                uint32_t flags,
                int width,
                int height,
                int refresh) {
  Info* info = static_cast<Info*>(data);

  info->next_modes.push_back({flags, width, height, refresh});
}

void OutputDone(void* data, wl_output* output) {
  Info* info = static_cast<Info*>(data);

  std::swap(info->modes, info->next_modes);
  info->next_modes.clear();
  std::swap(info->scales, info->next_scales);
  info->next_scales.clear();
}

void OutputScale(void* data, wl_output* output, int32_t scale) {
  Info* info = static_cast<Info*>(data);

  info->device_scale_factor = scale * 1000;
}

void AuraOutputScale(void* data,
                     zaura_output* output,
                     uint32_t flags,
                     uint32_t scale) {
  Info* info = static_cast<Info*>(data);

  info->next_scales.push_back({flags, static_cast<int32_t>(scale)});
}

void AuraOutputConnection(void* data,
                          zaura_output* output,
                          uint32_t connection) {
  Info* info = static_cast<Info*>(data);

  info->connection = connection;
}

void AuraOutputDeviceScaleFactor(void* data,
                                 zaura_output* output,
                                 uint32_t device_scale_factor) {
  Info* info = static_cast<Info*>(data);

  info->device_scale_factor = device_scale_factor;
}

void AuraOutputInsets(void* data,
                      zaura_output* output,
                      int32_t top,
                      int32_t left,
                      int32_t bottom,
                      int32_t right) {
  Info* info = static_cast<Info*>(data);

  info->insets = {top, left, bottom, right};
}

void AuraOutputLogicalTransform(void* data,
                                zaura_output* output,
                                int32_t transform) {
  Info* info = static_cast<Info*>(data);

  info->logical_transform = transform;
}

std::string OutputSubpixelToString(int32_t subpixel) {
  switch (subpixel) {
    case WL_OUTPUT_SUBPIXEL_UNKNOWN:
      return "unknown";
    case WL_OUTPUT_SUBPIXEL_NONE:
      return "none";
    case WL_OUTPUT_SUBPIXEL_HORIZONTAL_RGB:
      return "horizontal rgb";
    case WL_OUTPUT_SUBPIXEL_HORIZONTAL_BGR:
      return "horizontal bgr";
    case WL_OUTPUT_SUBPIXEL_VERTICAL_RGB:
      return "vertical rgb";
    case WL_OUTPUT_SUBPIXEL_VERTICAL_BGR:
      return "vertical bgr";
    default:
      return base::StringPrintf("unknown (%d)", subpixel);
  }
}

std::string OutputTransformToString(int32_t transform) {
  switch (transform) {
    case WL_OUTPUT_TRANSFORM_NORMAL:
      return "normal";
    case WL_OUTPUT_TRANSFORM_90:
      return "90°";
    case WL_OUTPUT_TRANSFORM_180:
      return "180°";
    case WL_OUTPUT_TRANSFORM_270:
      return "270°";
    case WL_OUTPUT_TRANSFORM_FLIPPED:
      return "flipped";
    case WL_OUTPUT_TRANSFORM_FLIPPED_90:
      return "flipped 90°";
    case WL_OUTPUT_TRANSFORM_FLIPPED_180:
      return "flipped 180°";
    case WL_OUTPUT_TRANSFORM_FLIPPED_270:
      return "flipped 270°";
    default:
      return base::StringPrintf("unknown (%d)", transform);
  }
}

std::string OutputModeFlagsToString(uint32_t flags) {
  std::string string;
  if (flags & WL_OUTPUT_MODE_CURRENT)
    string += "current       ";
  if (flags & WL_OUTPUT_MODE_PREFERRED)
    string += "preferred";
  base::TrimWhitespaceASCII(string, base::TRIM_TRAILING, &string);
  return string;
}

std::string AuraOutputScaleFlagsToString(uint32_t flags) {
  std::string string;
  if (flags & ZAURA_OUTPUT_SCALE_PROPERTY_CURRENT)
    string += "current       ";
  if (flags & ZAURA_OUTPUT_SCALE_PROPERTY_PREFERRED)
    string += "preferred";
  base::TrimWhitespaceASCII(string, base::TRIM_TRAILING, &string);
  return string;
}

std::string AuraOutputScaleFactorToString(int32_t scale) {
  switch (scale) {
    case ZAURA_OUTPUT_SCALE_FACTOR_0400:
    case ZAURA_OUTPUT_SCALE_FACTOR_0500:
    case ZAURA_OUTPUT_SCALE_FACTOR_0550:
    case ZAURA_OUTPUT_SCALE_FACTOR_0600:
    case ZAURA_OUTPUT_SCALE_FACTOR_0625:
    case ZAURA_OUTPUT_SCALE_FACTOR_0650:
    case ZAURA_OUTPUT_SCALE_FACTOR_0700:
    case ZAURA_OUTPUT_SCALE_FACTOR_0750:
    case ZAURA_OUTPUT_SCALE_FACTOR_0800:
    case ZAURA_OUTPUT_SCALE_FACTOR_0850:
    case ZAURA_OUTPUT_SCALE_FACTOR_0900:
    case ZAURA_OUTPUT_SCALE_FACTOR_0950:
    case ZAURA_OUTPUT_SCALE_FACTOR_1000:
    case ZAURA_OUTPUT_SCALE_FACTOR_1050:
    case ZAURA_OUTPUT_SCALE_FACTOR_1100:
    case ZAURA_OUTPUT_SCALE_FACTOR_1150:
    case ZAURA_OUTPUT_SCALE_FACTOR_1125:
    case ZAURA_OUTPUT_SCALE_FACTOR_1200:
    case ZAURA_OUTPUT_SCALE_FACTOR_1250:
    case ZAURA_OUTPUT_SCALE_FACTOR_1300:
    case ZAURA_OUTPUT_SCALE_FACTOR_1400:
    case ZAURA_OUTPUT_SCALE_FACTOR_1450:
    case ZAURA_OUTPUT_SCALE_FACTOR_1500:
    case ZAURA_OUTPUT_SCALE_FACTOR_1600:
    case ZAURA_OUTPUT_SCALE_FACTOR_1750:
    case ZAURA_OUTPUT_SCALE_FACTOR_1800:
    case ZAURA_OUTPUT_SCALE_FACTOR_2000:
    case ZAURA_OUTPUT_SCALE_FACTOR_2200:
    case ZAURA_OUTPUT_SCALE_FACTOR_2250:
    case ZAURA_OUTPUT_SCALE_FACTOR_2500:
    case ZAURA_OUTPUT_SCALE_FACTOR_2750:
    case ZAURA_OUTPUT_SCALE_FACTOR_3000:
    case ZAURA_OUTPUT_SCALE_FACTOR_3500:
    case ZAURA_OUTPUT_SCALE_FACTOR_4000:
    case ZAURA_OUTPUT_SCALE_FACTOR_4500:
    case ZAURA_OUTPUT_SCALE_FACTOR_5000:
      return base::StringPrintf("%.3f", scale / 1000.0);
    default:
      return base::StringPrintf("unknown (%g)", scale / 1000.0);
  }
}

std::string AuraOutputConnectionToString(uint32_t connection_type) {
  switch (connection_type) {
    case ZAURA_OUTPUT_CONNECTION_TYPE_UNKNOWN:
      return "unknown";
    case ZAURA_OUTPUT_CONNECTION_TYPE_INTERNAL:
      return "internal";
    default:
      return "invalid";
  }
}

}  // namespace

int main(int argc, char* argv[]) {
  std::unique_ptr<wl_display> display(wl_display_connect(nullptr));
  if (!display) {
    LOG(ERROR) << "Failed to connect to display";
    return 1;
  }

  Globals globals;

  wl_registry_listener registry_listener = {RegistryHandler, RegistryRemover};
  wl_registry* registry = wl_display_get_registry(display.get());
  wl_registry_add_listener(registry, &registry_listener, &globals);

  wl_display_roundtrip(display.get());

  wl_output_listener output_listener = {OutputGeometry, OutputMode, OutputDone,
                                        OutputScale};

  zaura_output_listener aura_output_listener = {
      AuraOutputScale, AuraOutputConnection, AuraOutputDeviceScaleFactor,
      AuraOutputInsets, AuraOutputLogicalTransform};
  for (auto& info : globals.outputs) {
    wl_output_add_listener(info.output.get(), &output_listener, &info);
    if (globals.aura_shell) {
      info.aura_output.reset(
          static_cast<zaura_output*>(zaura_shell_get_aura_output(
              globals.aura_shell.get(), info.output.get())));
      zaura_output_add_listener(info.aura_output.get(), &aura_output_listener,
                                &info);
    }
  }

  wl_display_roundtrip(display.get());

  for (auto& info : globals.outputs) {
    int id = &info - &globals.outputs[0];
    if (id)
      std::cout << std::endl;
    std::cout << "OUTPUT" << id << ":" << std::endl << std::endl;
    std::cout << "  connection:          "
              << AuraOutputConnectionToString(info.connection) << std::endl;
    std::cout << "  device scale factor: "
              << AuraOutputScaleFactorToString(info.device_scale_factor)
              << std::endl
              << std::endl;
    std::cout << "  geometry:" << std::endl
              << "    x:                 " << info.geometry.x << std::endl
              << "    y:                 " << info.geometry.y << std::endl
              << "    physical width:    " << info.geometry.physical_width
              << " mm" << std::endl
              << "    physical height:   " << info.geometry.physical_height
              << " mm" << std::endl
              << "    subpixel:          "
              << OutputSubpixelToString(info.geometry.subpixel) << std::endl
              << "    make:              " << info.geometry.make << std::endl
              << "    model:             " << info.geometry.model << std::endl
              << "    transform:         "
              << OutputTransformToString(info.geometry.transform) << std::endl
              << std::endl;
    std::cout << "  modes:" << std::endl;
    for (auto& mode : info.modes) {
      std::cout << "    " << std::left << std::setw(19)
                << base::StringPrintf("%dx%d:", mode.width, mode.height)
                << std::left << std::setw(14)
                << base::StringPrintf("%.2f Hz", mode.refresh / 1000.0)
                << OutputModeFlagsToString(mode.flags) << std::endl;
    }
    if (!info.scales.empty()) {
      std::cout << std::endl;
      std::cout << "  scales:" << std::endl;
      for (auto& scale : info.scales) {
        std::cout << "    " << std::left << std::setw(19)
                  << (AuraOutputScaleFactorToString(scale.scale) + ":")
                  << AuraOutputScaleFlagsToString(scale.flags) << std::endl;
      }
    }
    std::cout << "  insets:" << std::endl
              << "    top:     " << info.insets.top << std::endl
              << "    left:    " << info.insets.left << std::endl
              << "    bottom:  " << info.insets.bottom << std::endl
              << "    right:   " << info.insets.right << std::endl
              << std::endl;
    std::cout << "  logical_transform: "
              << OutputTransformToString(info.logical_transform) << std::endl;
  }

  return 0;
}
