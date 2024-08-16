// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_QUADS_FRAME_INTERVAL_INPUTS_H_
#define COMPONENTS_VIZ_COMMON_QUADS_FRAME_INTERVAL_INPUTS_H_

#include <cstdint>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "components/viz/common/viz_common_export.h"

namespace viz {

// Information passed from viz clients used to compute the overall ideal frame
// interval. Information should generally be descriptive of information in
// clients, but clients should generally avoid doing aggregation itself to allow
// viz decide using full description of all clients. For example clients should
// not compute a preferred frame interval unless the content it's displaying
// actually has a fixed or specified frame interval.
// This file should generally avoid OS build flags checks (eg IS_ANDROID),
// though exception can be made for if the information is expensive to compute,
// or if feature is really specific to a specific platform.

// The type of content that has a fixed or specified frame interval.
enum class ContentFrameIntervalType {
  kVideo,
  kAnimatingImage,  // Gifs.
  kScrollBarFadeOutAnimation,
};

VIZ_COMMON_EXPORT std::string ContentFrameIntervalTypeToString(
    ContentFrameIntervalType type);

struct VIZ_COMMON_EXPORT ContentFrameIntervalInfo {
  // Type of content that has fixed content frame interval.
  ContentFrameIntervalType type = ContentFrameIntervalType::kVideo;

  // Content frame interval.
  base::TimeDelta frame_interval;

  // Number of _additional_ content this entry refers to. Eg if there are 2
  // videos with the same content frame interval, then they can share the same
  // entry and `duplicate_count` should be set to 1.
  uint32_t duplicate_count = 0u;
};

struct VIZ_COMMON_EXPORT FrameIntervalInputs {
  FrameIntervalInputs();
  FrameIntervalInputs(const FrameIntervalInputs& other);
  ~FrameIntervalInputs();

  // Frame time used to produce this frame. This is used to selectively ignore
  // clients that has not submitted a frame for some time.
  base::TimeTicks frame_time;

  // Ported from old system that client has received user input recently.
  // Ideally this should be limited to latency-sensitive animations only, such
  // as touch scrolling, in the future.
  bool has_input = false;

  // Any content that has a fixed or specified content frame interval can be
  // added to `content_interval_info`. If `content_interval_info` contains
  // information on _all_ updating content in this client , then client can
  // indicate this by setting `has_only_content_frame_interval_updates` to true.
  std::vector<ContentFrameIntervalInfo> content_interval_info;
  bool has_only_content_frame_interval_updates = false;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_QUADS_FRAME_INTERVAL_INPUTS_H_
