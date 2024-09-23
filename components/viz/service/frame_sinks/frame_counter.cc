// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/frame_sinks/frame_counter.h"

#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/time/time.h"
#include "services/viz/privileged/mojom/compositing/frame_sink_manager.mojom.h"
#include "services/viz/privileged/mojom/compositing/frame_sinks_metrics_recorder.mojom.h"

namespace viz {
namespace {

// Max number of frame count records. It is 1800s when bucket size if 1s and
// no tests should run longer than 1800s.
constexpr size_t kMaxFrameRecords = 1800u;

}  // namespace

FrameCounter::FrameCounter(base::TimeTicks start_time,
                           base::TimeDelta bucket_size)
    : start_time_(start_time), bucket_size_(bucket_size) {}

FrameCounter::~FrameCounter() = default;

void FrameCounter::AddFrameSink(const FrameSinkId& frame_sink_id,
                                mojom::CompositorFrameSinkType type,
                                bool is_root,
                                std::string_view debug_label) {
  DCHECK(!base::Contains(frame_sink_data_, frame_sink_id));

  auto per_sink_data = mojom::FrameCountingPerSinkData::New(
      type, is_root, static_cast<std::string>(debug_label), 0,
      std::vector<uint16_t>());
  per_sink_data->presented_frames.reserve(kMaxFrameRecords);

  frame_sink_data_[frame_sink_id] = std::move(per_sink_data);
}

void FrameCounter::AddPresentedFrame(const FrameSinkId& frame_sink_id,
                                     base::TimeTicks present_timestamp) {
  auto& per_sink_data = frame_sink_data_[frame_sink_id];
  DCHECK(!per_sink_data.is_null());

  if (start_time_ > present_timestamp) {
    LOG(WARNING) << "Presentation timestamp is less than start time, skip.";
    return;
  }

  size_t bucket_index =
      (present_timestamp - start_time_).InSeconds() / bucket_size_.InSeconds();
  DCHECK_LT(bucket_index, kMaxFrameRecords);

  auto& presented_frames = per_sink_data->presented_frames;

  if (presented_frames.empty()) {
    CHECK_LT(bucket_index, std::numeric_limits<uint16_t>::max());
    per_sink_data->start_bucket = bucket_index;
  }

  const size_t relative_index = bucket_index - per_sink_data->start_bucket;
  if (relative_index >= presented_frames.size()) {
    presented_frames.resize(relative_index + 1, 0u);
  }

  ++presented_frames[relative_index];
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

void FrameCounter::SetFrameSinkDebugLabel(const FrameSinkId& frame_sink_id,
                                          std::string_view debug_label) {
  // SetFrameSinkDebugLabel could happen before a frame sink is created. Ignore
  // the call and the debug label info will be added when AddFrameSink is
  // called.
  auto it = frame_sink_data_.find(frame_sink_id);
  if (it == frame_sink_data_.end()) {
    return;
  }

  it->second->debug_label = static_cast<std::string>(debug_label);
}

}  // namespace viz
