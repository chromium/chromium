// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/output_metrics.h"

#include "base/bit_cast.h"
#include "components/exo/wayland/wayland_display_util.h"
#include "components/exo/wm_helper.h"
#include "ui/display/display.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/display/manager/util/display_manager_util.h"

namespace exo::wayland {
namespace {

constexpr float kInchInMm = 25.4f;

constexpr char kUnknown[] = "unknown";

const display::ManagedDisplayInfo& GetDisplayInfo(
    const display::Display& display) {
  return WMHelper::GetInstance()->GetDisplayInfo(display.id());
}

std::vector<OutputMetrics::OutputScale> GetOutputScales(
    const display::Display& display) {
  std::vector<OutputMetrics::OutputScale> output_scales;

  const WMHelper* wm_helper = WMHelper::GetInstance();
  const display::ManagedDisplayInfo& display_info = GetDisplayInfo(display);

  display::ManagedDisplayMode active_mode;
  bool rv = wm_helper->GetActiveModeForDisplayId(display.id(), &active_mode);
  DCHECK(rv);
  const int32_t current_output_scale =
      std::round(display_info.zoom_factor() * 1000.f);
  std::vector<float> zoom_factors = display::GetDisplayZoomFactors(active_mode);

  // Ensure that the current zoom factor is a part of the list.
  if (base::ranges::none_of(zoom_factors, [&display_info](float zoom_factor) {
        return std::abs(display_info.zoom_factor() - zoom_factor) <=
               std::numeric_limits<float>::epsilon();
      })) {
    zoom_factors.push_back(display_info.zoom_factor());
  }

  for (float zoom_factor : zoom_factors) {
    int32_t output_scale = std::round(zoom_factor * 1000.f);
    uint32_t flags = 0;
    if (output_scale == 1000) {
      flags |= ZAURA_OUTPUT_SCALE_PROPERTY_PREFERRED;
    }
    if (current_output_scale == output_scale) {
      flags |= ZAURA_OUTPUT_SCALE_PROPERTY_CURRENT;
    }

    output_scales.push_back(
        {.scale_property = flags,
         .scale_factor = static_cast<uint32_t>(output_scale)});
  }

  return output_scales;
}

}  // namespace

OutputMetrics::OutputMetrics(const display::Display& display)
    : make(GetDisplayInfo(display).manufacturer_id().empty()
               ? GetDisplayInfo(display).manufacturer_id()
               : kUnknown),
      model(GetDisplayInfo(display).product_id().empty()
                ? GetDisplayInfo(display).product_id()
                : kUnknown),
      origin(display.bounds().origin()),
      refresh_mhz(static_cast<int32_t>(60000)),
      logical_origin(display.bounds().origin()),
      logical_size(display.bounds().size()),
      description(display.label()),
      display_id(ui::wayland::ToWaylandDisplayIdPair(display.id())),
      output_scales(GetOutputScales(display)),
      connection_type(display.IsInternal()
                          ? ZAURA_OUTPUT_CONNECTION_TYPE_INTERNAL
                          : ZAURA_OUTPUT_CONNECTION_TYPE_UNKNOWN),
      logical_insets(display.bounds().InsetsFrom(display.work_area())),
      device_scale_factor_deprecated(
          GetDisplayInfo(display).device_scale_factor() * 1000),
      device_scale_factor(display.device_scale_factor()),
      logical_transform(OutputTransform(display.rotation())) {
  const display::ManagedDisplayInfo& info = GetDisplayInfo(display);

  physical_size_px = info.bounds_in_native().size();
  physical_size_mm =
      ScaleToRoundedSize(physical_size_px, kInchInMm / info.device_dpi());

  physical_overscan_insets = info.GetOverscanInsetsInPixel();

  // Use panel_rotation otherwise some X apps will refuse to take events from
  // outside the "visible" region.
  panel_transform = OutputTransform(display.panel_rotation());

  // TODO(reveman): Send real list of modes.
  mode_flags = WL_OUTPUT_MODE_CURRENT | WL_OUTPUT_MODE_PREFERRED;

  // wl_output only supports integer scaling, so if device scale factor is
  // fractional we need to round it up to the closest integer.
  scale = std::ceil(display.device_scale_factor());
}

OutputMetrics::OutputMetrics(const OutputMetrics&) = default;

OutputMetrics& OutputMetrics::operator=(const OutputMetrics&) = default;

OutputMetrics::~OutputMetrics() = default;

}  // namespace exo::wayland
