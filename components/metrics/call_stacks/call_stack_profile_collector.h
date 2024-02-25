// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_CALL_STACKS_CALL_STACK_PROFILE_COLLECTOR_H_
#define COMPONENTS_METRICS_CALL_STACKS_CALL_STACK_PROFILE_COLLECTOR_H_

#include "components/metrics/public/mojom/call_stack_profile_collector.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace metrics {

class CallStackProfileCollector : public mojom::CallStackProfileCollector {
 public:
  CallStackProfileCollector();

  CallStackProfileCollector(const CallStackProfileCollector&) = delete;
  CallStackProfileCollector& operator=(const CallStackProfileCollector&) =
      delete;

  ~CallStackProfileCollector() override;

  // Create a collector to receive profiles from |expected_process|.
  static void Create(
      mojo::PendingReceiver<mojom::CallStackProfileCollector> receiver);

  // mojom::CallStackProfileCollector:
  void Collect(base::TimeTicks start_timestamp,
               mojom::ProfileType profile_type,
               mojom::SampledProfilePtr profile) override;
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_CALL_STACKS_CALL_STACK_PROFILE_COLLECTOR_H_
