// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/heap_profiling/public/cpp/controller.h"

#include "components/services/heap_profiling/public/cpp/settings.h"

namespace heap_profiling {

Controller::Controller(mojo::PendingRemote<mojom::ProfilingService> service,
                       mojom::StackMode stack_mode,
                       uint32_t sampling_rate)
    : heap_profiling_service_(std::move(service)),
      sampling_rate_(sampling_rate),
      stack_mode_(stack_mode) {
  DCHECK_NE(sampling_rate, 0u);
}

Controller::~Controller() = default;

void Controller::StartProfilingClient(
    mojo::PendingRemote<mojom::ProfilingClient> client,
    base::ProcessId pid,
    mojom::ProcessType process_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  mojom::ProfilingParamsPtr params = mojom::ProfilingParams::New();
  params->sampling_rate = sampling_rate_;
  params->stack_mode = stack_mode_;
  heap_profiling_service_->AddProfilingClient(pid, std::move(client),
                                              process_type, std::move(params));
}

void Controller::GetProfiledPids(GetProfiledPidsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  heap_profiling_service_->GetProfiledPids(std::move(callback));
}

base::WeakPtr<Controller> Controller::GetWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_factory_.GetWeakPtr();
}

}  // namespace heap_profiling
