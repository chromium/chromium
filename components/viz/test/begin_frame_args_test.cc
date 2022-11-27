// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/test/begin_frame_args_test.h"

#include <stdint.h>
#include <ostream>

#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"

namespace viz {

BeginFrameArgs CreateBeginFrameArgsForTesting(
    BeginFrameArgs::CreationLocation location,
    uint64_t source_id,
    uint64_t sequence_number) {
  return CreateBeginFrameArgsForTesting(location, source_id, sequence_number,
                                        base::TimeTicks::Now());
}

BeginFrameArgs CreateBeginFrameArgsForTesting(
    BeginFrameArgs::CreationLocation location,
    uint64_t source_id,
    uint64_t sequence_number,
    base::TimeTicks frame_time) {
  return BeginFrameArgs::Create(
      location, source_id, sequence_number, frame_time,
      frame_time + BeginFrameArgs::DefaultInterval() -
          BeginFrameArgs::DefaultEstimatedDisplayDrawTime(
              BeginFrameArgs::DefaultInterval()),
      BeginFrameArgs::DefaultInterval(), BeginFrameArgs::NORMAL);
}

BeginFrameArgs CreateBeginFrameArgsForTesting(
    BeginFrameArgs::CreationLocation location,
    uint64_t source_id,
    uint64_t sequence_number,
    int64_t frame_time,
    int64_t deadline,
    int64_t interval) {
  return BeginFrameArgs::Create(
      location, source_id, sequence_number,
      base::TimeTicks() + base::Microseconds(frame_time),
      base::TimeTicks() + base::Microseconds(deadline),
      base::Microseconds(interval), BeginFrameArgs::NORMAL);
}

BeginFrameArgs CreateBeginFrameArgsForTesting(
    BeginFrameArgs::CreationLocation location,
    uint64_t source_id,
    uint64_t sequence_number,
    int64_t frame_time,
    int64_t deadline,
    int64_t interval,
    BeginFrameArgs::BeginFrameArgsType type) {
  return BeginFrameArgs::Create(
      location, source_id, sequence_number,
      base::TimeTicks() + base::Microseconds(frame_time),
      base::TimeTicks() + base::Microseconds(deadline),
      base::Microseconds(interval), type);
}

BeginFrameArgs CreateBeginFrameArgsForTesting(
    BeginFrameArgs::CreationLocation location,
    uint64_t source_id,
    uint64_t sequence_number,
    const base::TickClock* now_src) {
  base::TimeTicks now = now_src->NowTicks();
  return BeginFrameArgs::Create(
      location, source_id, sequence_number, now,
      now + BeginFrameArgs::DefaultInterval() -
          BeginFrameArgs::DefaultEstimatedDisplayDrawTime(
              BeginFrameArgs::DefaultInterval()),
      BeginFrameArgs::DefaultInterval(), BeginFrameArgs::NORMAL);
}

bool operator==(const BeginFrameArgs& lhs, const BeginFrameArgs& rhs) {
  return (lhs.type == rhs.type) && (lhs.frame_id == rhs.frame_id) &&
         (lhs.frame_time == rhs.frame_time) && (lhs.deadline == rhs.deadline) &&
         (lhs.interval == rhs.interval) &&
         (lhs.frames_throttled_since_last == rhs.frames_throttled_since_last);
}

::std::ostream& operator<<(::std::ostream& os, const BeginFrameArgs& args) {
  PrintTo(args, &os);
  return os;
}

void PrintTo(const BeginFrameArgs& args, ::std::ostream* os) {
  *os << "BeginFrameArgs(" << BeginFrameArgs::TypeToString(args.type) << ", "
      << args.frame_id.source_id << ", " << args.frame_id.sequence_number
      << ", " << args.frame_time.since_origin().InMicroseconds() << ", "
      << args.deadline.since_origin().InMicroseconds() << ", "
      << args.interval.InMicroseconds() << "us, "
      << args.frames_throttled_since_last << ")";
}

bool operator==(const BeginFrameAck& lhs, const BeginFrameAck& rhs) {
  return (lhs.frame_id == rhs.frame_id) && (lhs.has_damage == rhs.has_damage);
}

::std::ostream& operator<<(::std::ostream& os, const BeginFrameAck& args) {
  PrintTo(args, &os);
  return os;
}

void PrintTo(const BeginFrameAck& ack, ::std::ostream* os) {
  *os << "BeginFrameAck(" << ack.frame_id.source_id << ", "
      << ack.frame_id.sequence_number << ", " << ack.has_damage << ")";
}

}  // namespace viz
