// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <fuzzer/FuzzedDataProvider.h>

#include <vector>

#include "base/command_line.h"
#include "components/viz/common/hit_test/hit_test_query.h"
#include "ui/gfx/geometry/test/fuzzer_util.h"

namespace {

void AddHitTestRegion(FuzzedDataProvider* fuzz,
                      std::vector<viz::AggregatedHitTestRegion>* regions,
                      std::vector<viz::FrameSinkId>* frame_sink_ids,
                      const uint32_t depth = 0) {
  constexpr uint32_t kMaxDepthAllowed = 25;
  if (fuzz->remaining_bytes() < sizeof(viz::AggregatedHitTestRegion))
    return;
  viz::FrameSinkId frame_sink_id(fuzz->ConsumeIntegralInRange<uint32_t>(
                                     1, std::numeric_limits<uint32_t>::max()),
                                 fuzz->ConsumeIntegralInRange<uint32_t>(
                                     1, std::numeric_limits<uint32_t>::max()));
  uint32_t flags = fuzz->ConsumeIntegral<uint32_t>();
  // The reasons' value is kNotAsyncHitTest if the flag's value is kHitTestAsk.
  uint32_t reasons = (flags & viz::HitTestRegionFlags::kHitTestAsk)
                         ? fuzz->ConsumeIntegralInRange<uint32_t>(
                               1, std::numeric_limits<uint32_t>::max())
                         : viz::AsyncHitTestReasons::kNotAsyncHitTest;
  gfx::Rect rect(
      fuzz->ConsumeIntegralInRange<int>(std::numeric_limits<int>::min() + 1,
                                        std::numeric_limits<int>::max()),
      fuzz->ConsumeIntegralInRange<int>(std::numeric_limits<int>::min() + 1,
                                        std::numeric_limits<int>::max()),
      fuzz->ConsumeIntegral<int>(), fuzz->ConsumeIntegral<int>());
  int32_t child_count =
      depth < kMaxDepthAllowed ? fuzz->ConsumeIntegralInRange(0, 10) : 0;
  regions->emplace_back(frame_sink_id, flags, rect,
                        gfx::ConsumeTransform(*fuzz), child_count, reasons);
  // Always add the first frame sink id, because the root needs to be in the
  // list of FrameSinkId.
  if (regions->size() == 1 || fuzz->ConsumeBool())
    frame_sink_ids->push_back(frame_sink_id);
  while (child_count-- > 0)
    AddHitTestRegion(fuzz, regions, frame_sink_ids, depth + 1);
}

class Environment {
 public:
  Environment() { base::CommandLine::Init(0, nullptr); }
};

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t num_bytes) {
  // Initialize the environment only once.
  static Environment environment;

  // If there isn't enough memory to have a single AggregatedHitTestRegion, then
  // skip.
  if (num_bytes < sizeof(viz::AggregatedHitTestRegion))
    return 0;

  // Create the list of AggregatedHitTestRegion objects.
  std::vector<viz::AggregatedHitTestRegion> regions;
  std::vector<viz::FrameSinkId> frame_sink_ids;
  FuzzedDataProvider fuzz(data, num_bytes);
  AddHitTestRegion(&fuzz, &regions, &frame_sink_ids);

  // Create the HitTestQuery and send hit-test data.
  viz::HitTestQuery query{std::nullopt};
  query.OnAggregatedHitTestRegionListUpdated(regions);

  for (float x = 0; x < 1000.; x += 10) {
    for (float y = 0; y < 1000.; y += 10) {
      gfx::PointF location(x, y);
      query.FindTargetForLocation(viz::EventSource::MOUSE, location);
      query.TransformLocationForTarget(frame_sink_ids, location, &location);
    }
  }

  return 0;
}
