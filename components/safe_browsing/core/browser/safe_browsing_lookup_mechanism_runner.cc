// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/safe_browsing_lookup_mechanism_runner.h"
#include "components/safe_browsing/core/browser/safe_browsing_lookup_mechanism_experimenter.h"

namespace safe_browsing {

namespace {
// Maximum time in milliseconds to wait for the SafeBrowsing service reputation
// check. After this amount of time the outstanding check will be aborted, and
// the resource will be treated as if it were safe.
const int kCheckUrlTimeoutMs = 5000;
}  // namespace

SafeBrowsingLookupMechanismRunner::SafeBrowsingLookupMechanismRunner(
    std::unique_ptr<SafeBrowsingLookupMechanism> lookup_mechanism,
    CompleteCheckCallbackWithTimeout complete_check_callback)
    : lookup_mechanism_(std::move(lookup_mechanism)),
      complete_check_callback_(std::move(complete_check_callback)) {}

SafeBrowsingLookupMechanismRunner::~SafeBrowsingLookupMechanismRunner() =
    default;

SafeBrowsingLookupMechanism::StartCheckResult
SafeBrowsingLookupMechanismRunner::Run() {
  // Start a timer to abort the check if it takes too long.
  timer_->Start(FROM_HERE, base::Milliseconds(kCheckUrlTimeoutMs), this,
                &SafeBrowsingLookupMechanismRunner::OnTimeout);
  start_lookup_time_ = base::TimeTicks::Now();

  SafeBrowsingLookupMechanism::StartCheckResult result =
      lookup_mechanism_->StartCheck(base::BindOnce(
          &SafeBrowsingLookupMechanismRunner::OnCompleteCheckResult,
          weak_factory_.GetWeakPtr()));
  if (result.is_safe_synchronously) {
    OnCheckComplete();
  }
  return result;
}

base::TimeDelta SafeBrowsingLookupMechanismRunner::GetRunDuration() {
#if DCHECK_IS_ON()
  DCHECK(is_check_complete_);
#endif
  DCHECK(!timer_->IsRunning());
  return end_lookup_time_ - start_lookup_time_;
}

void SafeBrowsingLookupMechanismRunner::OnCompleteCheckResult(
    std::unique_ptr<SafeBrowsingLookupMechanism::CompleteCheckResult> result) {
  OnCheckComplete();
  std::move(complete_check_callback_)
      .Run(/*timed_out=*/false, std::move(result));
  // NOTE: Invoking the callback results in the synchronous destruction of this
  // object, so there is nothing safe to do here but return.
}

void SafeBrowsingLookupMechanismRunner::OnTimeout() {
  OnCheckComplete();
  std::move(complete_check_callback_).Run(/*timed_out=*/true, absl::nullopt);
  // NOTE: Invoking the callback results in the synchronous destruction of this
  // object, so there is nothing safe to do here but return.
}

void SafeBrowsingLookupMechanismRunner::OnCheckComplete() {
#if DCHECK_IS_ON()
  DCHECK(!is_check_complete_);
  is_check_complete_ = true;
#endif
  timer_->Stop();
  end_lookup_time_ = base::TimeTicks::Now();
  weak_factory_.InvalidateWeakPtrs();
  lookup_mechanism_.reset();
}

void SafeBrowsingLookupMechanismRunner::SetLookupMechanismExperimenter(
    scoped_refptr<SafeBrowsingLookupMechanismExperimenter> experimenter) {
  experimenter_ = experimenter;
}

}  // namespace safe_browsing
