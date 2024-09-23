// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HEAP_PROFILING_IN_PROCESS_CHILD_PROCESS_SNAPSHOT_CONTROLLER_H_
#define COMPONENTS_HEAP_PROFILING_IN_PROCESS_CHILD_PROCESS_SNAPSHOT_CONTROLLER_H_

#include "components/heap_profiling/in_process/mojom/snapshot_controller.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace heap_profiling {

// Triggers heap snapshots in the HeapProfilerController of the current child
// process in response to notifications from the HeapProfilerController in the
// browser process.
class ChildProcessSnapshotController final : public mojom::SnapshotController {
 public:
  // Creates a ChildProcessSnapshotController and binds it to `receiver`.
  static void CreateSelfOwnedReceiver(
      mojo::PendingReceiver<mojom::SnapshotController> receiver);

  ~ChildProcessSnapshotController() final = default;

  ChildProcessSnapshotController(const ChildProcessSnapshotController&) =
      delete;
  ChildProcessSnapshotController& operator=(
      const ChildProcessSnapshotController&) = delete;

  // SnapshotController:
  void TakeSnapshot(uint32_t process_probability_pct,
                    uint32_t process_index) final;

 private:
  ChildProcessSnapshotController() = default;
};

}  // namespace heap_profiling

#endif  // COMPONENTS_HEAP_PROFILING_IN_PROCESS_CHILD_PROCESS_SNAPSHOT_CONTROLLER_H_
