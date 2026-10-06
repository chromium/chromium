// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/quads/compositor_frame.h"

#include <unordered_map>

#include "base/containers/adapters.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "base/values.h"
#include "cc/paint/filter_operations.h"
#include "components/viz/common/quads/draw_quad.h"
#include "components/viz/common/traced_value.h"

namespace viz {

CompositorFrame::CompositorFrame() = default;

CompositorFrame::CompositorFrame(CompositorFrame&& other) = default;

CompositorFrame::~CompositorFrame() = default;

CompositorFrame& CompositorFrame::operator=(CompositorFrame&& other) = default;

bool CompositorFrame::HasCopyOutputRequests() const {
  // Iterate the RenderPasses back-to-front, because CopyOutputRequests tend to
  // be made on the later passes.
  for (const auto& pass : base::Reversed(render_pass_list)) {
    if (!pass->copy_requests.empty()) {
      return true;
    }
  }
  return false;
}

bool CompositorFrame::HasVisuallyNonEmptyContent() const {
  if (render_pass_list.empty()) {
    return false;
  }
  // Multiple render passes indicate intermediate passes (e.g. effects, filters,
  // masks), which imply rendered content.
  if (render_pass_list.size() > 1) {
    return true;
  }

  // An empty root pass or a single solid color quad (representing the default
  // background fill) are both considered empty, as long as the frame damages
  // its entire output_rect (see below). We do not check the single quad's
  // coverage, clipping, or transform, because an attacker cannot realistically
  // spoof a page with a single solid color quad and no resources, and avoiding
  // strict rect checks prevents false positives from viewport sizing or scale
  // rounding quirks.
  //
  // Anything else (e.g. tile, picture, texture, surface, debug border)
  // represents rendered content and is considered non-empty.
  //
  // Note: Any transferable resources (textures, tile raster buffers, images)
  // in `resource_list` also imply non-empty content, but checking for this is
  // unnecessary for empty quad lists or single solid quads, since those quads
  // do not reference resources.
  const auto& root_pass = render_pass_list.back();

  // Filters can paint arbitrary content from a single quad: ALPHA_THRESHOLD
  // masks the pass to an arbitrary region, and REFERENCE applies an arbitrary
  // PaintFilter graph. Both are deserialized from untrusted clients. A client
  // cannot currently get filters on its own root pass rendered, because
  // SurfaceAggregator drops them when it merges the pass into its embedder and
  // only the embedder can prevent that merge, so this is defense in depth
  // against that changing. It costs little in practice: a blank document never
  // sets filters on its root pass, and legitimate CSS filters render into their
  // own pass, which the check above already treats as non-empty.
  if (!root_pass->filters.IsEmpty() || !root_pass->backdrop_filters.IsEmpty()) {
    return true;
  }

  // A frame that does not damage all of `output_rect` can leave pixels outside
  // `damage_rect` on screen from earlier frames until the display compositor
  // next damages and redraws the surface in full: SurfaceAggregator uses the
  // root pass damage as the surface's damage, and DirectRenderer scissors the
  // redraw to it. A client could otherwise draw content one small rect per
  // frame, or erase rects back to the embedder's background color with an empty
  // quad list, while every individual frame looks empty here. A blank document
  // always damages its whole viewport: the first frame of a surface is fully
  // damaged, and nothing in a blank document can damage a sub-rect on its own.
  if (!root_pass->damage_rect.Contains(root_pass->output_rect)) {
    return true;
  }

  if (root_pass->quad_list.empty()) {
    return false;
  }
  const bool has_single_solid_color_quad =
      root_pass->quad_list.size() == 1 &&
      root_pass->quad_list.front()->material == DrawQuad::Material::kSolidColor;
  return !has_single_solid_color_quad;
}

void CompositorFrame::AsValueInto(base::trace_event::TracedValue* value) const {
  value->BeginDictionary("metadata");
  metadata.AsValueInto(value);
  value->EndDictionary();

  value->SetInteger("resource_list_size", resource_list.size());
  size_t index = 0;
  std::unordered_map<ResourceId, size_t> resource_id_to_index_map;
  value->BeginArray("resource_list");
  for (const auto& resource : resource_list) {
    value->BeginDictionary();
    resource.AsValueInto(value);
    resource_id_to_index_map.emplace(resource.id, index);
    value->SetInteger("index", index++);
    value->EndDictionary();
  }
  value->EndArray();

  value->SetInteger("render_pass_list_size", render_pass_list.size());
  index = 0;
  value->BeginArray("render_pass_list");
  for (const auto& render_pass : render_pass_list) {
    value->BeginDictionary();
    render_pass->AsValueInto(value, resource_id_to_index_map);
    value->SetInteger("index", index++);
    value->EndDictionary();
  }
  value->EndArray();
}

std::string CompositorFrame::ToString() const {
  base::trace_event::TracedValueJSON value;
  AsValueInto(&value);
  return value.ToFormattedJSON();
}

}  // namespace viz
