// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/test/begin_frame_args_test.h"

#include <stdint.h>

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
      base::TimeTicks() + base::TimeDelta::FromMicroseconds(frame_time),
      base::TimeTicks() + base::TimeDelta::FromMicroseconds(deadline),
      base::TimeDelta::FromMicroseconds(interval), BeginFrameArgs::NORMAL);
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
      base::TimeTicks() + base::TimeDelta::FromMicroseconds(frame_time),
      base::TimeTicks() + base::TimeDelta::FromMicroseconds(deadline),
      base::TimeDelta::FromMicroseconds(interval), type);
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
  return (lhs.type == rhs.type) && (lhs.source_id == rhs.source_id) &&
         (lhs.sequence_number == rhs.sequence_number) &&
         (lhs.frame_time == rhs.frame_time) && (lhs.deadline == rhs.deadline) &&
         (lhs.interval == rhs.interval);
}

::std::ostream& operator<<(::std::ostream& os, const BeginFrameArgs& args) {
  PrintTo(args, &os);
  return os;
}

void PrintTo(const BeginFrameArgs& args, ::std::ostream* os) {
  *os << "BeginFrameArgs(" << BeginFrameArgs::TypeToString(args.type) << ", "
      << args.source_id << ", " << args.sequence_number << ", "
      << args.frame_time.since_origin().InMicroseconds() << ", "
      << args.deadline.since_origin().InMicroseconds() << ", "
      << args.interval.InMicroseconds() << "us)";
}

bool operator==(const BeginFrameAck& lhs, const BeginFrameAck& rhs) {
  return (lhs.source_id == rhs.source_id) &&
         (lhs.sequence_number == rhs.sequence_number) &&
         (lhs.has_damage == rhs.has_damage);
}

::std::ostream& operator<<(::std::ostream& os, const BeginFrameAck& args) {
  PrintTo(args, &os);
  return os;
}

void PrintTo(const BeginFrameAck& ack, ::std::ostream* os) {
  *os << "BeginFrameAck(" << ack.source_id << ", " << ack.sequence_number
      << ", " << ack.has_damage << ")";
}

}  // namespace viz
