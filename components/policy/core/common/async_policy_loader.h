// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_ASYNC_POLICY_LOADER_H_
#define COMPONENTS_POLICY_CORE_COMMON_ASYNC_POLICY_LOADER_H_

#include <optional>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/policy/core/common/schema_map.h"
#include "components/policy/policy_export.h"

namespace base {
class SequencedTaskRunner;
}

namespace policy {

class ManagementService;
class PolicyBundle;

// Base implementation for platform-specific policy loaders. Together with the
// AsyncPolicyProvider, this base implementation takes care of the initial load,
// refreshing policies and object lifetime. Also if the object has
// |period_updates_| set to true it takes care of periodic reloads and watching
// file changes.
//
// All methods are invoked on the background |task_runner_|, including the
// destructor. The only exceptions are the constructor (which may be called on
// any thread), InitialLoad() which is called on the thread that owns the
// provider and the calls of Load() and LastModificationTime() during the
// initial load.
// Also, during tests the destructor may be called on the main thread.
class POLICY_EXPORT AsyncPolicyLoader {
 public:
  explicit AsyncPolicyLoader(
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      bool periodic_updates);
  explicit AsyncPolicyLoader(
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      ManagementService* management_service,
      bool periodic_updates);
  AsyncPolicyLoader(const AsyncPolicyLoader&) = delete;
  AsyncPolicyLoader& operator=(const AsyncPolicyLoader&) = delete;
  virtual ~AsyncPolicyLoader();

  // Gets a SequencedTaskRunner backed by the background thread.
  const scoped_refptr<base::SequencedTaskRunner>& task_runner() const {
    return task_runner_;
  }

  // Returns the currently configured policies. Load() is always invoked on
  // the background thread, except for the initial Load() at startup which is
  // invoked from the thread that owns the provider.
  virtual PolicyBundle Load() = 0;

  // Allows implementations to finalize their initialization on the background
  // thread (e.g. setup file watchers).
  virtual void InitOnBackgroundThread() = 0;

  // Implementations should return the time of the last modification detected,
  // or base::Time() if it doesn't apply, which is the default.
  virtual base::Time LastModificationTime();

  // Used by the AsyncPolicyProvider to do the initial Load(). The first load
  // is also used to initialize |last_modification_time_| and
  // |schema_map_|.
  PolicyBundle InitialLoad(const scoped_refptr<SchemaMap>& schemas);

  // Implementations should invoke Reload() when a change is detected. This
  // must be invoked from the background thread and will trigger a Load(),
  // and pass the returned bundle to the provider.
  // The load is immediate when |force| is true. Otherwise, the loader
  // reschedules the reload until the LastModificationTime() is a couple of
  // seconds in the past. This mitigates the problem of reading files that are
  // currently being written to, and whose contents are incomplete.
  // When |periodic_updates_| is true a reload is posted periodically, if it
  // hasn't been triggered recently. This makes sure the policies are reloaded
  // if the update events aren't triggered.
  virtual void Reload(bool force);

  // Returns `true` and only if the platform is not managed by a trusted source.
  bool ShouldFilterSensitivePolicies();
  void SetPlatformManagementTrustworthinessAndReload(
      bool force,
      ManagementAuthorityTrustworthiness trustworthiness);

  const scoped_refptr<SchemaMap>& schema_map() const { return schema_map_; }

  base::TimeDelta get_reload_interval() const { return reload_interval_; }

  void set_reload_interval(base::TimeDelta reload_interval) {
    reload_interval_ = reload_interval;
  }

 protected:
  // Return true if we need to asynchronously get
  //`platform_management_trustworthiness_` bit before reloading policies.
  bool NeedManagementBitBeforeLoad();

 private:
  // Allow AsyncPolicyProvider to call Init().
  friend class AsyncPolicyProvider;

  using UpdateCallback = base::RepeatingCallback<void(PolicyBundle)>;

  // Used by the AsyncPolicyProvider to install the |update_callback_|.
  // Invoked on the background thread.
  void Init(scoped_refptr<base::SequencedTaskRunner> ui_thread_task_runner,
            const UpdateCallback& update_callback);

  // Used by the AsyncPolicyProvider to reload with an updated SchemaMap.
  void RefreshPolicies(scoped_refptr<SchemaMap> schema_map);

  // Cancels any pending periodic reload and posts one |delay| time units from
  // now.
  void ScheduleNextReload(base::TimeDelta delay);

  // Checks if the underlying files haven't changed recently, by checking the
  // LastModificationTime(). |delay| is updated with a suggested time to wait
  // before retrying when this returns false.
  bool IsSafeToReload(const base::Time& now, base::TimeDelta* delay);

  // Task runner for running background jobs.
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Task runner for running foregroud jobs.
  scoped_refptr<base::SequencedTaskRunner> ui_thread_task_runner_;

  std::optional<ManagementAuthorityTrustworthiness>
      platform_management_trustworthiness_;

  raw_ptr<ManagementService> management_service_;

  // Whether the loader will schedule periodic updates for policy data.
  const bool periodic_updates_;

  // Callback for updates, passed in Init().
  UpdateCallback update_callback_;

  // Records last known modification timestamp.
  base::Time last_modification_time_;

  // The wall clock time at which the last modification timestamp was
  // recorded.  It's better to not assume the file notification time and the
  // wall clock times come from the same source, just in case there is some
  // non-local filesystem involved.
  base::Time last_modification_clock_;

  // The current policy schemas that this provider should load.
  scoped_refptr<SchemaMap> schema_map_;

  // The interval of time between periodic updates. Only relevant when
  // `periodic_updates_` is true to enable periodic updates.
  base::TimeDelta reload_interval_;

  // Used to get WeakPtrs for the periodic reload task.
  base::WeakPtrFactory<AsyncPolicyLoader> weak_factory_{this};
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_ASYNC_POLICY_LOADER_H_
