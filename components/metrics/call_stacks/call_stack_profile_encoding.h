// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_CALL_STACKS_CALL_STACK_PROFILE_ENCODING_H_
#define COMPONENTS_METRICS_CALL_STACKS_CALL_STACK_PROFILE_ENCODING_H_

#include "base/profiler/call_stack_profile_params.h"
#include "base/profiler/process_type.h"
#include "third_party/metrics_proto/sampled_profile.pb.h"

namespace metrics {

// Translates CallStackProfileParams's process to the corresponding execution
// context Process.
Process ToExecutionContextProcess(base::ProfilerProcessType process);

// Translates CallStackProfileParams's thread to the corresponding
// SampledProfile Thread.
Thread ToExecutionContextThread(base::ProfilerThreadType thread);

// Translates CallStackProfileParams's trigger to the corresponding
// SampledProfile TriggerEvent.
SampledProfile::TriggerEvent ToSampledProfileTriggerEvent(
    base::CallStackProfileParams::Trigger trigger);

}  // namespace metrics

#endif  // COMPONENTS_METRICS_CALL_STACKS_CALL_STACK_PROFILE_ENCODING_H_
