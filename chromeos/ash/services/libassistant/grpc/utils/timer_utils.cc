// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/libassistant/grpc/utils/timer_utils.h"

#include "base/time/time.h"
#include "chromeos/ash/services/libassistant/public/cpp/assistant_timer.h"
#include "chromeos/assistant/internal/proto/shared/proto/timer_params.pb.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/delegate/event_handler_interface.pb.h"

namespace ash::libassistant {

namespace {

using assistant::AssistantTimer;
using assistant::AssistantTimerState;

AssistantTimerState GetTimerState(assistant_client::Timer::State state) {
  switch (state) {
    case assistant_client::Timer::State::UNKNOWN:
      return AssistantTimerState::kUnknown;
    case assistant_client::Timer::State::SCHEDULED:
      return AssistantTimerState::kScheduled;
    case assistant_client::Timer::State::PAUSED:
      return AssistantTimerState::kPaused;
    case assistant_client::Timer::State::FIRED:
      return AssistantTimerState::kFired;
  }
}

}  // namespace

::assistant::api::OnAlarmTimerEventRequest
CreateOnAlarmTimerEventRequestProtoForV1(
    const std::vector<AssistantTimer>& all_curr_timers) {
  ::assistant::api::OnAlarmTimerEventRequest proto;
  auto* timer_params = proto.mutable_event()
                           ->mutable_on_timer_state_changed()
                           ->mutable_timer_params();

  for (const auto& t : all_curr_timers) {
    auto* timer = timer_params->add_timer();
    ConvertAssistantTimerToProtoTimer(t, timer);
  }

  return proto;
}

std::vector<AssistantTimer> ConstructAssistantTimersFromProto(
    const ::assistant::api::params::TimerParams& timer_params) {
  std::vector<AssistantTimer> assistant_timers;
  for (const auto& proto_timer : timer_params.timer()) {
    AssistantTimer assistant_timer;
    ConvertProtoTimerToAssistantTimer(proto_timer, &assistant_timer);
    assistant_timers.emplace_back(std::move(assistant_timer));
  }

  return assistant_timers;
}

void ConvertAssistantTimerToProtoTimer(
    const AssistantTimer& input,
    ::assistant::api::params::Timer* output) {
  output->set_timer_id(input.id);
  if (output->TimerStatus_IsValid(static_cast<int>(input.state))) {
    output->set_status(
        static_cast<::assistant::api::params::Timer::TimerStatus>(input.state));
  }
  output->set_original_duration(input.original_duration.InMilliseconds());

  if (input.state == AssistantTimerState::kPaused) {
    output->set_remaining_duration(input.remaining_time.InMilliseconds());
  } else {
    output->set_expire_time(
        (input.fire_time - base::Time::UnixEpoch()).InMilliseconds());
  }

  output->set_label(input.label);
}

void ConvertProtoTimerToAssistantTimer(
    const ::assistant::api::params::Timer& input,
    AssistantTimer* output) {
  output->id = input.timer_id();
  output->state = static_cast<AssistantTimerState>(input.status());
  output->original_duration = base::Milliseconds(input.original_duration());

  if (input.has_remaining_duration()) {
    // Remaining time in ms when paused or used in timer.CREATE_TIMER.
    output->remaining_time = base::Milliseconds(input.remaining_duration());
  } else {
    // Time the timer is scheduled to expire in ms since the unix epoch
    // when not paused or used in timer.UPDATE_TIMER.
    output->fire_time =
        base::Time::UnixEpoch() + base::Milliseconds(input.expire_time());

    // This is a workaround for not breaking the current timer functionality.
    // We have to make sure that |remaining_time| is always set since it is
    // used to update the timer. In V2 proto, |remaining_time| and |fire_time|
    // are mutally exclusive.
    output->remaining_time = output->fire_time - base::Time::Now();
  }

  output->label = input.label();
}

std::vector<AssistantTimer> GetAllCurrentTimersFromEvents(
    const std::vector<assistant_client::AlarmTimerManager::Event>& events) {
  std::vector<AssistantTimer> result;
  for (const auto& event : events) {
    // Note that we currently only handle timers, alarms are unsupported.
    if (event.type != assistant_client::AlarmTimerEvent::TIMER)
      continue;

    AssistantTimer timer;
    timer.id = event.timer_data.timer_id;
    timer.label = event.timer_data.label;
    timer.state = GetTimerState(event.timer_data.state);
    timer.original_duration =
        base::Milliseconds(event.timer_data.original_duration_ms);

    // LibAssistant provides |fire_time_ms| as an offset from unix epoch.
    timer.fire_time = base::Time::UnixEpoch() +
                      base::Milliseconds(event.timer_data.fire_time_ms);

    // If the |timer| is paused, LibAssistant will specify the amount of time
    // remaining. Otherwise we calculate it based on |fire_time|.
    timer.remaining_time =
        timer.state == AssistantTimerState::kPaused
            ? base::Milliseconds(event.timer_data.remaining_duration_ms)
            : timer.fire_time - base::Time::Now();

    result.push_back(std::move(timer));
  }

  return result;
}

}  // namespace ash::libassistant
