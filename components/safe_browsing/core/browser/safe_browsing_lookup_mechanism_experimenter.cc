// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/safe_browsing_lookup_mechanism_experimenter.h"

#include "base/containers/contains.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/browser/hash_database_mechanism.h"
#include "components/safe_browsing/core/browser/hash_realtime_mechanism.h"
#include "components/safe_browsing/core/browser/safe_browsing_lookup_mechanism_runner.h"
#include "components/safe_browsing/core/browser/url_realtime_mechanism.h"

namespace safe_browsing {
SafeBrowsingLookupMechanismExperimenter::
    SafeBrowsingLookupMechanismExperimenter(bool is_prefetch) {
  is_prefetch_ = is_prefetch;
}
SafeBrowsingLookupMechanismExperimenter::
    ~SafeBrowsingLookupMechanismExperimenter() = default;

SafeBrowsingLookupMechanism::StartCheckResult
SafeBrowsingLookupMechanismExperimenter::RunChecks(
    size_t safe_browsing_url_checker_index,
    SafeBrowsingLookupMechanismRunner::CompleteCheckCallbackWithTimeout
        url_real_time_result_callback,
    const GURL& url,
    const SBThreatTypeSet& threat_types,
    network::mojom::RequestDestination request_destination,
    scoped_refptr<SafeBrowsingDatabaseManager> database_manager,
    bool can_check_db,
    bool can_check_high_confidence_allowlist,
    std::string url_lookup_service_metric_suffix,
    const GURL& last_committed_url,
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    base::WeakPtr<RealTimeUrlLookupServiceBase> url_lookup_service_on_ui,
    UrlRealTimeMechanism::WebUIDelegate* webui_delegate,
    base::WeakPtr<HashRealTimeService> hash_real_time_service_on_ui) {
  auto url_real_time_mechanism = std::make_unique<UrlRealTimeMechanism>(
      url, threat_types, request_destination, database_manager, can_check_db,
      can_check_high_confidence_allowlist, url_lookup_service_metric_suffix,
      last_committed_url, ui_task_runner, url_lookup_service_on_ui,
      webui_delegate, MechanismExperimentHashDatabaseCache::kUrlRealTimeOnly);
  auto hash_database_mechanism = std::make_unique<HashDatabaseMechanism>(
      url, threat_types, database_manager, can_check_db,
      MechanismExperimentHashDatabaseCache::kHashDatabaseOnly);
  auto hash_real_time_mechanism = std::make_unique<HashRealTimeMechanism>(
      url, threat_types, database_manager, can_check_db, ui_task_runner,
      hash_real_time_service_on_ui,
      MechanismExperimentHashDatabaseCache::kHashRealTimeOnly);

  return RunChecksInternal(
      safe_browsing_url_checker_index, std::move(url_real_time_mechanism),
      std::move(hash_database_mechanism), std::move(hash_real_time_mechanism),
      std::move(url_real_time_result_callback));
}

SafeBrowsingLookupMechanism::StartCheckResult
SafeBrowsingLookupMechanismExperimenter::RunChecksInternal(
    size_t safe_browsing_url_checker_index,
    std::unique_ptr<SafeBrowsingLookupMechanism> url_real_time_mechanism,
    std::unique_ptr<SafeBrowsingLookupMechanism> hash_database_mechanism,
    std::unique_ptr<SafeBrowsingLookupMechanism> hash_real_time_mechanism,
    SafeBrowsingLookupMechanismRunner::CompleteCheckCallbackWithTimeout
        url_real_time_result_callback) {
  DCHECK(!base::Contains(safe_browsing_url_checker_index_to_experimenter_index_,
                         safe_browsing_url_checker_index));
  safe_browsing_url_checker_index_to_experimenter_index_
      [safe_browsing_url_checker_index] = checks_to_run_.size();

  // Create the mechanism runners and give them a reference to this object.
  // UrlRealTimeMechanism
  auto url_real_time_runner = std::make_unique<
      SafeBrowsingLookupMechanismRunner>(
      std::move(url_real_time_mechanism),
      base::BindOnce(
          &SafeBrowsingLookupMechanismExperimenter::OnUrlRealTimeCheckComplete,
          weak_factory_.GetWeakPtr()));
  url_real_time_runner->SetLookupMechanismExperimenter(
      base::WrapRefCounted(this));
  // HashDatabaseMechanism
  auto hash_database_runner = std::make_unique<
      SafeBrowsingLookupMechanismRunner>(
      std::move(hash_database_mechanism),
      base::BindOnce(
          &SafeBrowsingLookupMechanismExperimenter::OnHashDatabaseCheckComplete,
          weak_factory_.GetWeakPtr()));
  hash_database_runner->SetLookupMechanismExperimenter(
      base::WrapRefCounted(this));
  // HashRealTimeMechanism
  auto hash_real_time_runner = std::make_unique<
      SafeBrowsingLookupMechanismRunner>(
      std::move(hash_real_time_mechanism),
      base::BindOnce(
          &SafeBrowsingLookupMechanismExperimenter::OnHashRealTimeCheckComplete,
          weak_factory_.GetWeakPtr()));
  hash_real_time_runner->SetLookupMechanismExperimenter(
      base::WrapRefCounted(this));
  // Start tracking this check
  checks_to_run_.push_back(std::make_unique<CheckToRun>(
      std::move(url_real_time_runner), std::move(hash_database_runner),
      std::move(hash_real_time_runner),
      std::move(url_real_time_result_callback)));
  // Always run the URL real-time lookup, since we need to return its results.
  auto url_real_time_result =
      checks_to_run_.back()->url_real_time_details.runner->Run();
  if (!first_check_start_time_.has_value()) {
    first_check_start_time_ = base::TimeTicks::Now();
  }
  DCHECK(!url_real_time_result.is_safe_synchronously);
  // Kick off running the hash-prefix database lookup if one is not already
  // running.
  if (hash_database_check_index_ == checks_to_run_.size() - 1) {
    // Normally it can be dangerous to run code after a call to
    // |RunNextHashDatabaseCheck| due to the possible synchronous destruction of
    // this object, but in this case it is impossible because the experiment is
    // still waiting on the results of the latest hash real-time check.
    RunNextHashDatabaseCheck();
  }
  // Kick off running the hash-prefix real-time lookup if one is not already
  // running.
  if (hash_real_time_check_index_ == checks_to_run_.size() - 1) {
    RunNextHashRealTimeCheck();
  }
  return url_real_time_result;
}

void SafeBrowsingLookupMechanismExperimenter::RunNextHashDatabaseCheck() {
  auto result = checks_to_run_[hash_database_check_index_]
                    ->hash_database_details.runner->Run();
  if (result.is_safe_synchronously) {
    OnHashDatabaseCheckCompleteInternal(/*timed_out=*/false,
                                        SB_THREAT_TYPE_SAFE);
    return;
    // NOTE: Calling |OnHashDatabaseCheckCompleteInternal| may result in the
    // synchronous destruction of this object, so there is nothing safe to do
    // here but return.
  }
}
void SafeBrowsingLookupMechanismExperimenter::RunNextHashRealTimeCheck() {
  auto result = checks_to_run_[hash_real_time_check_index_]
                    ->hash_real_time_details.runner->Run();
  DCHECK(!result.is_safe_synchronously);
}
void SafeBrowsingLookupMechanismExperimenter::OnWillProcessResponseReached(
    base::TimeTicks reached_time) {
  will_process_response_reached_time_ = reached_time;
  MaybeCompleteExperiment();
  // Normally it can be dangerous to run code after a call to
  // |MaybeCompleteExperiment| due to the possible synchronous destruction of
  // this object, but in this case it would be safe, because the caller
  // (BrowserUrlLoaderThrottle::CheckerOnIO) must still have a reference to
  // *this* if it was able to call |OnWillProcessResponseReached|, and the
  // experimenter does not influence the lifetime of CheckerOnIO.
}
void SafeBrowsingLookupMechanismExperimenter::OnUrlRealTimeCheckComplete(
    bool timed_out,
    absl::optional<
        std::unique_ptr<SafeBrowsingLookupMechanism::CompleteCheckResult>>
        result) {
  auto& check = checks_to_run_.back();
  auto threat_type = result.has_value() ? result.value()->threat_type
                                        : absl::optional<SBThreatType>();
  auto& run_details = check->url_real_time_details;
  std::move(check->url_real_time_details.url_result_callback)
      .Run(timed_out, std::move(result));
  StoreCheckResults(timed_out, threat_type, run_details);
  // NOTE: Calling |StoreCheckResults| may result in the synchronous
  // destruction of this object, so there is nothing safe to do here but return.
}
void SafeBrowsingLookupMechanismExperimenter::OnHashDatabaseCheckComplete(
    bool timed_out,
    absl::optional<
        std::unique_ptr<SafeBrowsingLookupMechanism::CompleteCheckResult>>
        result) {
  OnHashDatabaseCheckCompleteInternal(
      timed_out, result.has_value() ? result.value()->threat_type
                                    : absl::optional<SBThreatType>());
  // NOTE: Calling |OnHashDatabaseCheckCompleteInternal| may result in the
  // synchronous destruction of this object, so there is nothing safe to do here
  // but return.
}
void SafeBrowsingLookupMechanismExperimenter::
    OnHashDatabaseCheckCompleteInternal(
        bool timed_out,
        absl::optional<SBThreatType> threat_type) {
  auto weak_self = weak_factory_.GetWeakPtr();
  StoreCheckResults(
      timed_out, threat_type,
      checks_to_run_[hash_database_check_index_]->hash_database_details);
  // NOTE: Calling |StoreCheckResults| may result in the synchronous
  // destruction of this object, so we confirm *this* still exists before
  // proceeding.
  if (!!weak_self) {
    hash_database_check_index_++;
    if (hash_database_check_index_ < checks_to_run_.size()) {
      RunNextHashDatabaseCheck();
      return;
      // NOTE: Calling |RunNextHashDatabaseCheck| may result in the synchronous
      // destruction of this object, so there is nothing safe to do here but
      // return.
    }
  }
}
void SafeBrowsingLookupMechanismExperimenter::OnHashRealTimeCheckComplete(
    bool timed_out,
    absl::optional<
        std::unique_ptr<SafeBrowsingLookupMechanism::CompleteCheckResult>>
        result) {
  auto weak_self = weak_factory_.GetWeakPtr();
  StoreCheckResults(
      timed_out,
      result.has_value() ? result.value()->threat_type
                         : absl::optional<SBThreatType>(),
      checks_to_run_[hash_real_time_check_index_]->hash_real_time_details);
  // NOTE: Calling |StoreCheckResults| may result in the synchronous
  // destruction of this object, so we confirm *this* still exists before
  // proceeding.
  if (!!weak_self) {
    hash_real_time_check_index_++;
    if (hash_real_time_check_index_ < checks_to_run_.size()) {
      RunNextHashRealTimeCheck();
    }
  }
}
void SafeBrowsingLookupMechanismExperimenter::StoreCheckResults(
    bool timed_out,
    absl::optional<SBThreatType> threat_type,
    CheckToRun::RunDetails& runner_and_results) {
  DCHECK_EQ(timed_out, !threat_type.has_value());
  base::TimeDelta time_taken = runner_and_results.runner->GetRunDuration();
  bool had_warning =
      !timed_out && (threat_type == SBThreatType::SB_THREAT_TYPE_URL_PHISHING ||
                     threat_type == SBThreatType::SB_THREAT_TYPE_URL_MALWARE ||
                     threat_type == SBThreatType::SB_THREAT_TYPE_URL_UNWANTED ||
                     threat_type == SBThreatType::SB_THREAT_TYPE_BILLING);
  runner_and_results.results =
      MechanismResults(time_taken, had_warning, timed_out);
  MaybeCompleteExperiment();
  // NOTE: Calling |MaybeCompleteExperiment| may result in the synchronous
  // destruction of this object, so there is nothing safe to do here but return.
}
void SafeBrowsingLookupMechanismExperimenter::MaybeCompleteExperiment() {
  if (checks_to_run_.empty()) {
    // This can happen if the experiment was canceled, in which case we don't
    // want to log any results. It can also happen if the experiment wasn't run
    // at all.
    return;
  }
  auto& latest_check = checks_to_run_.back();
  if (!latest_check->hash_database_details.results.has_value() ||
      !latest_check->hash_real_time_details.results.has_value() ||
      !latest_check->url_real_time_details.results.has_value() ||
      num_checks_with_eligibility_determined_ < checks_to_run_.size() ||
      (!will_process_response_reached_time_.has_value() &&
       !is_browser_url_loader_throttle_checker_on_io_destructed_)) {
    // The results are not yet complete.
    return;
  }
#if DCHECK_IS_ON()
  DCHECK(!is_experiment_complete_);
  is_experiment_complete_ = true;
#endif
  if (AreAnyChecksEligibleForLogging()) {
    LogExperimentResults();
  }
  EndExperiment();
  // NOTE: Calling |EndExperiment| may result in the synchronous destruction
  // of this object, so there is nothing safe to do here but return.
}

bool SafeBrowsingLookupMechanismExperimenter::AreAnyChecksEligibleForLogging() {
  bool any_eligible = false;
  bool all_eligible = true;
  for (auto& check : checks_to_run_) {
    if (check->would_check_show_warning_if_unsafe.value()) {
      any_eligible = true;
    } else {
      all_eligible = false;
    }
  }
  if (checks_to_run_.size() > 1 && any_eligible) {
    base::UmaHistogramBoolean(
        "SafeBrowsing.HPRTExperiment.Redirects.AllChecksEligible",
        all_eligible);
  }
  return any_eligible;
}

void SafeBrowsingLookupMechanismExperimenter::LogExperimentResults() {
  if (checks_to_run_.size() == 1) {
    auto& single_check = checks_to_run_.back();
    DCHECK(single_check->would_check_show_warning_if_unsafe.value());
    LogAggregatedResults("",
                         single_check->url_real_time_details.results.value(),
                         single_check->hash_database_details.results.value(),
                         single_check->hash_real_time_details.results.value());
  } else {
    auto url_real_time_results = AggregateRedirectInfo(base::BindRepeating(
        [](std::unique_ptr<CheckToRun>& check) -> MechanismResults& {
          return check->url_real_time_details.results.value();
        }));
    auto hash_database_results = AggregateRedirectInfo(base::BindRepeating(
        [](std::unique_ptr<CheckToRun>& check) -> MechanismResults& {
          return check->hash_database_details.results.value();
        }));
    auto hash_real_time_results = AggregateRedirectInfo(base::BindRepeating(
        [](std::unique_ptr<CheckToRun>& check) -> MechanismResults& {
          return check->hash_real_time_details.results.value();
        }));
    LogAggregatedResults("Redirects.", url_real_time_results,
                         hash_database_results, hash_real_time_results);
  }
}
void SafeBrowsingLookupMechanismExperimenter::LogAggregatedResults(
    const std::string& redirects_qualifier,
    MechanismResults& url_real_time_results,
    MechanismResults& hash_database_results,
    MechanismResults& hash_real_time_results) const {
  LogCombinedResults(redirects_qualifier, url_real_time_results,
                     hash_database_results, hash_real_time_results);

  LogPairedResults(redirects_qualifier, url_real_time_results,
                   hash_database_results, "URT", "HPD");
  LogPairedResults(redirects_qualifier, url_real_time_results,
                   hash_real_time_results, "URT", "HPRT");
  LogPairedResults(redirects_qualifier, hash_database_results,
                   hash_real_time_results, "HPD", "HPRT");

  LogIndividualMechanismResult(redirects_qualifier, url_real_time_results,
                               "URT");
  LogIndividualMechanismResult(redirects_qualifier, hash_database_results,
                               "HPD");
  LogIndividualMechanismResult(redirects_qualifier, hash_real_time_results,
                               "HPRT");
}
void SafeBrowsingLookupMechanismExperimenter::LogPairedResults(
    const std::string& redirects_qualifier,
    MechanismResults& results1,
    MechanismResults& results2,
    const std::string& acronym1,
    const std::string& acronym2) const {
  // Possible logs:
  //  - SafeBrowsing.HPRTExperiment[.Redirects].URTFasterThanHPDAmount
  //  - SafeBrowsing.HPRTExperiment[.Redirects].HPDFasterThanURTAmount
  //  - SafeBrowsing.HPRTExperiment[.Redirects].URTFasterThanHPRTAmount
  //  - SafeBrowsing.HPRTExperiment[.Redirects].HPRTFasterThanURTAmount
  //  - SafeBrowsing.HPRTExperiment[.Redirects].HPDFasterThanHPRTAmount
  //  - SafeBrowsing.HPRTExperiment[.Redirects].HPRTFasterThanHPDAmount
  if (results1.time_taken < results2.time_taken) {
    auto histogram_name =
        base::StrCat({"SafeBrowsing.HPRTExperiment.", redirects_qualifier,
                      acronym1, "FasterThan", acronym2, "Amount"});
    base::UmaHistogramTimes(histogram_name,
                            results2.time_taken - results1.time_taken);

  } else {
    auto histogram_name =
        base::StrCat({"SafeBrowsing.HPRTExperiment.", redirects_qualifier,
                      acronym2, "FasterThan", acronym1, "Amount"});
    base::UmaHistogramTimes(histogram_name,
                            results1.time_taken - results2.time_taken);
  }
}
void SafeBrowsingLookupMechanismExperimenter::LogCombinedResults(
    const std::string& redirects_qualifier,
    MechanismResults& url_real_time_results,
    MechanismResults& hash_database_results,
    MechanismResults& hash_real_time_results) const {
  // Possible logs:
  //  - SafeBrowsing.HPRTExperiment[.Redirects].WarningsResult
  //  - SafeBrowsing.HPRTExperiment[.Redirects].TimedOutResult
  //  - SafeBrowsing.HPRTExperiment[.Redirects].DelayedResponseResult

  // Warnings
  base::UmaHistogramEnumeration(
      base::StrCat({"SafeBrowsing.HPRTExperiment.", redirects_qualifier,
                    "WarningsResult"}),
      CombineBoolResults(url_real_time_results.had_warning,
                         hash_database_results.had_warning,
                         hash_real_time_results.had_warning));
  // Timed out
  base::UmaHistogramEnumeration(
      base::StrCat({"SafeBrowsing.HPRTExperiment.", redirects_qualifier,
                    "TimedOutResult"}),
      CombineBoolResults(url_real_time_results.timed_out,
                         hash_database_results.timed_out,
                         hash_real_time_results.timed_out));
  // Caused delay
  auto url_real_time_delay_info = GetDelayInformation(url_real_time_results);
  auto hash_database_delay_info = GetDelayInformation(hash_database_results);
  auto hash_real_time_delay_info = GetDelayInformation(hash_real_time_results);
  if (url_real_time_delay_info.delayed_response !=
      ExperimentUnknownNoYesResult::kUnknown) {
    base::UmaHistogramEnumeration(
        base::StrCat({"SafeBrowsing.HPRTExperiment.", redirects_qualifier,
                      "DelayedResponseResult"}),
        CombineBoolResults(url_real_time_delay_info.delayed_response ==
                               ExperimentUnknownNoYesResult::kYes,
                           hash_database_delay_info.delayed_response ==
                               ExperimentUnknownNoYesResult::kYes,
                           hash_real_time_delay_info.delayed_response ==
                               ExperimentUnknownNoYesResult::kYes));
  }
}
void SafeBrowsingLookupMechanismExperimenter::LogIndividualMechanismResult(
    const std::string& redirects_qualifier,
    MechanismResults& results,
    const std::string& acronym) const {
  // Possible logs:
  //  - SafeBrowsing.HPRTExperiment[.Redirects].URT.TimeTaken
  //  - SafeBrowsing.HPRTExperiment[.Redirects].HPRT.TimeTaken
  //  - SafeBrowsing.HPRTExperiment[.Redirects].HPD.TimeTaken
  //  - SafeBrowsing.HPRTExperiment[.Redirects].URT.TimedOut
  //  - SafeBrowsing.HPRTExperiment[.Redirects].HPRT.TimedOut
  //  - SafeBrowsing.HPRTExperiment[.Redirects].HPD.TimedOut
  //  - SafeBrowsing.HPRTExperiment[.Redirects].URT.DelayedResponse
  //  - SafeBrowsing.HPRTExperiment[.Redirects].HPRT.DelayedResponse
  //  - SafeBrowsing.HPRTExperiment[.Redirects].HPD.DelayedResponse
  //  - SafeBrowsing.HPRTExperiment[.Redirects].URT.DelayedResponseAmount
  //  - SafeBrowsing.HPRTExperiment[.Redirects].HPRT.DelayedResponseAmount
  //  - SafeBrowsing.HPRTExperiment[.Redirects].HPD.DelayedResponseAmount
  auto histogram_prefix = base::StrCat(
      {"SafeBrowsing.HPRTExperiment.", redirects_qualifier, acronym});
  base::UmaHistogramTimes(base::StrCat({histogram_prefix, ".TimeTaken"}),
                          results.time_taken);
  base::UmaHistogramBoolean(base::StrCat({histogram_prefix, ".TimedOut"}),
                            results.timed_out);

  auto delay_information = GetDelayInformation(results);
  base::UmaHistogramEnumeration(
      base::StrCat({histogram_prefix, ".DelayedResponse"}),
      delay_information.delayed_response);
  if (delay_information.delayed_response_amount.has_value()) {
    base::UmaHistogramTimes(
        base::StrCat({histogram_prefix, ".DelayedResponseAmount"}),
        delay_information.delayed_response_amount.value());
  }
}
SafeBrowsingLookupMechanismExperimenter::DelayInformation
SafeBrowsingLookupMechanismExperimenter::GetDelayInformation(
    MechanismResults& results) const {
  DelayInformation delay_information;
  if (will_process_response_reached_time_.has_value()) {
    DCHECK(first_check_start_time_.has_value());
    bool delayed_response =
        first_check_start_time_.value() + results.time_taken >
        will_process_response_reached_time_.value();
    delay_information.delayed_response =
        delayed_response ? ExperimentUnknownNoYesResult::kYes
                         : ExperimentUnknownNoYesResult::kNo;
    delay_information.delayed_response_amount =
        delayed_response
            ? (will_process_response_reached_time_.value() -
               (first_check_start_time_.value() + results.time_taken))
            : base::TimeDelta();
  } else {
    // If the URL real-time check results in a warning, there might never
    // be a call to WillProcessResponse. In these cases, we log "Unknown"
    // because we don't know if the other mechanisms would have delayed
    // the response time. We could log this fewer times either (1) for URL
    // real-time checks generally, since we know they didn't delay the
    // response, or (2) for other checks if they were faster than the URL
    // real-time check since that check didn't delay the response.
    // However, we avoid doing either so that we don't skew the results.
    // This might also happen if something upstream decides to destruct
    // BrowserUrlLoaderThrottle before the page has loaded, but after the
    // lookups have completed.
    delay_information.delayed_response = ExperimentUnknownNoYesResult::kUnknown;
  }
  return delay_information;
}
SafeBrowsingLookupMechanismExperimenter::MechanismResults
SafeBrowsingLookupMechanismExperimenter::AggregateRedirectInfo(
    base::RepeatingCallback<MechanismResults&(std::unique_ptr<CheckToRun>&)>
        get_results) {
  bool had_warning = false;
  bool timed_out = false;
  base::TimeDelta time_taken = base::TimeDelta();
  for (auto& check : checks_to_run_) {
    if (get_results.Run(check).had_warning) {
      // Only count it as a warning for the particular check if it would have
      // shown a warning.
      had_warning = check->would_check_show_warning_if_unsafe.value();
    }
    if (get_results.Run(check).timed_out) {
      timed_out = true;
    }
    time_taken += get_results.Run(check).time_taken;
  }
  return MechanismResults(time_taken, had_warning, timed_out);
}
// static
SafeBrowsingLookupMechanismExperimenter::ExperimentAllInOneResult
SafeBrowsingLookupMechanismExperimenter::CombineBoolResults(
    bool url_real_time_result,
    bool hash_database_result,
    bool hash_real_time_result) {
  return hash_database_result && url_real_time_result && hash_real_time_result
             ? ExperimentAllInOneResult::kAllMechanisms
         : hash_database_result && url_real_time_result
             ? ExperimentAllInOneResult::kUrlRealTimeAndHashDatabase
         : hash_database_result && hash_real_time_result
             ? ExperimentAllInOneResult::kHashDatabaseAndHashRealTime
         : hash_real_time_result && url_real_time_result
             ? ExperimentAllInOneResult::kUrlRealTimeAndHashRealTime
         : hash_database_result  ? ExperimentAllInOneResult::kHashDatabaseOnly
         : hash_real_time_result ? ExperimentAllInOneResult::kHashRealTimeOnly
         : url_real_time_result  ? ExperimentAllInOneResult::kUrlRealTimeOnly
                                 : ExperimentAllInOneResult::kNoMechanism;
}

bool SafeBrowsingLookupMechanismExperimenter::IsCheckInExperiment(
    size_t safe_browsing_url_checker_index) {
  return base::Contains(safe_browsing_url_checker_index_to_experimenter_index_,
                        safe_browsing_url_checker_index);
}
void SafeBrowsingLookupMechanismExperimenter::SetCheckExperimentEligibility(
    size_t safe_browsing_url_checker_index,
    bool is_eligible_for_experiment) {
  if (checks_to_run_.empty()) {
    // The experiment already ended. Can happen if it's the second call from
    // SafeBrowsingUrlCheckerImpl and the first call was the last thing the
    // experiment was waiting on.
    return;
  }
  if (!IsCheckInExperiment(safe_browsing_url_checker_index)) {
    DCHECK(false);
    return;
  }
  auto index = safe_browsing_url_checker_index_to_experimenter_index_
      [safe_browsing_url_checker_index];
  if (index >= checks_to_run_.size()) {
    DCHECK(false);
    return;
  }
  if (checks_to_run_[index]->would_check_show_warning_if_unsafe.has_value()) {
    // It's not unexpected that the check might already have this populated,
    // since SafeBrowsingUrlCheckerImpl might try to populate it twice.
    return;
  }
  // In addition to applying results provided by the caller of this method, we
  // also set |would_check_show_warning_if_unsafe| to false if the request is a
  // prefetch request. We do this here instead of just not running the whole
  // experiment because we still want the 2 backgrounded mechanisms to be able
  // to cache the results as they would normally, so that later requests within
  // the experiment can benefit from those cached results.
  checks_to_run_[index]->would_check_show_warning_if_unsafe =
      is_eligible_for_experiment && !is_prefetch_;
  num_checks_with_eligibility_determined_++;
  MaybeCompleteExperiment();
  // NOTE: Calling |MaybeCompleteExperiment| may result in the synchronous
  // destruction of this object, so there is nothing safe to do here but return.
}

void SafeBrowsingLookupMechanismExperimenter::
    OnBrowserUrlLoaderThrottleCheckerOnIODestructed() {
  is_browser_url_loader_throttle_checker_on_io_destructed_ = true;
  if (!will_process_response_reached_time_.has_value()) {
    MaybeCompleteExperiment();
    // Normally it can be dangerous to run code after a call to
    // |MaybeCompleteExperiment| due to the possible synchronous destruction of
    // this object, but in this case it would be safe. This is because the
    // caller (BrowserUrlLoaderThrottle::CheckerOnIO) is calling into this
    // function from its destructor, meaning at least until this method ends it
    // still has a reference to the experimenter.
  }
}
void SafeBrowsingLookupMechanismExperimenter::
    OnSafeBrowsingUrlCheckerImplDestructed() {
  if (checks_to_run_.empty()) {
    // The experiment already ended or never started.
    return;
  }
  auto& latest_check = checks_to_run_.back();
  if (!latest_check->url_real_time_details.results.has_value()) {
    // Cancel the experiment if SBUCI is destructed but the URL real-time lookup
    // results haven't completed.
    EndExperiment();
    //  Normally it can be dangerous to run code after a call to
    //  |EndExperiment| due to the possible synchronous destruction of this
    //  object, but in this case it would be safe. This is because the caller
    //  (SafeBrowsingUrlCheckerImpl) is calling into this function from its
    //  destructor, meaning at least until this method ends it still has a
    //  reference to the experimenter.
  }
}

void SafeBrowsingLookupMechanismExperimenter::EndExperiment() {
  weak_factory_.InvalidateWeakPtrs();
  // Can't use |checks_to_run_.clear()| because that implementation might
  // use *this* after it has been freed.
  std::vector<std::unique_ptr<CheckToRun>>().swap(checks_to_run_);
  // NOTE: Resetting the checks may result in the synchronous destruction of
  // this object, so there is nothing safe to do here but return.
}

SafeBrowsingLookupMechanismExperimenter::CheckToRun::CheckToRun(
    std::unique_ptr<SafeBrowsingLookupMechanismRunner> url_real_time_runner,
    std::unique_ptr<SafeBrowsingLookupMechanismRunner> hash_database_runner,
    std::unique_ptr<SafeBrowsingLookupMechanismRunner> hash_real_time_runner,
    SafeBrowsingLookupMechanismRunner::CompleteCheckCallbackWithTimeout
        url_real_time_result_callback)
    : hash_database_details(RunDetails(std::move(hash_database_runner))),
      hash_real_time_details(RunDetails(std::move(hash_real_time_runner))),
      url_real_time_details(
          UrlRealTimeRunDetails(std::move(url_real_time_runner),
                                std::move(url_real_time_result_callback))) {}
SafeBrowsingLookupMechanismExperimenter::CheckToRun::~CheckToRun() = default;
SafeBrowsingLookupMechanismExperimenter::MechanismResults::MechanismResults(
    base::TimeDelta time_taken,
    bool had_warning,
    bool timed_out)
    : time_taken(time_taken), had_warning(had_warning), timed_out(timed_out) {}
SafeBrowsingLookupMechanismExperimenter::MechanismResults::~MechanismResults() =
    default;
SafeBrowsingLookupMechanismExperimenter::CheckToRun::RunDetails::RunDetails(
    std::unique_ptr<SafeBrowsingLookupMechanismRunner> runner)
    : runner(std::move(runner)) {}
SafeBrowsingLookupMechanismExperimenter::CheckToRun::RunDetails::~RunDetails() =
    default;
SafeBrowsingLookupMechanismExperimenter::CheckToRun::UrlRealTimeRunDetails::
    UrlRealTimeRunDetails(
        std::unique_ptr<SafeBrowsingLookupMechanismRunner> runner,
        SafeBrowsingLookupMechanismRunner::CompleteCheckCallbackWithTimeout
            url_result_callback)
    : RunDetails(std::move(runner)),
      url_result_callback(std::move(url_result_callback)) {}
SafeBrowsingLookupMechanismExperimenter::CheckToRun::UrlRealTimeRunDetails::
    ~UrlRealTimeRunDetails() = default;
}  // namespace safe_browsing
