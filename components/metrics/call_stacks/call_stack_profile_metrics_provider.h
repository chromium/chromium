// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_CALL_STACKS_CALL_STACK_PROFILE_METRICS_PROVIDER_H_
#define COMPONENTS_METRICS_CALL_STACKS_CALL_STACK_PROFILE_METRICS_PROVIDER_H_

#include <map>

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/time/time.h"
#include "build/buildflag.h"
#include "components/metrics/metrics_provider.h"
#include "components/metrics/public/mojom/call_stack_profile_collector.mojom-forward.h"
#include "third_party/metrics_proto/execution_context.pb.h"
#include "third_party/metrics_proto/sampled_profile.pb.h"

namespace metrics {

class ChromeUserMetricsExtension;

// base::Feature for reporting CPU profiles. Provided here for test use.
BASE_DECLARE_FEATURE(kSamplingProfilerReporting);

// Performs metrics logging for the stack sampling profiler.
class CallStackProfileMetricsProvider : public MetricsProvider {
 public:
  // A callback type that can be registered to intercept profiles, for testing
  // purposes.
  using InterceptorCallback =
      base::RepeatingCallback<void(SampledProfile profile)>;

#if BUILDFLAG(IS_CHROMEOS)
  // Count of profiles, brokens down by the Process and Thread type of the
  // profile.
  using ProcessThreadCount =
      std::map<::metrics::Process, std::map<::metrics::Thread, int>>;
#endif

  CallStackProfileMetricsProvider();

  CallStackProfileMetricsProvider(const CallStackProfileMetricsProvider&) =
      delete;
  CallStackProfileMetricsProvider& operator=(
      const CallStackProfileMetricsProvider&) = delete;

  ~CallStackProfileMetricsProvider() override;

  // Receives SampledProfile protobuf instances. May be called on any thread.
  static void ReceiveProfile(base::TimeTicks profile_start_time,
                             SampledProfile profile);

  // Receives serialized SampledProfile protobuf instances. May be called on any
  // thread.  Note that receiving serialized profiles is supported separately so
  // that profiles received in serialized form can be kept in that form until
  // upload. This significantly reduces memory costs.
  static void ReceiveSerializedProfile(
      base::TimeTicks profile_start_time,
      bool is_heap_profile,
      mojom::SampledProfilePtr serialized_profile);

  // Allows tests to intercept received CPU profiles, to validate that the
  // expected profiles are received. This function must be invoked prior to
  // starting any profiling since the callback is accessed asynchronously on the
  // profiling thread.
  static void SetCpuInterceptorCallbackForTesting(InterceptorCallback callback);

#if BUILDFLAG(IS_CHROMEOS)
  // Gets the counts of all successfully collected profiles, broken down by
  // process type and thread type. "Successfully collected" is defined pretty
  // minimally (we got a couple of frames). Expensive function; intended only
  // to be run during ChromeOS tast integration testing, not to be run on end-
  // user machines.
  static ProcessThreadCount GetSuccessfullyCollectedCounts();
#endif

  // MetricsProvider:
  void OnRecordingEnabled() override;
  void OnRecordingDisabled() override;
  void ProvideCurrentSessionData(
      ChromeUserMetricsExtension* uma_proto) override;

 protected:
  // Reset the static state to the defaults after startup.
  static void ResetStaticStateForTesting();
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_CALL_STACKS_CALL_STACK_PROFILE_METRICS_PROVIDER_H_
