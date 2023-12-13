// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_LIBASSISTANT_GRPC_UTILS_TIMER_UTILS_H_
#define CHROMEOS_ASH_SERVICES_LIBASSISTANT_GRPC_UTILS_TIMER_UTILS_H_

#include "chromeos/ash/services/libassistant/public/cpp/assistant_timer.h"
#include "chromeos/assistant/internal/libassistant/shared_headers.h"

namespace assistant {
namespace api {

namespace params {
class Timer;
class TimerParams;
}  // namespace params

}  // namespace api
}  // namespace assistant

namespace ash::libassistant {

// `timer_params` contains the information of all the current timers.
std::vector<assistant::AssistantTimer> ConstructAssistantTimersFromProto(
    const ::assistant::api::params::TimerParams& timer_params);

void ConvertProtoTimerToAssistantTimer(
    const ::assistant::api::params::Timer& input,
    assistant::AssistantTimer* output);

}  // namespace ash::libassistant

#endif  // CHROMEOS_ASH_SERVICES_LIBASSISTANT_GRPC_UTILS_TIMER_UTILS_H_
