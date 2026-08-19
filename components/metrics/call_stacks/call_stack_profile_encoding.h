// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_CALL_STACKS_CALL_STACK_PROFILE_ENCODING_H_
#define COMPONENTS_METRICS_CALL_STACKS_CALL_STACK_PROFILE_ENCODING_H_

#include "components/metrics/public/mojom/profile_params.mojom.h"
#include "components/sampling_profiler/process_type.h"
#include "third_party/metrics_proto/sampled_profile.pb.h"

namespace metrics {

// Translates CallStackProfileParams's process to the corresponding execution
// context Process.
Process ToExecutionContextProcess(
    sampling_profiler::ProfilerProcessType process);

// Translates CallStackProfileParams's thread to the corresponding
// SampledProfile Thread.
Thread ToExecutionContextThread(sampling_profiler::ProfilerThreadType thread);

// Translates mojom::TriggerEvent to the corresponding SampledProfile
// TriggerEvent.
SampledProfile::TriggerEvent ToSampledProfileTriggerEvent(
    mojom::TriggerEvent trigger_event);

// Translates SampledProfile TriggerEvent to the corresponding
// mojom::TriggerEvent.
mojom::TriggerEvent ToMojomTriggerEvent(
    SampledProfile::TriggerEvent trigger_event);

}  // namespace metrics

#endif  // COMPONENTS_METRICS_CALL_STACKS_CALL_STACK_PROFILE_ENCODING_H_
