// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HEAP_PROFILING_IN_PROCESS_BROWSER_PROCESS_SNAPSHOT_CONTROLLER_H_
#define COMPONENTS_HEAP_PROFILING_IN_PROCESS_BROWSER_PROCESS_SNAPSHOT_CONTROLLER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace base {
class SequencedTaskRunner;
}

namespace heap_profiling {

namespace mojom {
class SnapshotController;
}

// Sends notifications to ChildProcessSnapshotController endpoints in child
// processes to trigger snapshots on demand from the HeapProfilerController in
// the current browser process.
//
// Unless otherwise noted all methods must be called on the main thread.
class BrowserProcessSnapshotController {
 public:
  // Returns the BrowserProcessSnapshotController for this process or nullptr if
  // none exists.
  static BrowserProcessSnapshotController* GetInstance();

  // Outside of tests, BrowserProcessSnapshotController is created and owned by
  // the HeapProfilerController. `snapshot_task_runner` will be used to take
  // heap snapshots off of the main thread.
  explicit BrowserProcessSnapshotController(
      scoped_refptr<base::SequencedTaskRunner> snapshot_task_runner);

  // The destructor must be called on `snapshot_task_runner` so that
  // WeakPtr's are invalidated on the correct sequence.
  ~BrowserProcessSnapshotController();

  BrowserProcessSnapshotController(const BrowserProcessSnapshotController&) =
      delete;
  BrowserProcessSnapshotController& operator=(
      const BrowserProcessSnapshotController&) = delete;

  // Returns a pointer that must be dereferenced on `snapshot_task_runner`.
  // This can be called from any thread.
  base::WeakPtr<BrowserProcessSnapshotController> GetWeakPtr();

  // Sets a callback that registers a remote endpoint to send commands to a
  // child process. `callback` will be invoked from BindRemoteForChildProcess()
  // with a `child_process_id` and a `pending_receiver`. The callback should
  // bind the `pending_receiver` to a mojo::Receiver in the child process with
  // ID `child_process_id`. The BrowserProcessSnapshotController will hold the
  // mojo::Remote end of the connection.
  using BindRemoteCallback = base::RepeatingCallback<void(
      int /*child_process_id*/,
      mojo::PendingReceiver<mojom::SnapshotController> /*pending_receiver*/)>;
  void SetBindRemoteForChildProcessCallback(BindRemoteCallback callback);

  // Binds a remote endpoint to communicate with `child_process_id`.
  void BindRemoteForChildProcess(int child_process_id);

  // Triggers a snapshot in all known child processes. This must be called on
  // `snapshot_task_runner`.
  void TakeSnapshotsOnSnapshotSequence();

 private:
  // Saves `remote` in `remotes_`. Must be called on `snapshot_task_runner`.
  void StoreRemoteOnSnapshotSequence(
      mojo::PendingRemote<mojom::SnapshotController> remote);

  SEQUENCE_CHECKER(main_sequence_checker_);
  SEQUENCE_CHECKER(snapshot_sequence_checker_);

  // A task runner to trigger snapshots off of the main thread.
  scoped_refptr<base::SequencedTaskRunner> snapshot_task_runner_;

  // Callback used to bind a mojo remote to a child process.
  BindRemoteCallback bind_remote_callback_
      GUARDED_BY_CONTEXT(main_sequence_checker_);

  // A set of remotes for controlling child processes. Must be accessed on
  // `snapshot_task_runner_`.
  mojo::RemoteSet<mojom::SnapshotController> remotes_
      GUARDED_BY_CONTEXT(snapshot_sequence_checker_);

  base::WeakPtrFactory<BrowserProcessSnapshotController> weak_factory_{this};
};

}  // namespace heap_profiling

#endif  // COMPONENTS_HEAP_PROFILING_IN_PROCESS_BROWSER_PROCESS_SNAPSHOT_CONTROLLER_H_
