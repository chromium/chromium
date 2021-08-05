// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_SNAPSHOT_CONTROLLER_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_SNAPSHOT_CONTROLLER_H_

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"

namespace offline_pages {

// Takes various signals and produces StartSnapshot calls following a specific
// policy. Can request snapshots multiple times per 'session'. Session can be
// ended and another one started by calling Reset().
// Main invariants:
// - It never starts overlapping snapshots, Client reports when previous
//   snapshot is done.
// - The currently worked on (pending) snapshot is always allowed to complete,
//   the new attempts to start a snapshot are ignored until it does.
// - Some signals prevent more snapshots to be taken.
//   OnLoad is currently such signal.
// - Once Reset() is called on the SnapshotController, the delayed tasks are
//   reset so no StartSnapshot calls is made 'cross-session'.
class SnapshotController {
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

  // Client of the SnapshotController.
  class Client {
   public:
    // Invoked at a good moment to start a snapshot. May be invoked multiple
    // times, but not in overlapping manner - waits until
    // PreviousSnapshotCompleted() before the next StatrSnapshot().
    // Client should overwrite the result of previous snapshot with the new one,
    // it is assumed that later snapshots are better then previous.
    virtual void StartSnapshot() = 0;

   protected:
    virtual ~Client() {}
  };

  SnapshotController(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
      SnapshotController::Client* client);

  virtual ~SnapshotController();

  // Resets the 'session', returning controller to initial state.
  void Reset();

  // Stops current session, no more Client::StartSnapshot calls will be
  // invoked from the SnapshotController until current session is Reset().
  // Called by Client, for example when it encounters an error loading the page.
  void Stop();

  // The way for Client to report that previously started snapshot is
  // now completed (so the next one can be started).
  void PendingSnapshotCompleted();

  // Invoked from WebContentObserver::DocumentAvailableInMainFrame
  void DocumentAvailableInMainFrame();

  // Invoked from WebContentObserver::DocumentOnLoadCompletedInMainFrame
  void DocumentOnLoadCompletedInMainFrame();

  int64_t GetDelayAfterDocumentAvailableForTest();
  int64_t GetDelayAfterDocumentOnLoadCompletedForTest();

  PageQuality current_page_quality() const { return current_page_quality_; }

 private:
  void MaybeStartSnapshot(PageQuality updated_page_quality);
  void MaybeStartSnapshotThenStop();

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  // Client owns this class.
  SnapshotController::Client* client_;
  SnapshotController::State state_;
  int64_t delay_after_document_available_ms_;
  int64_t delay_after_document_on_load_completed_ms_;

  // The expected quality of a snapshot taken at the moment this value is
  // queried.
  PageQuality current_page_quality_ = PageQuality::POOR;

  base::WeakPtrFactory<SnapshotController> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SnapshotController);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_SNAPSHOT_CONTROLLER_H_
