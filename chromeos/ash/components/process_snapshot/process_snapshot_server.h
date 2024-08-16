// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PROCESS_SNAPSHOT_PROCESS_SNAPSHOT_SERVER_H_
#define CHROMEOS_ASH_COMPONENTS_PROCESS_SNAPSHOT_PROCESS_SNAPSHOT_SERVER_H_

#include "base/component_export.h"
#include "base/lazy_instance.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/process/process_iterator.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace ash {

// Provides an interface to regularly getting a full snapshot of the running
// processes on the system.
// The process snapshot will be refreshed at the highest refresh rate (i.e.
// lowest desired refresh time) requested by all clients.
// This system is useful for clients that regularly need a process snapshot but
// don't care if it's stale for a short amount of time. This is to avoid
// multiple clients building a separate process snapshot individually which can
// lead to performance issues.
// (See b/154362057 and https://crbug.com/).
// TODO(afakhry): Currently, this is only used by chome-os-specific task manager
// providers (for VM/Crostini and ARC++ tasks). Consider moving this server
// outside chrome/browser/chromeos/ so that other non-chrome-os clients can use
// it.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_PROCESS_SNAPSHOT)
    ProcessSnapshotServer {
 public:
  class Observer : public base::CheckedObserver {
   public:
    explicit Observer(base::TimeDelta desired_refresh_time);

    base::TimeDelta desired_refresh_time() const {
      return desired_refresh_time_;
    }

    // Called when the process |snapshot| is refreshed. This may be called with
    // intervals shorter than |desired_refresh_time_| as there might be other
    // observers demanding higher refresh rates.
    virtual void OnProcessSnapshotRefreshed(
        const base::ProcessIterator::ProcessEntries& snapshot) = 0;

   protected:
    ~Observer() override = default;

   private:
    // The desired time between two successive refreshes of the process
    // snapshot. The server will use the value from the observer which is
    // minimum among all other observers.
    const base::TimeDelta desired_refresh_time_;
  };

  ProcessSnapshotServer(const ProcessSnapshotServer&) = delete;
  ProcessSnapshotServer& operator=(const ProcessSnapshotServer&) = delete;
  ~ProcessSnapshotServer();

  static ProcessSnapshotServer* Get();

  const base::ProcessIterator::ProcessEntries& snapshot() const {
    return snapshot_;
  }

  // If the added |observer| has a lower |desired_refresh_time_| than all
  // current observers, the refresh rate will be adjusted to match that,
  // otherwise, the server will continue at the same current refresh rate.
  //
  // The first |observer| to be added will trigger an immediate refresh of the
  // process snapshot regardless of the |desired_refresh_time_|.
  void AddObserver(Observer* observer);

  // If the removed |observer| used to have the lowset |desired_refresh_time_|,
  // the refresh rate will be adjusted according to the remaining observers.
  // If |observer| is the last one to be removed, then refreshing will stop.
  void RemoveObserver(Observer* observer);

  const base::RepeatingTimer& GetTimerForTesting() const { return timer_; }

 private:
  friend struct base::LazyInstanceTraitsBase<ProcessSnapshotServer>;

  ProcessSnapshotServer();

  // Triggers a refresh of the process snapshot on |task_runner_|.
  void RefreshSnapshot();

  // Called on the UI thread when the process snapshot is refreshed.
  void OnSnapshotRefreshed(base::ProcessIterator::ProcessEntries snapshot);

  // Updates the current refresh rate based on |new_refresh_time| which could
  // also lead to stopping the timer if |base::TimeDelta::Max()| is provided.
  void RefreshTimer(base::TimeDelta new_refresh_time);

  // The current cached process snapshot. It's only valid if the |timer_| is
  // running and it's being refreshed regularly. Depending on the current
  // refresh rate, it can be too stale.
  base::ProcessIterator::ProcessEntries snapshot_;

  base::RepeatingTimer timer_;

  base::ObserverList<Observer> observers_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  base::WeakPtrFactory<ProcessSnapshotServer> weak_ptr_factory_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_PROCESS_SNAPSHOT_PROCESS_SNAPSHOT_SERVER_H_
