// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_CALL_STACKS_CALL_STACK_PROFILE_ENCODING_H_
#define COMPONENTS_METRICS_CALL_STACKS_CALL_STACK_PROFILE_ENCODING_H_

#include "components/metrics/call_stacks/call_stack_profile_params.h"
#include "third_party/metrics_proto/sampled_profile.pb.h"

namespace metrics {

// Translates CallStackProfileParams's process to the corresponding execution
// context Process.
Process ToExecutionContextProcess(CallStackProfileParams::Process process);

// Translates CallStackProfileParams's thread to the corresponding
// SampledProfile Thread.
Thread ToExecutionContextThread(CallStackProfileParams::Thread thread);

// Translates CallStackProfileParams's trigger to the corresponding
// SampledProfile TriggerEvent.
SampledProfile::TriggerEvent ToSampledProfileTriggerEvent(
    CallStackProfileParams::Trigger trigger);

}  // namespace metrics

#endif  // COMPONENTS_METRICS_CALL_STACKS_CALL_STACK_PROFILE_ENCODING_H_
