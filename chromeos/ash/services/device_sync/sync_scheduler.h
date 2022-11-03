// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_DEVICE_SYNC_SYNC_SCHEDULER_H_
#define CHROMEOS_ASH_SERVICES_DEVICE_SYNC_SYNC_SCHEDULER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"

namespace ash {

namespace device_sync {

// Interface for scheduling the next CryptAuth sync (e.g. enrollment or device
// sync). The scheduler has two different strategies affecting when to perform
// the next operation:
//     PERIODIC_REFRESH: The last sync was made successfully, so we can wait a
//         relatively long time before making another sync.
//     AGGRESSIVE_RECOVERY: The last sync failed, so we try more aggressively to
//         make enrollment attempts with subsequent backoff for repeated
//         failures.
// A random jitter is applied to each sync period to smooth qps to the server.
class SyncScheduler {
 public:
  // The sync strategies mentioned in the class comments.
  enum class Strategy { PERIODIC_REFRESH, AGGRESSIVE_RECOVERY };

  // The states that the scheduler can be in.
  enum class SyncState { NOT_STARTED, WAITING_FOR_REFRESH, SYNC_IN_PROGRESS };

  // An instance is passed to the delegate when the scheduler fires for each
  // sync attempt. The delegate should call |Complete()| when the sync succeeds
  // or fails to resume the scheduler.
  class SyncRequest {
   public:
    explicit SyncRequest(base::WeakPtr<SyncScheduler> sync_scheduler);

    SyncRequest(const SyncRequest&) = delete;
    SyncRequest& operator=(const SyncRequest&) = delete;

    ~SyncRequest();

    void OnDidComplete(bool success);
    void Cancel();

   protected:
    // The parent scheduler that dispatched this request.
    base::WeakPtr<SyncScheduler> sync_scheduler_;

    // True if |OnDidComplete()| has been called.
    bool completed_;
  };

  // Handles the actual sync operation.
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Called when the scheduler fires and requests a sync attempt. The delegate
    // should call sync_request->Complete() when the request finishes.
    virtual void OnSyncRequested(std::unique_ptr<SyncRequest> sync_request) = 0;
  };

  virtual ~SyncScheduler() {}

  // Starts the scheduler with an aggressive recovery strategy if
  // |strategy| is true; otherwise, it will be started with
  // periodic refresh.
  //
  // |elapsed_time_since_last_sync| is the time since the last successful sync,
  // so we can determine the duration of the first sync period. For example, the
  // scheduler will immediately issue a sync request if the elapsed time is
  // greater than the refresh period.
  virtual void Start(const base::TimeDelta& elapsed_time_since_last_sync,
                     Strategy strategy) = 0;

  // Cancels the current scheduled sync, and forces a sync immediately. Note
  // that if this sync fails, the scheduler will adopt the AGGRESSIVE_RECOVERY
  // strategy.
  virtual void ForceSync() = 0;

  // Returns the time until the next scheduled sync operation. If no sync is
  // scheduled, a TimeDelta of zero will be returned.
  virtual base::TimeDelta GetTimeToNextSync() const = 0;

  // Returns the current sync strategy.
  virtual Strategy GetStrategy() const = 0;

  // Returns the current state of the scheduler.
  virtual SyncState GetSyncState() const = 0;

 protected:
  // Called by SyncRequest instances when the sync completes.
  virtual void OnSyncCompleted(bool success) = 0;
};

}  // namespace device_sync

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_DEVICE_SYNC_SYNC_SCHEDULER_H_
