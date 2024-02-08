// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_OUTPUT_METRICS_H_
#define COMPONENTS_EXO_WAYLAND_OUTPUT_METRICS_H_

#include <vector>

#include <aura-shell-server-protocol.h>
#include <wayland-server-protocol-core.h>

#include "ui/base/wayland/wayland_display_util.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"

namespace display {
class Display;
}  // namespace display

namespace exo::wayland {

// Metrics for wl_output and supported extensions.
struct OutputMetrics {
  explicit OutputMetrics(const display::Display& display);
  OutputMetrics(const OutputMetrics&);
  OutputMetrics& operator=(const OutputMetrics&);
  virtual ~OutputMetrics();

  //////////////////////////////////////////////////////////////////////////////
  // wl_output metrics

  // Textual description of the manufacturer.
  std::string make;

  // Textual description of the model.
  std::string model;

  // `origin` is used in wayland service to identify the workspace the pixel
  // size will be applied.
  gfx::Point origin;

  // `physical_size_px` is the physical resolution of the display in pixels.
  // The value should not include any overscan insets or display rotation,
  // except for any panel orientation adjustment.
  gfx::Size physical_size_px;

  // `physical_size_mm` is our best-effort approximation for the physical size
  // of the display in millimeters, given the display resolution and DPI. The
  // value should not include any overscan insets or display rotation, except
  // for any panel orientation adjustment.
  gfx::Size physical_size_mm;

  // Subpixel orientation of the output.
  wl_output_subpixel subpixel = WL_OUTPUT_SUBPIXEL_UNKNOWN;

  // Transform that maps framebuffer to output.
  wl_output_transform panel_transform;

  // Bitfield of mode flags.
  uint32_t mode_flags;

  // Vertical refresh rate in mHz.
  int32_t refresh_mhz;

  // Scaling geometry information.
  int32_t scale;

  //////////////////////////////////////////////////////////////////////////////
  // xdg_output metrics

  // `logical_origin` is used in wayland service to identify the workspace
  // the pixel size will be applied.
  gfx::Point logical_origin;

  // Size of the output in the global compositor space.
  gfx::Size logical_size;

  // Human-readable description of this output.
  std::string description;

  //////////////////////////////////////////////////////////////////////////////
  // aura output metrics

  // Aura's display id associated with this output.
  ui::wayland::WaylandDisplayIdPair display_id;

  struct OutputScale {
    // Bitfield of scale flags.
    uint32_t scale_property;

    // The output scale factor.
    uint32_t scale_factor;
  };
  std::vector<OutputScale> output_scales;

  // Describes how the output is connected.
  zaura_output_connection_type connection_type;

  // Describes the insets for the output in logical screen coordinates, from
  // which the work area can be calculated.
  gfx::Insets logical_insets;

  // Describes the overscan insets for the output in physical pixels.
  gfx::Insets physical_overscan_insets;

  // A deprecated description of the device scale factor for the output. This is
  // calculated by taking `ManagedDisplayInfo::device_scale_factor_`,
  // multiplying it by 1000 and casting it to a unit32_t (truncating towards
  // zero).
  // TODO(tluk): Migrate clients to the new device_scale_factor value below and
  // remove this.
  uint32_t device_scale_factor_deprecated;

  // Describes the device scale factor for the output. Maps directly to
  // `Display::device_scale_factor_`.
  float device_scale_factor;

  // Describes the logical transform for the output. Whereas
  // wl_output.geometry's transform corresponds to the display's panel rotation,
  // the logical transform corresponds to the display's logical rotation.
  wl_output_transform logical_transform;
};

}  // namespace exo::wayland

#endif  // COMPONENTS_EXO_WAYLAND_OUTPUT_METRICS_H_
