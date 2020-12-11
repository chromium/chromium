// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_IMPL_CYCLE_SYNC_CYCLE_H_
#define COMPONENTS_SYNC_ENGINE_IMPL_CYCLE_SYNC_CYCLE_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/time/time.h"
#include "components/sync/base/model_type.h"
#include "components/sync/engine/cycle/sync_cycle_snapshot.h"
#include "components/sync/engine_impl/cycle/status_controller.h"
#include "components/sync/engine_impl/cycle/sync_cycle_context.h"
#include "components/sync/engine_impl/sync_cycle_event.h"
#include "components/sync/protocol/sync_protocol_error.h"

namespace syncer {

class ProtocolEvent;

// A class representing an attempt to synchronize the local syncable data
// store with a sync server. A SyncCycle instance is passed as a stateful
// bundle throughout the sync cycle.  The SyncCycle is not reused across
// sync cycles; each cycle starts with a new one.
class SyncCycle {
 public:
  // The Delegate services events that occur during the cycle requiring an
  // explicit (and cycle-global) action, as opposed to events that are simply
  // recorded in per-cycle state.
  class Delegate {
   public:
    // The client was throttled and should cease-and-desist syncing activity
    // until the specified time.
    virtual void OnThrottled(const base::TimeDelta& throttle_duration) = 0;

    // Some of the client's types were throttled.
    virtual void OnTypesThrottled(ModelTypeSet types,
                                  const base::TimeDelta& throttle_duration) = 0;

    // Some of the client's types were backed off.
    virtual void OnTypesBackedOff(ModelTypeSet types) = 0;

    // Silenced intervals can be out of phase with individual cycles, so the
    // delegate is the only thing that can give an authoritative answer for
    // "is syncing silenced right now". This shouldn't be necessary very often
    // as the delegate ensures no cycle is started if syncing is silenced.
    // ** Note **  This will return true if silencing commenced during this
    // cycle and the interval has not yet elapsed, but the contract here is
    // solely based on absolute time values. So, this cannot be used to infer
    // that any given cycle _instance_ is silenced.  An example of reasonable
    // use is for UI reporting.
    virtual bool IsAnyThrottleOrBackoff() = 0;

    // The client has been instructed to change its poll interval.
    virtual void OnReceivedPollIntervalUpdate(
        const base::TimeDelta& new_interval) = 0;

    // The client has been instructed to change a nudge delay.
    virtual void OnReceivedCustomNudgeDelays(
        const std::map<ModelType, base::TimeDelta>& nudge_delays) = 0;

    // Called for the syncer to respond to the error sent by the server.
    virtual void OnSyncProtocolError(
        const SyncProtocolError& sync_protocol_error) = 0;

    // Called when the server wants to change the number of hints the client
    // will buffer locally.
    virtual void OnReceivedClientInvalidationHintBufferSize(int size) = 0;

    // Called when server wants to schedule a retry GU.
    virtual void OnReceivedGuRetryDelay(const base::TimeDelta& delay) = 0;

    // Called when server requests a migration.
    virtual void OnReceivedMigrationRequest(ModelTypeSet types) = 0;

   protected:
    virtual ~Delegate() {}
  };

  SyncCycle(SyncCycleContext* context, Delegate* delegate);
  ~SyncCycle();

  // Builds a thread-safe and read-only copy of the current cycle state.
  SyncCycleSnapshot TakeSnapshot() const;
  SyncCycleSnapshot TakeSnapshotWithOrigin(
      sync_pb::SyncEnums::GetUpdatesOrigin get_updates_origin) const;

  // Builds and sends a snapshot to the cycle context's listeners.
  void SendSyncCycleEndEventNotification(
      sync_pb::SyncEnums::GetUpdatesOrigin get_updates_origin);
  void SendEventNotification(SyncCycleEvent::EventCause cause);

  void SendProtocolEvent(const ProtocolEvent& event);

  // TODO(akalin): Split this into context() and mutable_context().
  SyncCycleContext* context() const { return context_; }
  Delegate* delegate() const { return delegate_; }
  const StatusController& status_controller() const {
    return *status_controller_.get();
  }
  StatusController* mutable_status_controller() {
    return status_controller_.get();
  }

 private:
  // The context for this cycle, guaranteed to outlive |this|.
  SyncCycleContext* const context_;

  // The delegate for this cycle, must never be null.
  Delegate* const delegate_;

  // Our controller for various status and error counters.
  std::unique_ptr<StatusController> status_controller_;

  DISALLOW_COPY_AND_ASSIGN(SyncCycle);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_IMPL_CYCLE_SYNC_CYCLE_H_
