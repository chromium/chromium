// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/base/buffering_state.h"

#include <sstream>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "media/base/timestamp_constants.h"

namespace chromecast {
namespace media {

BufferingConfig::BufferingConfig(
    base::TimeDelta low_level_threshold,
    base::TimeDelta high_level_threshold)
    : low_level_threshold_(low_level_threshold),
      high_level_threshold_(high_level_threshold) {
}

BufferingConfig::~BufferingConfig() {
}

BufferingState::BufferingState(const std::string& stream_id,
                               const scoped_refptr<BufferingConfig>& config,
                               const base::RepeatingClosure& state_changed_cb,
                               const HighLevelBufferCB& high_level_buffer_cb)
    : stream_id_(stream_id),
      config_(config),
      state_changed_cb_(state_changed_cb),
      high_level_buffer_cb_(high_level_buffer_cb),
      state_(kLowLevel),
      media_time_(::media::kNoTimestamp),
      max_rendering_time_(::media::kNoTimestamp),
      buffered_time_(::media::kNoTimestamp) {}

BufferingState::~BufferingState() {
}

void BufferingState::OnConfigChanged() {
  state_ = GetBufferLevelState();
}

void BufferingState::SetMediaTime(base::TimeDelta media_time) {
  media_time_ = media_time;
  switch (state_) {
    case kLowLevel:
    case kMediumLevel:
    case kHighLevel:
      UpdateState(GetBufferLevelState());
      break;
    case kEosReached:
      break;
  }
}

void BufferingState::SetMaxRenderingTime(base::TimeDelta max_rendering_time) {
  max_rendering_time_ = max_rendering_time;
}

base::TimeDelta BufferingState::GetMaxRenderingTime() const {
  return max_rendering_time_;
}

void BufferingState::SetBufferedTime(base::TimeDelta buffered_time) {
  buffered_time_ = buffered_time;
  switch (state_) {
    case kLowLevel:
    case kMediumLevel:
    case kHighLevel:
      UpdateState(GetBufferLevelState());
      break;
    case kEosReached:
      break;
  }
}

void BufferingState::NotifyEos() {
  UpdateState(kEosReached);
}

void BufferingState::NotifyMaxCapacity(base::TimeDelta buffered_time) {
  if (media_time_ == ::media::kNoTimestamp ||
      buffered_time == ::media::kNoTimestamp) {
    LOG(WARNING) << "Max capacity with no timestamp";
    return;
  }
  base::TimeDelta buffer_duration = buffered_time - media_time_;
  if (buffer_duration < config_->high_level())
    high_level_buffer_cb_.Run(buffer_duration);
}

static const char* StateToString(BufferingState::State state) {
  switch (state) {
    case BufferingState::kLowLevel:
      return "kLowLevel";
    case BufferingState::kMediumLevel:
      return "kMediumLevel";
    case BufferingState::kHighLevel:
      return "kHighLevel";
    case BufferingState::kEosReached:
      return "kEosReached";
    default:
      NOTREACHED();
  }
}

static std::string TimeDeltaToString(const base::TimeDelta& t) {
  if (t == ::media::kNoTimestamp)
    return "kNoTimestamp";
  return base::NumberToString(t.InSecondsF());
}

std::string BufferingState::ToString() const {
  std::ostringstream s;
  s << stream_id_ << " state=" << StateToString(state_)
    << " media_time=" << TimeDeltaToString(media_time_)
    << " buffered_time=" << TimeDeltaToString(buffered_time_);
  return s.str();
}

BufferingState::State BufferingState::GetBufferLevelState() const {
  if (media_time_ == ::media::kNoTimestamp ||
      buffered_time_ == ::media::kNoTimestamp) {
    return kLowLevel;
  }

  base::TimeDelta buffer_duration = buffered_time_ - media_time_;
  if (buffer_duration <= config_->low_level())
    return kLowLevel;
  if (buffer_duration >= config_->high_level())
    return kHighLevel;
  return kMediumLevel;
}

void BufferingState::UpdateState(State new_state) {
  if (new_state == state_)
    return;

  state_ = new_state;
  if (!state_changed_cb_.is_null())
    state_changed_cb_.Run();
}

}  // namespace media
}  // namespace chromecast
