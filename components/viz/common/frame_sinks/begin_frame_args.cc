// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/frame_sinks/begin_frame_args.h"

#include <utility>

#include "base/trace_event/interned_args_helper.h"
#include "base/trace_event/traced_value.h"
#include "base/tracing/protos/chrome_track_event.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/source_location.pbzero.h"

namespace viz {

const char* BeginFrameArgs::TypeToString(BeginFrameArgsType type) {
  switch (type) {
    case BeginFrameArgs::INVALID:
      return "INVALID";
    case BeginFrameArgs::NORMAL:
      return "NORMAL";
    case BeginFrameArgs::MISSED:
      return "MISSED";
  }
  NOTREACHED_IN_MIGRATION();
  return "???";
}

namespace {
perfetto::protos::pbzero::BeginFrameArgsV2::BeginFrameArgsType
TypeToProtozeroEnum(BeginFrameArgs::BeginFrameArgsType type) {
  using pbzeroType = perfetto::protos::pbzero::BeginFrameArgsV2;
  switch (type) {
    case BeginFrameArgs::INVALID:
      return pbzeroType::BEGIN_FRAME_ARGS_TYPE_INVALID;
    case BeginFrameArgs::NORMAL:
      return pbzeroType::BEGIN_FRAME_ARGS_TYPE_NORMAL;
    case BeginFrameArgs::MISSED:
      return pbzeroType::BEGIN_FRAME_ARGS_TYPE_MISSED;
  }
  NOTREACHED_IN_MIGRATION();
  return pbzeroType::BEGIN_FRAME_ARGS_TYPE_UNSPECIFIED;
}
}  // namespace

constexpr uint64_t BeginFrameArgs::kInvalidFrameNumber;
constexpr uint64_t BeginFrameArgs::kStartingFrameNumber;

BeginFrameId::BeginFrameId()
    : source_id(0), sequence_number(BeginFrameArgs::kInvalidFrameNumber) {}

BeginFrameId::BeginFrameId(const BeginFrameId& id) = default;
BeginFrameId& BeginFrameId::operator=(const BeginFrameId& id) = default;

BeginFrameId::BeginFrameId(uint64_t source_id, uint64_t sequence_number)
    : source_id(source_id), sequence_number(sequence_number) {}

bool BeginFrameId::IsNextInSequenceTo(const BeginFrameId& previous) const {
  return (source_id == previous.source_id &&
          sequence_number > previous.sequence_number);
}

bool BeginFrameId::IsSequenceValid() const {
  return (BeginFrameArgs::kInvalidFrameNumber != sequence_number);
}

std::string BeginFrameId::ToString() const {
  base::trace_event::TracedValueJSON value;
  value.SetInteger("source_id", source_id);
  value.SetInteger("sequence_number", sequence_number);
  return value.ToJSON();
}

PossibleDeadline::PossibleDeadline(int64_t vsync_id,
                                   base::TimeDelta latch_delta,
                                   base::TimeDelta present_delta)
    : vsync_id(vsync_id),
      latch_delta(latch_delta),
      present_delta(present_delta) {}
PossibleDeadline::PossibleDeadline(const PossibleDeadline& other) = default;
PossibleDeadline::PossibleDeadline(PossibleDeadline&& other) = default;
PossibleDeadline::~PossibleDeadline() = default;
PossibleDeadline& PossibleDeadline::operator=(const PossibleDeadline& other) =
    default;
PossibleDeadline& PossibleDeadline::operator=(PossibleDeadline&& other) =
    default;

PossibleDeadlines::PossibleDeadlines(size_t preferred_index)
    : preferred_index(preferred_index) {}
PossibleDeadlines::PossibleDeadlines(const PossibleDeadlines& other) = default;
PossibleDeadlines::PossibleDeadlines(PossibleDeadlines&& other) = default;
PossibleDeadlines::~PossibleDeadlines() = default;
PossibleDeadlines& PossibleDeadlines::operator=(
    const PossibleDeadlines& other) = default;
PossibleDeadlines& PossibleDeadlines::operator=(PossibleDeadlines&& other) =
    default;

const PossibleDeadline& PossibleDeadlines::GetPreferredDeadline() const {
  return deadlines[preferred_index];
}

BeginFrameArgs::BeginFrameArgs()
    : frame_time(base::TimeTicks::Min()),
      deadline(base::TimeTicks::Min()),
      interval(base::Microseconds(-1)),
      frame_id(BeginFrameId(0, kInvalidFrameNumber)) {}

BeginFrameArgs::~BeginFrameArgs() = default;

BeginFrameArgs::BeginFrameArgs(uint64_t source_id,
                               uint64_t sequence_number,
                               base::TimeTicks frame_time,
                               base::TimeTicks deadline,
                               base::TimeDelta interval,
                               BeginFrameArgs::BeginFrameArgsType type)
    : frame_time(frame_time),
      deadline(deadline),
      interval(interval),
      frame_id(BeginFrameId(source_id, sequence_number)),
      type(type) {
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
  state->SetInteger("source_id", frame_id.source_id);
  state->SetInteger("sequence_number", frame_id.sequence_number);
  state->SetInteger("frames_throttled_since_last", frames_throttled_since_last);
  state->SetDouble("frame_time_us",
                   frame_time.since_origin().InMicrosecondsF());
  state->SetDouble("deadline_us", deadline.since_origin().InMicrosecondsF());
  state->SetDouble("interval_us", interval.InMicrosecondsF());
#ifndef NDEBUG
  state->SetString("created_from", created_from.ToString());
#endif
  state->SetBoolean("on_critical_path", on_critical_path);
  state->SetBoolean("animate_only", animate_only);
  state->SetBoolean("has_possible_deadlines", !!possible_deadlines);
}

void BeginFrameArgs::AsProtozeroInto(
    perfetto::EventContext& ctx,
    perfetto::protos::pbzero::BeginFrameArgsV2* state) const {
  state->set_type(TypeToProtozeroEnum(type));
  state->set_source_id(frame_id.source_id);
  state->set_sequence_number(frame_id.sequence_number);
  state->set_frames_throttled_since_last(frames_throttled_since_last);
  state->set_frame_time_us(frame_time.since_origin().InMicroseconds());
  state->set_deadline_us(deadline.since_origin().InMicroseconds());
  state->set_interval_delta_us(interval.InMicroseconds());
  state->set_on_critical_path(on_critical_path);
  state->set_animate_only(animate_only);
#ifndef NDEBUG
  state->set_source_location_iid(
      base::trace_event::InternedSourceLocation::Get(&ctx, created_from));
#endif
}

std::string BeginFrameArgs::ToString() const {
  base::trace_event::TracedValueJSON value;
  AsValueInto(&value);
  return value.ToJSON();
}

BeginFrameAck::BeginFrameAck(const BeginFrameArgs& args, bool has_damage)
    : frame_id(args.frame_id),
      trace_id(args.trace_id),
      has_damage(has_damage) {}

BeginFrameAck::BeginFrameAck(uint64_t source_id,
                             uint64_t sequence_number,
                             bool has_damage,
                             int64_t trace_id)
    : frame_id(BeginFrameId(source_id, sequence_number)),
      trace_id(trace_id),
      has_damage(has_damage) {
  DCHECK(frame_id.IsSequenceValid());
}

// static
BeginFrameAck BeginFrameAck::CreateManualAckWithDamage() {
  return BeginFrameAck(BeginFrameArgs::kManualSourceId,
                       BeginFrameArgs::kStartingFrameNumber, true);
}

}  // namespace viz
