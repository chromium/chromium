// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/url_checker/safe_browsing_lookup_mechanism_runner.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"

namespace safe_browsing {

namespace {
// Maximum time in milliseconds to wait for the SafeBrowsing service reputation
// check. After this amount of time the outstanding check will be aborted, and
// the resource will be treated as if it were safe.
const int kCheckUrlTimeoutMs = 5000;

void RecordRunnerTimeTaken(const std::string& performed_check_suffix,
                           base::TimeDelta time_taken) {
  base::UmaHistogramTimes(base::StrCat({"SafeBrowsing.CheckUrl.TimeTaken.",
                                        performed_check_suffix}),
                          time_taken);
}

void RecordRunnerThreatType(const std::string& performed_check_suffix,
                            const std::string& profile_mode_suffix,
                            SBThreatType threat_type) {
  base::UmaHistogramEnumeration(
      base::StrCat(
          {"SafeBrowsing.CheckUrl.ThreatType.", performed_check_suffix}),
      threat_type);
  base::UmaHistogramEnumeration(
      base::StrCat({"SafeBrowsing.CheckUrl.ThreatType.", performed_check_suffix,
                    ".", profile_mode_suffix}),
      threat_type);
}
}  // namespace

SafeBrowsingLookupMechanismRunner::SafeBrowsingLookupMechanismRunner(
    std::unique_ptr<SafeBrowsingLookupMechanism> lookup_mechanism,
    const std::string& performed_check_suffix,
    const std::string& profile_mode_suffix,
    CompleteCheckCallbackWithTimeout complete_check_callback)
    : lookup_mechanism_(std::move(lookup_mechanism)),
      performed_check_suffix_(performed_check_suffix),
      profile_mode_suffix_(profile_mode_suffix),
      complete_check_callback_(std::move(complete_check_callback)) {
  CHECK(!profile_mode_suffix_.empty());
}

SafeBrowsingLookupMechanismRunner::~SafeBrowsingLookupMechanismRunner() =
    default;

SafeBrowsingLookupMechanism::StartCheckResult
SafeBrowsingLookupMechanismRunner::Run() {
  start_lookup_time_ = base::TimeTicks::Now();
  // Start a timer to abort the check if it takes too long.
  timer_->Start(FROM_HERE, base::Milliseconds(kCheckUrlTimeoutMs), this,
                &SafeBrowsingLookupMechanismRunner::OnTimeout);

  SafeBrowsingLookupMechanism::StartCheckResult result =
      lookup_mechanism_->StartCheck(base::BindOnce(
          &SafeBrowsingLookupMechanismRunner::OnCompleteCheckResult,
          weak_factory_.GetWeakPtr()));
  if (result.is_safe_synchronously) {
    OnCheckComplete(SBThreatType::SB_THREAT_TYPE_SAFE);
  }
  return result;
}

void SafeBrowsingLookupMechanismRunner::OnCompleteCheckResult(
    std::unique_ptr<SafeBrowsingLookupMechanism::CompleteCheckResult> result) {
  OnCheckComplete(result->threat_type);
  std::move(complete_check_callback_)
      .Run(/*timed_out=*/false, std::move(result));
  // NOTE: Invoking the callback results in the synchronous destruction of this
  // object, so there is nothing safe to do here but return.
}

void SafeBrowsingLookupMechanismRunner::OnTimeout() {
  OnCheckComplete(/*threat_type=*/std::nullopt);
  std::move(complete_check_callback_).Run(/*timed_out=*/true, std::nullopt);
  // NOTE: Invoking the callback results in the synchronous destruction of this
  // object, so there is nothing safe to do here but return.
}

void SafeBrowsingLookupMechanismRunner::OnCheckComplete(
    std::optional<SBThreatType> threat_type) {
#if DCHECK_IS_ON()
  DCHECK(!is_check_complete_);
  is_check_complete_ = true;
#endif
  timer_->Stop();
  RecordRunnerTimeTaken(performed_check_suffix_,
                        base::TimeTicks::Now() - start_lookup_time_);
  if (threat_type.has_value()) {
    RecordRunnerThreatType(performed_check_suffix_, profile_mode_suffix_,
                           threat_type.value());
  }
  weak_factory_.InvalidateWeakPtrs();
  lookup_mechanism_.reset();
}

}  // namespace safe_browsing
