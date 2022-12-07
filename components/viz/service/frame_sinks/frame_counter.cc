// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/frame_sinks/frame_counter.h"

#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/time/time.h"
#include "services/viz/privileged/mojom/compositing/frame_sink_manager.mojom.h"

namespace viz {
namespace {

// Max number of frame count records. It is 1800s when bucket size if 1s and
// no tests should run longer than 1800s.
constexpr size_t kMaxFrameRecords = 1800u;

}  // namespace

FrameCounter::FrameCounter(base::TimeDelta bucket_size)
    : start_time_(base::TimeTicks::Now()), bucket_size_(bucket_size) {}

FrameCounter::~FrameCounter() = default;

void FrameCounter::AddFrameSink(const FrameSinkId& frame_sink_id,
                                mojom::CompositorFrameSinkType type,
                                bool is_root) {
  DCHECK(!base::Contains(frame_sink_data_, frame_sink_id));

  auto per_sink_data = mojom::FrameCountingPerSinkData::New(
      type, is_root, std::vector<uint16_t>());
  per_sink_data->presented_frames.reserve(kMaxFrameRecords);

  frame_sink_data_[frame_sink_id] = std::move(per_sink_data);
}

void FrameCounter::AddPresentedFrame(const FrameSinkId& frame_sink_id,
                                     base::TimeTicks present_timestamp) {
  auto& per_sink_data = frame_sink_data_[frame_sink_id];
  DCHECK(!per_sink_data.is_null());

  DCHECK_LE(start_time_, present_timestamp);
  size_t bucket_index =
      (present_timestamp - start_time_).InSeconds() / bucket_size_.InSeconds();
  DCHECK_LT(bucket_index, kMaxFrameRecords);

  auto& presented_frames = per_sink_data->presented_frames;
  if (bucket_index >= presented_frames.size())
    presented_frames.resize(bucket_index + 1, 0u);

  ++presented_frames[bucket_index];
}

mojom::FrameCountingDataPtr FrameCounter::TakeData() {
  mojom::FrameCountingDataPtr data = mojom::FrameCountingData::New();
  for (auto& [sink_id, per_sink_data] : frame_sink_data_) {
    data->per_sink_data.emplace_back(std::move(per_sink_data));
  }
  frame_sink_data_.clear();
  return data;
}

void FrameCounter::SetFrameSinkType(const FrameSinkId& frame_sink_id,
                                    mojom::CompositorFrameSinkType type) {
  frame_sink_data_[frame_sink_id]->type = type;
}

}  // namespace viz
