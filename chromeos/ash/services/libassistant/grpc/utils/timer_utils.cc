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

}  // namespace

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

}  // namespace ash::libassistant
