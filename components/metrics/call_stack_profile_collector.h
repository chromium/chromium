// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_CALL_STACK_PROFILE_COLLECTOR_H_
#define COMPONENTS_METRICS_CALL_STACK_PROFILE_COLLECTOR_H_

#include "base/macros.h"
#include "components/metrics/public/mojom/call_stack_profile_collector.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace metrics {

class CallStackProfileCollector : public mojom::CallStackProfileCollector {
 public:
  CallStackProfileCollector();
  ~CallStackProfileCollector() override;

  // Create a collector to receive profiles from |expected_process|.
  static void Create(
      mojo::PendingReceiver<mojom::CallStackProfileCollector> receiver);

  // mojom::CallStackProfileCollector:
  void Collect(base::TimeTicks start_timestamp,
               mojom::SampledProfilePtr profile) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(CallStackProfileCollector);
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_CALL_STACK_PROFILE_COLLECTOR_H_
