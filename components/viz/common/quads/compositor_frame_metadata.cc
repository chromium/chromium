// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/quads/compositor_frame_metadata.h"

#include "base/strings/stringprintf.h"
#include "base/trace_event/traced_value.h"
#include "cc/base/math_util.h"

namespace viz {

CompositorFrameMetadata::CompositorFrameMetadata() = default;

CompositorFrameMetadata::CompositorFrameMetadata(
    CompositorFrameMetadata&& other) = default;

CompositorFrameMetadata::~CompositorFrameMetadata() = default;

CompositorFrameMetadata& CompositorFrameMetadata::operator=(
    CompositorFrameMetadata&& other) = default;

CompositorFrameMetadata CompositorFrameMetadata::Clone() const {
  CompositorFrameMetadata metadata(*this);
  return metadata;
}

void CompositorFrameMetadata::AsValueInto(
    base::trace_event::TracedValue* value) const {
  value->SetDouble("device_scale_factor", device_scale_factor);
  cc::MathUtil::AddToTracedValue("root_scroll_offset", root_scroll_offset,
                                 value);
  value->SetDouble("page_scale_factor", page_scale_factor);
  cc::MathUtil::AddToTracedValue("scrollable_viewport_size",
                                 scrollable_viewport_size, value);
  cc::MathUtil::AddToTracedValue("visible_viewport_size", visible_viewport_size,
                                 value);
  value->SetInteger("content_color_usage",
                    static_cast<int>(content_color_usage));
  value->SetBoolean("may_contain_video", may_contain_video);
  value->SetBoolean("may_throttle_if_undrawn_frames",
                    may_throttle_if_undrawn_frames);
  value->SetBoolean("is_handling_interaction", is_handling_interaction);
  value->SetBoolean("is_handling_animation", is_handling_animation);
  value->SetString(
      "root_background_color",
      base::StringPrintf("rgba_f(%.2f, %.2f, %.2f, %.2f)",
                         root_background_color.fR, root_background_color.fG,
                         root_background_color.fB, root_background_color.fA));

  value->SetInteger("latency_info_count",
                    base::saturated_cast<int>(latency_info.size()));

  value->BeginArray("referenced_surfaces");
  for (const auto& surface_range : referenced_surfaces) {
    value->AppendString(surface_range.ToString());
  }
  value->EndArray();

  value->BeginArray("activation_dependencies");
  for (const auto& surface_id : activation_dependencies) {
    value->AppendString(surface_id.ToString());
  }
  value->EndArray();

  value->SetString("deadline", deadline.ToString());

  value->BeginDictionary("begin_frame_ack");
  begin_frame_ack.AsValueInto(value);
  value->EndDictionary();

  value->SetInteger("frame_token", static_cast<int>(frame_token));
  value->SetBoolean("send_frame_token_to_embedder",
                    send_frame_token_to_embedder);
  value->SetDouble("min_page_scale_factor", min_page_scale_factor);

  if (top_controls_visible_height) {
    value->SetDouble("top_controls_visible_height",
                     *top_controls_visible_height);
  }

  value->SetInteger("display_transform_hint",
                    static_cast<int>(display_transform_hint));
  value->SetBoolean("is_mobile_optimized", is_mobile_optimized);

  value->SetBoolean("has_delegated_ink_metadata", !!delegated_ink_metadata);

  value->BeginArray("transition_directives");
  for (const auto& directive : transition_directives) {
    value->BeginDictionary();
    directive.AsValueInto(value);
    value->EndDictionary();
  }
  value->EndArray();

  value->SetString("capture_bounds", capture_bounds.ToString());

  value->SetBoolean("has_shared_element_resources",
                    has_shared_element_resources);
  value->SetBoolean("has_screenshot_destination",
                    screenshot_destination.has_value());
  value->SetBoolean("is_software", is_software);

  value->BeginArray("offset_tag_definitions");
  for (const auto& definition : offset_tag_definitions) {
    value->BeginDictionary();
    definition.AsValueInto(value);
    value->EndDictionary();
  }
  value->EndArray();

  value->BeginArray("offset_tag_values");
  for (const auto& tag_value : offset_tag_values) {
    value->AppendString(tag_value.ToString());
  }
  value->EndArray();

  value->BeginDictionary("frame_interval_inputs");
  frame_interval_inputs.AsValueInto(value);
  value->EndDictionary();

  // Ignore trees_in_viz_timing_details because it's for metrics only.
}

CompositorFrameMetadata::CompositorFrameMetadata(
    const CompositorFrameMetadata& other)
    : device_scale_factor(other.device_scale_factor),
      root_scroll_offset(other.root_scroll_offset),
      page_scale_factor(other.page_scale_factor),
      scrollable_viewport_size(other.scrollable_viewport_size),
      visible_viewport_size(other.visible_viewport_size),
      content_color_usage(other.content_color_usage),
      may_contain_video(other.may_contain_video),
      is_handling_interaction(other.is_handling_interaction),
      is_handling_animation(other.is_handling_animation),
      root_background_color(other.root_background_color),
      latency_info(other.latency_info),
      referenced_surfaces(other.referenced_surfaces),
      activation_dependencies(other.activation_dependencies),
      deadline(other.deadline),
      begin_frame_ack(other.begin_frame_ack),
      frame_token(other.frame_token),
      send_frame_token_to_embedder(other.send_frame_token_to_embedder),
      min_page_scale_factor(other.min_page_scale_factor),
      top_controls_visible_height(other.top_controls_visible_height),
      display_transform_hint(other.display_transform_hint),
      is_mobile_optimized(other.is_mobile_optimized),
      transition_directives(other.transition_directives),
      has_shared_element_resources(other.has_shared_element_resources),
      screenshot_destination(other.screenshot_destination),
      is_software(other.is_software),
      offset_tag_definitions(other.offset_tag_definitions),
      offset_tag_values(other.offset_tag_values),
      frame_interval_inputs(other.frame_interval_inputs) {
  if (other.delegated_ink_metadata) {
    delegated_ink_metadata = std::make_unique<gfx::DelegatedInkMetadata>(
        *other.delegated_ink_metadata.get());
  }
}

}  // namespace viz
