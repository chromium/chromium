// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/aggregated_frame.h"

#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"

namespace viz {

AggregatedFrame::AggregatedFrame() = default;
AggregatedFrame::AggregatedFrame(AggregatedFrame&& other) = default;
AggregatedFrame::~AggregatedFrame() = default;

AggregatedFrame& AggregatedFrame::operator=(AggregatedFrame&& other) = default;

void AggregatedFrame::AsValueInto(base::trace_event::TracedValue* value) const {
  value->SetBoolean("has_copy_requests", has_copy_requests);

  // Quad data can be quite large, so only dump render passes if we are
  // logging verbosely or viz.quads tracing category is enabled.
  bool quads_enabled = VLOG_IS_ON(3);
  if (!quads_enabled) {
    TRACE_EVENT_CATEGORY_GROUP_ENABLED(TRACE_DISABLED_BY_DEFAULT("viz.quads"),
                                       &quads_enabled);
  }
  if (quads_enabled) {
    value->BeginArray("render_passes");
    for (const auto& render_pass : render_pass_list) {
      value->BeginDictionary();
      render_pass->AsValueInto(value);
      value->EndDictionary();
    }
    value->EndArray();
  }
}

std::string AggregatedFrame::ToString() const {
  base::trace_event::TracedValueJSON value;
  AsValueInto(&value);
  return value.ToFormattedJSON();
}

}  // namespace viz
