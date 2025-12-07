// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/quads/compositor_frame.h"

#include <unordered_map>

#include "base/containers/adapters.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "base/values.h"
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
