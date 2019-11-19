// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_MODEL_SAFE_WORKER_H_
#define COMPONENTS_SYNC_ENGINE_MODEL_SAFE_WORKER_H_

#include <map>
#include <memory>
#include <string>

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/syncer_error.h"

namespace base {
class DictionaryValue;
}  // namespace base

namespace syncer {

using WorkCallback = base::OnceCallback<SyncerError(void)>;

enum ModelSafeGroup {
  GROUP_PASSIVE = 0,   // Models that are just "passively" being synced; e.g.
                       // changes to these models don't need to be pushed to a
                       // native model.
  GROUP_UI,            // Models that live on UI thread and are being synced.
  GROUP_PASSWORD,      // Models that live on the password thread and are
                       // being synced.  On windows and linux, this runs on the
                       // DB thread.
  GROUP_NON_BLOCKING,  // Models that correspond to non-blocking types. These
                       // models always stay in GROUP_NON_BLOCKING; changes are
                       // forwarded to these models without ModelSafeWorker/
                       // SyncBackendRegistrar involvement.
};

std::string ModelSafeGroupToString(ModelSafeGroup group);

// The Syncer uses a ModelSafeWorker for all tasks that could potentially
// modify syncable entries (e.g under a WriteTransaction). The ModelSafeWorker
// only knows how to do one thing, and that is take some work (in a fully
// pre-bound callback) and have it performed (as in Run()) from a thread which
// is guaranteed to be "model-safe", where "safe" refers to not allowing us to
// cause an embedding application model to fall out of sync with the
// syncable::Directory due to a race. Each ModelSafeWorker is affiliated with
// a thread and does actual work on that thread.
class ModelSafeWorker : public base::RefCountedThreadSafe<ModelSafeWorker> {
 public:
  // If not stopped, calls ScheduleWork() to schedule |work| and waits until it
  // is done or abandoned. Otherwise, returns CANNOT_DO_WORK.
  SyncerError DoWorkAndWaitUntilDone(WorkCallback work);

  // Soft stop worker by setting stopped_ flag. Called when sync is disabled
  // or browser is shutting down. Called on UI loop.
  virtual void RequestStop();

  virtual ModelSafeGroup GetModelSafeGroup() = 0;

  // Returns true if called on the sequence this worker works on.
  virtual bool IsOnModelSequence() = 0;

 protected:
  ModelSafeWorker();
  virtual ~ModelSafeWorker();

 private:
  friend class base::RefCountedThreadSafe<ModelSafeWorker>;

  // Schedules |work| on the appropriate sequence.
  virtual void ScheduleWork(base::OnceClosure work) = 0;

  void DoWork(WorkCallback work,
              base::ScopedClosureRunner scoped_closure_runner,
              SyncerError* error,
              bool* did_run);

  // Synchronizes access to all members.
  base::Lock lock_;

  // Signaled when DoWorkAndWaitUntilDone() can return, either because the work
  // is done, the work has been abandoned or RequestStop() was called while no
  // work was running. Reset at the beginning of DoWorkAndWaitUntilDone().
  base::WaitableEvent work_done_or_abandoned_;

  // Whether a WorkCallback is currently running.
  bool is_work_running_ = false;

  // Whether the worker was stopped. No WorkCallback can start running when this
  // is true.
  bool stopped_ = false;

  DISALLOW_COPY_AND_ASSIGN(ModelSafeWorker);
};

// A map that details which ModelSafeGroup each ModelType
// belongs to.  Routing info can change in response to the user enabling /
// disabling sync for certain types, as well as model association completions.
using ModelSafeRoutingInfo = std::map<ModelType, ModelSafeGroup>;

// Caller takes ownership of return value.
std::unique_ptr<base::DictionaryValue> ModelSafeRoutingInfoToValue(
    const ModelSafeRoutingInfo& routing_info);

std::string ModelSafeRoutingInfoToString(
    const ModelSafeRoutingInfo& routing_info);

ModelTypeSet GetRoutingInfoTypes(const ModelSafeRoutingInfo& routing_info);

ModelSafeGroup GetGroupForModelType(const ModelType type,
                                    const ModelSafeRoutingInfo& routes);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_MODEL_SAFE_WORKER_H_
