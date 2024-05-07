// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/heap_profiling/in_process/browser_process_snapshot_controller.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "components/heap_profiling/in_process/heap_profiler_controller.h"
#include "components/heap_profiling/in_process/mojom/snapshot_controller.mojom.h"
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
    int child_process_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  mojo::PendingRemote<mojom::SnapshotController> remote;
  bind_remote_callback_.Run(child_process_id,
                            remote.InitWithNewPipeAndPassReceiver());
  snapshot_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &BrowserProcessSnapshotController::StoreRemoteOnSnapshotSequence,
          GetWeakPtr(), std::move(remote)));
}

void BrowserProcessSnapshotController::TakeSnapshotsOnSnapshotSequence() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(snapshot_sequence_checker_);
  for (const auto& remote : remotes_) {
    remote->TakeSnapshot();
  }
}

void BrowserProcessSnapshotController::StoreRemoteOnSnapshotSequence(
    mojo::PendingRemote<mojom::SnapshotController> remote) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(snapshot_sequence_checker_);
  remotes_.Add(std::move(remote), snapshot_task_runner_);
}

}  // namespace heap_profiling
