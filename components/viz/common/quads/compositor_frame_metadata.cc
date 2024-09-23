// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/quads/compositor_frame_metadata.h"

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

CompositorFrameMetadata::CompositorFrameMetadata(
    const CompositorFrameMetadata& other)
    : device_scale_factor(other.device_scale_factor),
      root_scroll_offset(other.root_scroll_offset),
      page_scale_factor(other.page_scale_factor),
      scrollable_viewport_size(other.scrollable_viewport_size),
      content_color_usage(other.content_color_usage),
      may_contain_video(other.may_contain_video),
      is_handling_interaction(other.is_handling_interaction),
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
      transition_directives(other.transition_directives),
      has_shared_element_resources(other.has_shared_element_resources),
      screenshot_destination(other.screenshot_destination),
      is_software(other.is_software),
      offset_tag_definitions(other.offset_tag_definitions),
      offset_tag_values(other.offset_tag_values) {
  if (other.delegated_ink_metadata) {
    delegated_ink_metadata = std::make_unique<gfx::DelegatedInkMetadata>(
        *other.delegated_ink_metadata.get());
  }
}

}  // namespace viz
