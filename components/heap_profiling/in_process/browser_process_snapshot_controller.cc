// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/heap_profiling/in_process/browser_process_snapshot_controller.h"

#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/notreached.h"
#include "base/rand_util.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "components/heap_profiling/in_process/heap_profiler_controller.h"
#include "components/heap_profiling/in_process/heap_profiler_parameters.h"
#include "components/heap_profiling/in_process/mojom/snapshot_controller.mojom.h"
#include "components/sampling_profiler/process_type.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace heap_profiling {

// static
BrowserProcessSnapshotController*
BrowserProcessSnapshotController::GetInstance() {
  const auto* controller = HeapProfilerController::GetInstance();
  return controller ? controller->GetBrowserProcessSnapshotController()
                    : nullptr;
}

BrowserProcessSnapshotController::BrowserProcessSnapshotController(
    scoped_refptr<base::SequencedTaskRunner> snapshot_task_runner)
    : snapshot_task_runner_(std::move(snapshot_task_runner)) {
  // Label this as the main sequence.
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);

  // Initialize with all supported process types.
  using RemoteSet = mojo::RemoteSet<mojom::SnapshotController>;
  remotes_by_process_type_.emplace(sampling_profiler::ProfilerProcessType::kGpu,
                                   std::make_unique<RemoteSet>());
  remotes_by_process_type_.emplace(
      sampling_profiler::ProfilerProcessType::kNetworkService,
      std::make_unique<RemoteSet>());
  remotes_by_process_type_.emplace(
      sampling_profiler::ProfilerProcessType::kRenderer,
      std::make_unique<RemoteSet>());
  remotes_by_process_type_.emplace(
      sampling_profiler::ProfilerProcessType::kUtility,
      std::make_unique<RemoteSet>());

  // Now that `remotes_by_process_type_` is initialized all further access
  // should be on the snapshot sequence.
  DETACH_FROM_SEQUENCE(snapshot_sequence_checker_);
}

BrowserProcessSnapshotController::~BrowserProcessSnapshotController() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(snapshot_sequence_checker_);
}

base::WeakPtr<BrowserProcessSnapshotController>
BrowserProcessSnapshotController::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void BrowserProcessSnapshotController::SetBindRemoteForChildProcessCallback(
    BindRemoteCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  bind_remote_callback_ = std::move(callback);
}

void BrowserProcessSnapshotController::BindRemoteForChildProcess(
    int child_process_id,
    sampling_profiler::ProfilerProcessType child_process_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  mojo::PendingRemote<mojom::SnapshotController> remote;
  bind_remote_callback_.Run(child_process_id,
                            remote.InitWithNewPipeAndPassReceiver());
  snapshot_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &BrowserProcessSnapshotController::StoreRemoteOnSnapshotSequence,
          GetWeakPtr(), std::move(remote), child_process_type));
}

void BrowserProcessSnapshotController::TakeSnapshotsOnSnapshotSequence() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(snapshot_sequence_checker_);
  for (auto& [process_type, remote_set] : remotes_by_process_type_) {
    CHECK(remote_set);
    if (remote_set->empty()) {
      // Avoid division by 0. This won't change any metrics since there are no
      // processes to measure.
      continue;
    }
    int snapshot_probability_pct;
    switch (process_type) {
      case sampling_profiler::ProfilerProcessType::kGpu:
        snapshot_probability_pct = kGpuSnapshotProbability.Get();
        break;
      case sampling_profiler::ProfilerProcessType::kNetworkService:
        snapshot_probability_pct = kNetworkSnapshotProbability.Get();
        break;
      case sampling_profiler::ProfilerProcessType::kRenderer:
        snapshot_probability_pct = kRendererSnapshotProbability.Get();
        break;
      case sampling_profiler::ProfilerProcessType::kUtility:
        snapshot_probability_pct = kUtilitySnapshotProbability.Get();
        break;
      default:
        NOTREACHED();
    }
    CHECK_GE(snapshot_probability_pct, 0);
    CHECK_LE(snapshot_probability_pct, 100);
    if (snapshot_probability_pct == 0) {
      // No need to test each process since none will be chosen.
      continue;
    }

    // Choose a random set of processes to snapshot. If randomness is
    // suppressed, choose processes by their index in the set.
    double prob_idx = 0;     // Index for simulating probability.
    size_t process_idx = 0;  // Index of chosen process.
    for (const auto& remote : *remote_set) {
      const double prob = suppress_randomness_for_testing_
                              ? (prob_idx++ / remote_set->size())
                              : base::RandDouble();
      if (prob * 100.0 < snapshot_probability_pct) {
        remote->TakeSnapshot(snapshot_probability_pct, process_idx++);
      }
    }
  }
}

void BrowserProcessSnapshotController::SuppressRandomnessForTesting() {
  suppress_randomness_for_testing_ = true;
}

void BrowserProcessSnapshotController::StoreRemoteOnSnapshotSequence(
    mojo::PendingRemote<mojom::SnapshotController> remote,
    sampling_profiler::ProfilerProcessType process_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(snapshot_sequence_checker_);
  // at() will CHECK if `process_type` wasn't added in the constructor.
  remotes_by_process_type_.at(process_type)
      ->Add(std::move(remote), snapshot_task_runner_);
}

}  // namespace heap_profiling
