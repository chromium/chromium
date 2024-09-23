// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/heap_profiling/in_process/child_process_snapshot_controller.h"

#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/memory/ptr_util.h"
#include "base/types/pass_key.h"
#include "components/heap_profiling/in_process/heap_profiler_controller.h"
#include "components/heap_profiling/in_process/mojom/snapshot_controller.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace heap_profiling {

// static
void ChildProcessSnapshotController::CreateSelfOwnedReceiver(
    mojo::PendingReceiver<mojom::SnapshotController> receiver) {
  mojo::MakeSelfOwnedReceiver(
      base::WrapUnique(new ChildProcessSnapshotController()),
      std::move(receiver));
}

void ChildProcessSnapshotController::TakeSnapshot(
    uint32_t process_probability_pct,
    uint32_t process_index) {
  CHECK_GT(process_probability_pct, 0u);
  CHECK_LE(process_probability_pct, 100u);
  if (auto* controller = HeapProfilerController::GetInstance()) {
    controller->TakeSnapshotInChildProcess(
        base::PassKey<ChildProcessSnapshotController>(),
        process_probability_pct, process_index);
  }
}

}  // namespace heap_profiling
