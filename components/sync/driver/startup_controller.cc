// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/startup_controller.h"

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/sync/base/command_line_switches.h"
#include "components/sync/base/features.h"

namespace syncer {

namespace {

// The amount of time we'll wait to initialize sync if no data type requests
// immediately initialization.
constexpr base::TimeDelta kDefaultDeferredInitDelay = base::Seconds(10);

base::TimeDelta GetDeferredInitDelay() {
  const base::CommandLine* cmdline = base::CommandLine::ForCurrentProcess();
  if (cmdline->HasSwitch(kSyncDeferredStartupTimeoutSeconds)) {
    int timeout = 0;
    if (base::StringToInt(
            cmdline->GetSwitchValueASCII(kSyncDeferredStartupTimeoutSeconds),
            &timeout)) {
      DCHECK_GE(timeout, 0);
      DVLOG(2) << "Sync StartupController overriding startup timeout to "
               << timeout << " seconds.";
      return base::Seconds(timeout);
    }
  }
  return kDefaultDeferredInitDelay;
}

}  // namespace

StartupController::StartupController(
    base::RepeatingCallback<ModelTypeSet()> get_preferred_data_types,
    base::RepeatingCallback<bool()> should_start,
    base::RepeatingClosure start_engine,
    policy::PolicyService* policy_service)
    : get_preferred_data_types_callback_(std::move(get_preferred_data_types)),
      should_start_callback_(std::move(should_start)),
      start_engine_callback_(std::move(start_engine)),
      bypass_deferred_startup_(false),
      policy_service_(policy_service) {
  if (policy_service_ && policy_service_->IsFirstPolicyLoadComplete(
                             policy::PolicyDomain::POLICY_DOMAIN_CHROME)) {
    // Policies are already loaded; no need to track the policy service.
    policy_service_ = nullptr;
  } else if (policy_service_) {
    policy_service_->AddObserver(policy::PolicyDomain::POLICY_DOMAIN_CHROME,
                                 this);
  }
}

StartupController::~StartupController() {
  if (policy_service_) {
    policy_service_->RemoveObserver(policy::PolicyDomain::POLICY_DOMAIN_CHROME,
                                    this);
  }
}

void StartupController::Reset() {
  bypass_deferred_startup_ = false;
  start_up_time_ = base::Time();
  start_engine_time_ = base::Time();
  // Don't let previous timers affect us post-reset.
  weak_factory_.InvalidateWeakPtrs();
}

void StartupController::StartUp(StartUpDeferredOption deferred_option) {
  const bool first_start = start_up_time_.is_null();
  if (first_start) {
    start_up_time_ = base::Time::Now();
  }

  if (deferred_option == STARTUP_DEFERRED &&
      get_preferred_data_types_callback_.Run().Has(SESSIONS)) {
    if (first_start) {
      base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&StartupController::OnFallbackStartupTimerExpired,
                         weak_factory_.GetWeakPtr()),
          GetDeferredInitDelay());
    }
    return;
  }

  if (start_engine_time_.is_null()) {
    start_engine_time_ = base::Time::Now();
    start_engine_callback_.Run();
  }
}

void StartupController::TryStart(bool force_immediate) {
  // Post a task instead of running the startup checks directly, to guarantee
  // that |start_engine_callback_| is never called synchronously from
  // TryStart().
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&StartupController::TryStartImpl,
                                weak_factory_.GetWeakPtr(), force_immediate));
}

void StartupController::TryStartImpl(bool force_immediate) {
  // Try starting up the sync engine if all policies are ready, otherwise wait
  // at most |kSyncPolicyLoadTimeout|.
  if (!ArePoliciesReady()) {
    if (waiting_for_policies_start_time_.is_null()) {
      waiting_for_policies_start_time_ = base::Time::Now();
      wait_for_policy_timer_.Start(
          FROM_HERE, kSyncPolicyLoadTimeout.Get(),
          base::BindOnce(&StartupController::OnFirstPoliciesLoadedTimeout,
                         base::Unretained(this)));
    }
    // If the Service had to start immediately, bypass the deferred startup when
    // we receive the policies.
    if (force_immediate) {
      bypass_deferred_startup_ = true;
    }
    return;
  }

  if (!should_start_callback_.Run()) {
    return;
  }

  // For performance reasons, defer the heavy lifting for sync init unless:
  //
  // - a datatype has requested an immediate start of sync, or
  // - sync needs to start up the engine immediately to provide control state
  //   and encryption information to the UI.
  StartUp((force_immediate || bypass_deferred_startup_) ? STARTUP_IMMEDIATE
                                                        : STARTUP_DEFERRED);
}

void StartupController::RecordTimeDeferred(DeferredInitTrigger trigger) {
  DCHECK(!start_up_time_.is_null());
  base::TimeDelta time_deferred = base::Time::Now() - start_up_time_;
  base::UmaHistogramCustomTimes("Sync.Startup.TimeDeferred2", time_deferred,
                                base::Seconds(0), base::Minutes(2), 60);
  base::UmaHistogramEnumeration("Sync.Startup.DeferredInitTrigger", trigger);
}

void StartupController::OnFallbackStartupTimerExpired() {
  if (!start_engine_time_.is_null()) {
    return;
  }

  DVLOG(2) << "Sync deferred init fallback timer expired, starting engine.";
  RecordTimeDeferred(DeferredInitTrigger::kFallbackTimer);
  // Once the deferred init timer has expired, don't defer startup again (until
  // Reset() or browser restart), even if this startup attempt doesn't succeed.
  bypass_deferred_startup_ = true;
  TryStart(/*force_immediate=*/false);
}

StartupController::State StartupController::GetState() const {
  if (!start_engine_time_.is_null()) {
    return State::STARTED;
  }
  if (!ArePoliciesReady() && !waiting_for_policies_start_time_.is_null()) {
    return State::STARTING_DEFERRED;
  }
  if (!start_up_time_.is_null()) {
    return State::STARTING_DEFERRED;
  }
  return State::NOT_STARTED;
}

void StartupController::OnFirstPoliciesLoaded(policy::PolicyDomain domain) {
  DCHECK_EQ(domain, policy::PolicyDomain::POLICY_DOMAIN_CHROME);

  // Cancel the timeout timer.
  wait_for_policy_timer_.AbandonAndStop();
  OnFirstPoliciesLoadedImpl(/*timeout=*/false);
}

void StartupController::OnFirstPoliciesLoadedTimeout() {
  OnFirstPoliciesLoadedImpl(/*timeout=*/true);
}

bool StartupController::ArePoliciesReady() const {
  // |policy_service_| is non-null iff we're waiting for policies to load.
  return policy_service_ == nullptr;
}

void StartupController::TriggerPolicyWaitTimeoutForTest() {
  OnFirstPoliciesLoadedTimeout();
}

void StartupController::OnFirstPoliciesLoadedImpl(bool timeout) {
  policy_service_->RemoveObserver(policy::PolicyDomain::POLICY_DOMAIN_CHROME,
                                  this);
  policy_service_ = nullptr;
  base::UmaHistogramBoolean("Sync.Startup.PolicyLoadTimeout2", timeout);
  base::UmaHistogramTimes(
      "Sync.Startup.PolicyLoadStartupDelay",
      waiting_for_policies_start_time_.is_null()
          ? base::TimeDelta()
          : base::Time::Now() - waiting_for_policies_start_time_);

  // Only try to start the engine if we explicitly tried to start but had to
  // wait for policies to be loaded.
  if (!waiting_for_policies_start_time_.is_null()) {
    TryStart(/*force_immediate=*/false);
  }
}

void StartupController::OnDataTypeRequestsSyncStartup(ModelType type) {
  if (!start_engine_time_.is_null()) {
    return;
  }

  DVLOG(2) << "Data type requesting sync startup: "
           << ModelTypeToDebugString(type);
  if (!start_up_time_.is_null()) {
    RecordTimeDeferred(DeferredInitTrigger::kDataTypeRequest);
  }
  bypass_deferred_startup_ = true;
  TryStart(/*force_immediate=*/false);
}

}  // namespace syncer
