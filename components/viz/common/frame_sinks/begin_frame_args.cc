// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/frame_sinks/begin_frame_args.h"

#include "base/trace_event/traced_value.h"

namespace viz {

const char* BeginFrameArgs::TypeToString(BeginFrameArgsType type) {
  switch (type) {
    case BeginFrameArgs::INVALID:
      return "INVALID";
    case BeginFrameArgs::NORMAL:
      return "NORMAL";
    case BeginFrameArgs::MISSED:
      return "MISSED";
    case BeginFrameArgs::BEGIN_FRAME_ARGS_TYPE_MAX:
      return "BEGIN_FRAME_ARGS_TYPE_MAX";
  }
  NOTREACHED();
  return "???";
}

constexpr uint64_t BeginFrameArgs::kInvalidFrameNumber;
constexpr uint64_t BeginFrameArgs::kStartingFrameNumber;

BeginFrameArgs::BeginFrameArgs()
    : frame_time(base::TimeTicks::Min()),
      deadline(base::TimeTicks::Min()),
      interval(base::TimeDelta::FromMicroseconds(-1)),
      source_id(0),
      sequence_number(kInvalidFrameNumber),
      type(BeginFrameArgs::INVALID),
      on_critical_path(true),
      animate_only(false) {}

BeginFrameArgs::BeginFrameArgs(uint64_t source_id,
                               uint64_t sequence_number,
                               base::TimeTicks frame_time,
                               base::TimeTicks deadline,
                               base::TimeDelta interval,
                               BeginFrameArgs::BeginFrameArgsType type)
    : frame_time(frame_time),
      deadline(deadline),
      interval(interval),
      source_id(source_id),
      sequence_number(sequence_number),
      type(type),
      on_critical_path(true),
      animate_only(false) {
  DCHECK_LE(kStartingFrameNumber, sequence_number);
}

BeginFrameArgs::BeginFrameArgs(const BeginFrameArgs& args) = default;
BeginFrameArgs& BeginFrameArgs::operator=(const BeginFrameArgs& args) = default;

BeginFrameArgs BeginFrameArgs::Create(BeginFrameArgs::CreationLocation location,
                                      uint64_t source_id,
                                      uint64_t sequence_number,
                                      base::TimeTicks frame_time,
                                      base::TimeTicks deadline,
                                      base::TimeDelta interval,
                                      BeginFrameArgs::BeginFrameArgsType type) {
  DCHECK_NE(type, BeginFrameArgs::INVALID);
  DCHECK_NE(type, BeginFrameArgs::BEGIN_FRAME_ARGS_TYPE_MAX);
#ifdef NDEBUG
  return BeginFrameArgs(source_id, sequence_number, frame_time, deadline,
                        interval, type);
#else
  BeginFrameArgs args = BeginFrameArgs(source_id, sequence_number, frame_time,
                                       deadline, interval, type);
  args.created_from = location;
  return args;
#endif
}

std::unique_ptr<base::trace_event::ConvertableToTraceFormat>
BeginFrameArgs::AsValue() const {
  std::unique_ptr<base::trace_event::TracedValue> state(
      new base::trace_event::TracedValue());
  AsValueInto(state.get());
  return std::move(state);
}

void BeginFrameArgs::AsValueInto(base::trace_event::TracedValue* state) const {
  state->SetString("type", "BeginFrameArgs");
  state->SetString("subtype", TypeToString(type));
  state->SetInteger("source_id", source_id);
  state->SetInteger("sequence_number", sequence_number);
  state->SetDouble("frame_time_us", frame_time.since_origin().InMicroseconds());
  state->SetDouble("deadline_us", deadline.since_origin().InMicroseconds());
  state->SetDouble("interval_us", interval.InMicroseconds());
#ifndef NDEBUG
  state->SetString("created_from", created_from.ToString());
#endif
  state->SetBoolean("on_critical_path", on_critical_path);
  state->SetBoolean("animate_only", animate_only);
}

// This is a hard-coded deadline adjustment that assumes 60Hz, to be used in
// cases where a good estimated draw time is not known. Using 1/3 of the vsync
// as the default adjustment gives the Browser the last 1/3 of a frame to
// produce output, the Renderer Impl thread the middle 1/3 of a frame to produce
// ouput, and the Renderer Main thread the first 1/3 of a frame to produce
// output.
base::TimeDelta BeginFrameArgs::DefaultEstimatedParentDrawTime() {
  return base::TimeDelta::FromMicroseconds(16666 / 3);
}

base::TimeDelta BeginFrameArgs::DefaultInterval() {
  return base::TimeDelta::FromMicroseconds(16666);
}

BeginFrameAck::BeginFrameAck()
    : source_id(0),
      sequence_number(BeginFrameArgs::kInvalidFrameNumber),
      has_damage(false) {}

BeginFrameAck::BeginFrameAck(const BeginFrameArgs& args, bool has_damage)
    : BeginFrameAck(args.source_id,
                    args.sequence_number,
                    has_damage,
                    args.trace_id) {}

BeginFrameAck::BeginFrameAck(uint64_t source_id,
                             uint64_t sequence_number,
                             bool has_damage,
                             int64_t trace_id)
    : source_id(source_id),
      sequence_number(sequence_number),
      trace_id(trace_id),
      has_damage(has_damage) {
  DCHECK_LT(BeginFrameArgs::kInvalidFrameNumber, sequence_number);
}

// static
BeginFrameAck BeginFrameAck::CreateManualAckWithDamage() {
  return BeginFrameAck(BeginFrameArgs::kManualSourceId,
                       BeginFrameArgs::kStartingFrameNumber, true);
}

}  // namespace viz
