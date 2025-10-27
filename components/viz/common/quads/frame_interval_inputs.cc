// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/quads/frame_interval_inputs.h"

#include "base/trace_event/traced_value.h"

namespace viz {

std::string ContentFrameIntervalTypeToString(ContentFrameIntervalType type) {
  switch (type) {
    case ContentFrameIntervalType::kVideo:
      return "video";
    case ContentFrameIntervalType::kAnimatingImage:
      return "animating_image";
    case ContentFrameIntervalType::kScrollBarFadeOutAnimation:
      return "scrollbar_fade_out";
    case ContentFrameIntervalType::kCompositorScroll:
      return "compositor_scroll";
  }
}

FrameIntervalInputs::FrameIntervalInputs() = default;
FrameIntervalInputs::FrameIntervalInputs(const FrameIntervalInputs& other) =
    default;
FrameIntervalInputs::~FrameIntervalInputs() = default;

void ContentFrameIntervalInfo::AsValueInto(
    base::trace_event::TracedValue* value) const {
  value->SetString("type", ContentFrameIntervalTypeToString(type));
  value->SetDouble("frame_interval_us", frame_interval.InMicroseconds());
  value->SetInteger("duplicate_count", duplicate_count);
}

void FrameIntervalInputs::AsValueInto(
    base::trace_event::TracedValue* value) const {
  value->SetDouble("frame_time_us",
                   (frame_time - base::TimeTicks()).InMicroseconds());
  value->SetBoolean("has_input", has_input);
  value->SetBoolean("has_user_input", has_user_input);
  value->SetDouble("major_scroll_speed_in_pixels_per_second",
                   major_scroll_speed_in_pixels_per_second);
  value->BeginArray("content_interval_info");
  for (const auto& info : content_interval_info) {
    value->BeginDictionary();
    info.AsValueInto(value);
    value->EndDictionary();
  }
  value->EndArray();
  value->SetBoolean("has_only_content_frame_interval_updates",
                    has_only_content_frame_interval_updates);
}

}  // namespace viz
