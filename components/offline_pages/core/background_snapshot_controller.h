// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_SNAPSHOT_CONTROLLER_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_SNAPSHOT_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"

namespace offline_pages {

// Takes various signals and produces StartSnapshot calls following a specific
// policy.
// Main invariants:
// - Some signals prevent more snapshots to be taken.
//   OnLoad is currently such signal.
// - Once Reset() is called on the BackgroundSnapshotController, the delayed
//   tasks are reset so no StartSnapshot calls is made 'cross-session'.
class BackgroundSnapshotController {
 public:
  enum class State {
    READY,             // Listening to input, will start snapshot when needed.
    SNAPSHOT_PENDING,  // Snapshot is in progress, don't start another.
    STOPPED,           // Terminal state, no snapshots until reset.
  };
  // The expected quality of a page based on its current loading progress.
  enum class PageQuality {
    // Not loaded enough to reach a minimum level of quality. Snapshots taken at
    // this point are expected to be useless.
    POOR = 0,
    // A minimal level of quality has been attained but the page is still
    // loading so its quality is continuously increasing. One or more snapshots
    // could be taken at this stage as later ones are expected to have higher
    // quality.
    FAIR_AND_IMPROVING,
    // The page is loaded enough and has attained its peak expected quality.
    // Snapshots taken at this point are not expected to increase in quality
    // after the first one.
    HIGH,
  };

  // Client of the BackgroundSnapshotController.
  class Client {
   public:
    // Invoked at a good moment to start a snapshot.
    virtual void StartSnapshot() = 0;

   protected:
    virtual ~Client() = default;
  };

  BackgroundSnapshotController(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
      BackgroundSnapshotController::Client* client,
      bool renovations_enabled);

  BackgroundSnapshotController(const BackgroundSnapshotController&) = delete;
  BackgroundSnapshotController& operator=(const BackgroundSnapshotController&) =
      delete;

  virtual ~BackgroundSnapshotController();

  // Resets the 'session', returning controller to initial state.
  void Reset();

  // Stops current session, no more Client::StartSnapshot calls will be
  // invoked from the BackgroundSnapshotController until current session is
  // Reset(). Called by Client, for example when it encounters an error loading
  // the page.
  void Stop();

  // The Client calls this when renovations have completed.
  void RenovationsCompleted();

  // Invoked from WebContentObserver::DocumentOnLoadCompletedInPrimaryMainFrame
  void DocumentOnLoadCompletedInPrimaryMainFrame();

  int64_t GetDelayAfterDocumentOnLoadCompletedForTest();
  int64_t GetDelayAfterRenovationsCompletedForTest();

 private:
  void MaybeStartSnapshot();
  void MaybeStartSnapshotThenStop();

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  // Client owns this class.
  raw_ptr<BackgroundSnapshotController::Client> client_;
  BackgroundSnapshotController::State state_;
  int64_t delay_after_document_on_load_completed_ms_;
  int64_t delay_after_renovations_completed_ms_;

  base::WeakPtrFactory<BackgroundSnapshotController> weak_ptr_factory_{this};
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_SNAPSHOT_CONTROLLER_H_
