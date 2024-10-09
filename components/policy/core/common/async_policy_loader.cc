// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/async_policy_loader.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/policy/core/common/management/platform_management_service.h"
#include "components/policy/core/common/policy_bundle.h"

using base::Time;

namespace policy {

namespace {

// Amount of time to wait for the files on disk to settle before trying to load
// them. This alleviates the problem of reading partially written files and
// makes it possible to batch quasi-simultaneous changes.
constexpr base::TimeDelta kSettleInterval = base::Seconds(5);

// The time interval for rechecking policy. This is the fallback in case the
// implementation never detects changes.
constexpr base::TimeDelta kReloadInterval = base::Minutes(15);

}  // namespace

AsyncPolicyLoader::AsyncPolicyLoader(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    bool periodic_updates)
    : AsyncPolicyLoader(task_runner,
                        /*management_service=*/nullptr,
                        periodic_updates) {}

AsyncPolicyLoader::AsyncPolicyLoader(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    ManagementService* management_service,
    bool periodic_updates)
    : task_runner_(task_runner),
      management_service_(management_service),
      periodic_updates_(periodic_updates),
      reload_interval_(kReloadInterval) {}

AsyncPolicyLoader::~AsyncPolicyLoader() = default;

Time AsyncPolicyLoader::LastModificationTime() {
  return Time();
}

void AsyncPolicyLoader::Reload(bool force) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  base::TimeDelta delay;
  Time now = Time::Now();
  // Check if there was a recent modification to the underlying files.
  if (!force && !IsSafeToReload(now, &delay)) {
    ScheduleNextReload(delay);
    return;
  }

  // `management_service_` must be called on the main thread.
  // base::Unretained is okay here since `management_service_` is an instance of
  // PlatformManagementService which is a singleton that outlives this class.
  if (NeedManagementBitBeforeLoad()) {
    DCHECK_EQ(management_service_, PlatformManagementService::GetInstance());
    ui_thread_task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(
            &ManagementService::GetManagementAuthorityTrustworthiness,
            base::Unretained(management_service_)),
        base::BindOnce(
            &AsyncPolicyLoader::SetPlatformManagementTrustworthinessAndReload,
            weak_factory_.GetWeakPtr(), force));
    return;
  }

  PolicyBundle bundle = Load();

  // Reset so that we get the latest management trustworthiness at the next
  // reload.
  platform_management_trustworthiness_.reset();

  // Check if there was a modification while reading.
  if (!force && !IsSafeToReload(now, &delay)) {
    ScheduleNextReload(delay);
    return;
  }

  // Filter out mismatching policies.
  schema_map_->FilterBundle(bundle,
                            /*drop_invalid_component_policies=*/true);

  update_callback_.Run(std::move(bundle));
  if (periodic_updates_) {
    // Note: it is important to schedule the next reload after calling Load()
    // to make sure that anything done in Load() that may change the state of
    // the loader  (e.g. changing the `reload_interval_`) is effective before
    // scheduling the next reload.
    ScheduleNextReload(get_reload_interval());
  }
}

bool AsyncPolicyLoader::ShouldFilterSensitivePolicies() {
#if BUILDFLAG(IS_WIN)
  DCHECK(platform_management_trustworthiness_);

  return *platform_management_trustworthiness_ <
         ManagementAuthorityTrustworthiness::TRUSTED;
#else
  return false;
#endif
}

void AsyncPolicyLoader::SetPlatformManagementTrustworthinessAndReload(
    bool force,
    ManagementAuthorityTrustworthiness trustworthiness) {
  platform_management_trustworthiness_ = trustworthiness;
  Reload(force);
}

bool AsyncPolicyLoader::NeedManagementBitBeforeLoad() {
  return !platform_management_trustworthiness_.has_value() &&
         management_service_;
}

PolicyBundle AsyncPolicyLoader::InitialLoad(
    const scoped_refptr<SchemaMap>& schema_map) {
  // This is the first load, early during startup. Use this to record the
  // initial |last_modification_time_|, so that potential changes made before
  // installing the watches can be detected.
  last_modification_time_ = LastModificationTime();
  schema_map_ = schema_map;
  if (management_service_) {
    DCHECK_EQ(management_service_, PlatformManagementService::GetInstance());
    platform_management_trustworthiness_ =
        management_service_->GetManagementAuthorityTrustworthiness();
  }
  PolicyBundle bundle = Load();
  platform_management_trustworthiness_.reset();
  // Filter out mismatching policies.
  schema_map_->FilterBundle(bundle,
                            /*drop_invalid_component_policies=*/true);
  return bundle;
}

void AsyncPolicyLoader::Init(
    scoped_refptr<base::SequencedTaskRunner> ui_thread_task_runner,
    const UpdateCallback& update_callback) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(update_callback_.is_null());
  DCHECK(!update_callback.is_null());
  update_callback_ = update_callback;
  ui_thread_task_runner_ = ui_thread_task_runner;

  InitOnBackgroundThread();

  // There might have been changes to the underlying files since the initial
  // load and before the watchers have been created.
  if (LastModificationTime() != last_modification_time_)
    Reload(false);

  // Start periodic refreshes.
  if (periodic_updates_) {
    ScheduleNextReload(get_reload_interval());
  }
}

void AsyncPolicyLoader::RefreshPolicies(scoped_refptr<SchemaMap> schema_map) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  schema_map_ = schema_map;
  Reload(true);
}

void AsyncPolicyLoader::ScheduleNextReload(base::TimeDelta delay) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  weak_factory_.InvalidateWeakPtrs();
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&AsyncPolicyLoader::Reload, weak_factory_.GetWeakPtr(),
                     false /* force */),
      delay);
}

bool AsyncPolicyLoader::IsSafeToReload(const Time& now,
                                       base::TimeDelta* delay) {
  Time last_modification = LastModificationTime();
  if (last_modification.is_null())
    return true;

  // If there was a change since the last recorded modification, wait some more.
  if (last_modification != last_modification_time_) {
    last_modification_time_ = last_modification;
    last_modification_clock_ = now;
    *delay = kSettleInterval;
    return false;
  }

  // Check whether the settle interval has elapsed.
  const base::TimeDelta age = now - last_modification_clock_;
  if (age < kSettleInterval) {
    *delay = kSettleInterval - age;
    return false;
  }

  return true;
}

}  // namespace policy
