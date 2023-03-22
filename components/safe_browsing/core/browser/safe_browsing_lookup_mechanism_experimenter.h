// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_SAFE_BROWSING_LOOKUP_MECHANISM_EXPERIMENTER_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_SAFE_BROWSING_LOOKUP_MECHANISM_EXPERIMENTER_H_

#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/browser/hashprefix_realtime/hash_realtime_service.h"
#include "components/safe_browsing/core/browser/ping_manager.h"
#include "components/safe_browsing/core/browser/realtime/url_lookup_service_base.h"
#include "components/safe_browsing/core/browser/safe_browsing_lookup_mechanism.h"
#include "components/safe_browsing/core/browser/safe_browsing_lookup_mechanism_runner.h"
#include "components/safe_browsing/core/browser/url_realtime_mechanism.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "url/gurl.h"

namespace safe_browsing {

// When eligible, this class kicks off all three lookup mechanisms (hash-prefix
// database lookup, hash-prefix real-time lookup, URL real-time lookup) instead
// of solely the URL real-time lookup. The two other mechanisms are performed in
// the background and do not influence the main URL real-time lookup result.
// Once all three lookups complete, this class logs their combined results.
// Since a lookup can have redirects, the consumer of RunChecks is responsible
// for calling RunChecks for every redirect, which will kick off another lookup
// for each mechanism. This class will wait until all redirects have completed
// before ending the experiment and logging the results.
// This class should only be used on the IO thread.
// TODO(crbug.com/1410253): Delete this class once the temporary experiment is
// complete.
class SafeBrowsingLookupMechanismExperimenter
    : public base::RefCounted<SafeBrowsingLookupMechanismExperimenter> {
 public:
  SafeBrowsingLookupMechanismExperimenter(
      bool is_prefetch,
      base::WeakPtr<PingManager> ping_manager_on_ui,
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner);
  SafeBrowsingLookupMechanismExperimenter(
      const SafeBrowsingLookupMechanismExperimenter&) = delete;
  SafeBrowsingLookupMechanismExperimenter& operator=(
      const SafeBrowsingLookupMechanismExperimenter&) = delete;

  // Kicks off the experiment. This creates three separate runners (one per
  // mechanism), so it requires the union of the parameters required for each
  // SafeBrowsingLookupMechanism. See the constructors of the individual
  // mechanisms for more details on individual parameters.
  // While this function kicks off all three lookups, the only one that matters
  // from the consumer's perspective is the URL real-time lookup, since that is
  // what will be used to determine whether to display a warning. Therefore, the
  // only callback passed through is |url_real_time_result_callback|, which is
  // called when the URL real-time check completes.
  SafeBrowsingLookupMechanism::StartCheckResult RunChecks(
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
      base::WeakPtr<RealTimeUrlLookupServiceBase> url_lookup_service_on_ui,
      UrlRealTimeMechanism::WebUIDelegate* webui_delegate,
      base::WeakPtr<HashRealTimeService> hash_real_time_service_on_ui);

  // This records the time that WillProcessResponse was called by
  // BrowserURLLoaderThrottle, which is used for logs when the experiment
  // completes. If this is the last piece of data the experiment was waiting on,
  // the experiment will end.
  void OnWillProcessResponseReached(base::TimeTicks reached_time);

  // The BrowserUrlLoaderThrottle's CheckerOnSB can be destructed before
  // WillProcessResponse is reached. If this is the case, if the time
  // WillProcessResponse was reached was the last piece of data the experiment
  // was waiting on, the experiment will end.
  void OnBrowserUrlLoaderThrottleCheckerOnSBDestructed();

  // If SafeBrowsingUrlCheckerImpl is destructed before the latest URL real-time
  // check has completed, the experiment end with the results being thrown away.
  void OnSafeBrowsingUrlCheckerImplDestructed();

  // Determines whether the particular SafeBrowsingUrlCheckerImpl |urls_| index
  // has been used to start a lookup for the experiment.
  bool IsCheckInExperiment(size_t safe_browsing_url_checker_index);

  // There is some post-logic after a server-side endpoint says a URL is
  // unsafe that determines whether a warning should be shown. A check is only
  // eligible for the experiment if it would show a warning if deemed unsafe.
  // It might complete the experiment if this is the last awaited item.
  void SetCheckExperimentEligibility(size_t safe_browsing_url_checker_index,
                                     bool is_eligible_for_experiment);

 private:
  friend class base::RefCounted<SafeBrowsingLookupMechanismExperimenter>;
  friend class SafeBrowsingLookupMechanismExperimenterTest;
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingLookupMechanismExperimenterTest,
                           TestGetDelayInformation);

  // Private destructor since the class is ref counted. We only want RefCounted
  // to be able to destruct this object.
  ~SafeBrowsingLookupMechanismExperimenter();

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class ExperimentAllInOneResult {
    kNoMechanism = 0,
    kUrlRealTimeOnly = 1,
    kHashDatabaseOnly = 2,
    kHashRealTimeOnly = 3,
    kUrlRealTimeAndHashDatabase = 4,
    kUrlRealTimeAndHashRealTime = 5,
    kHashDatabaseAndHashRealTime = 6,
    kAllMechanisms = 7,
    kMaxValue = kAllMechanisms
  };
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class ExperimentUnknownNoYesResult {
    kUnknown = 0,
    kNo = 1,
    kYes = 2,
    kMaxValue = kYes
  };
  // Contains the results of a particular mechanism's run.
  struct MechanismResults {
    MechanismResults(
        base::TimeDelta time_taken,
        bool had_warning,
        bool timed_out,
        absl::optional<SBThreatType> threat_type,
        absl::optional<SBThreatType> locally_cached_results_threat_type,
        absl::optional<bool> real_time_request_failed);
    ~MechanismResults();
    // How long the run took.
    base::TimeDelta time_taken;
    // Whether the run resulted in a warning.
    bool had_warning;
    // Whether the run did not complete in time.
    bool timed_out;

    // The following three fields (|threat_type|,
    // |locally_cached_results_threat_type|, |real_time_request_failed|) are
    // only used for URL-level validation logging.

    // The result threat type. Can be unpopulated if the run timed out.
    absl::optional<SBThreatType> threat_type;
    // The result threat type using only locally cached results. This is only
    // populated if a real-time check's local cache was checked, and if the run
    // did not time out.
    absl::optional<SBThreatType> locally_cached_results_threat_type;
    // Whether the request failed due to backoff, network errors, or other
    // service unavailability. This only applies to real-time checks, and is
    // false otherwise. This is only populated if the run did not time out.
    absl::optional<bool> real_time_request_failed;
  };
  // This represents all the information necessary to run a specific check
  // (across all 3 relevant mechanisms) as well as the results of each
  // individual lookup once it completes.
  struct CheckToRun {
    CheckToRun(
        const GURL& url,
        std::unique_ptr<SafeBrowsingLookupMechanismRunner> url_real_time_runner,
        std::unique_ptr<SafeBrowsingLookupMechanismRunner> hash_database_runner,
        std::unique_ptr<SafeBrowsingLookupMechanismRunner>
            hash_real_time_runner,
        SafeBrowsingLookupMechanismRunner::CompleteCheckCallbackWithTimeout
            url_real_time_result_callback);
    ~CheckToRun();
    // Represents a specific mechanism lookup's inputs and outputs.
    struct RunDetails {
      std::unique_ptr<SafeBrowsingLookupMechanismRunner> runner;
      // This is separate from |results| because it is populated (if relevant)
      // when the run starts, rather than when the run completes.
      absl::optional<bool> matched_global_cache;
      absl::optional<MechanismResults> results;
      explicit RunDetails(
          std::unique_ptr<SafeBrowsingLookupMechanismRunner> runner);
      ~RunDetails();
    };
    // For URL real-time checks specifically, one of the outputs is also the
    // callback that should be triggered once the lookup completes.
    struct UrlRealTimeRunDetails : RunDetails {
      UrlRealTimeRunDetails(
          std::unique_ptr<SafeBrowsingLookupMechanismRunner> runner,
          SafeBrowsingLookupMechanismRunner::CompleteCheckCallbackWithTimeout
              url_result_callback);
      ~UrlRealTimeRunDetails();
      SafeBrowsingLookupMechanismRunner::CompleteCheckCallbackWithTimeout
          url_result_callback;
    };

    // The URL used for the lookup. This field is used for URL-level validation
    // logging.
    GURL url;
    // The following three are the RunDetails corresponding to each mechanism
    // lookup.
    RunDetails hash_database_details;
    RunDetails hash_real_time_details;
    UrlRealTimeRunDetails url_real_time_details;
    // There is some post-logic after a server-side endpoint says a URL is
    // unsafe that determines whether a warning should be shown. Once the
    // real-time URL lookup completes, this value is populated with true or
    // false and affects whether or not this check's results are logged. Before
    // the lookup has completed, this value is unpopulated.
    absl::optional<bool> would_check_show_warning_if_unsafe;
  };

  struct DelayInformation {
    // Whether the response was delayed because of this mechanism.
    ExperimentUnknownNoYesResult delayed_response;
    // The amount the response was delayed. If it wasn't delayed, it's the
    // default base::TimeDelta(). If it's unknown whether the response was
    // delayed, this is not populated.
    absl::optional<base::TimeDelta> delayed_response_amount;
  };

  // Helper function that does everything |RunChecks| does after the lookup
  // mechanism objects have been created.
  SafeBrowsingLookupMechanism::StartCheckResult RunChecksInternal(
      size_t safe_browsing_url_checker_index,
      const GURL& url,
      std::unique_ptr<SafeBrowsingLookupMechanism> url_real_time_mechanism,
      std::unique_ptr<SafeBrowsingLookupMechanism> hash_database_mechanism,
      std::unique_ptr<SafeBrowsingLookupMechanism> hash_real_time_mechanism,
      SafeBrowsingLookupMechanismRunner::CompleteCheckCallbackWithTimeout
          url_real_time_result_callback);
  // Called once the URL real-time lookup completes. It will store the
  // results of the lookup and then possibly end the experiment if this was
  // the last thing it was waiting on. It will also trigger informing the
  // original caller on the results of the URL real-time check.
  void OnUrlRealTimeCheckComplete(
      bool timed_out,
      absl::optional<
          std::unique_ptr<SafeBrowsingLookupMechanism::CompleteCheckResult>>
          result);
  // Called once the hash-prefix database check completes. It will store the
  // results of the lookup and then possibly end the experiment if this was the
  // last thing it was waiting on.
  void OnHashDatabaseCheckComplete(
      bool timed_out,
      absl::optional<
          std::unique_ptr<SafeBrowsingLookupMechanism::CompleteCheckResult>>
          result);
  void OnHashDatabaseCheckCompleteInternal(
      bool timed_out,
      absl::optional<SBThreatType> threat_type,
      absl::optional<SBThreatType> locally_cached_results_threat_type,
      absl::optional<bool> real_time_request_failed);
  // Kicks off the next hash-prefix database lookup available from the
  // |checks_to_run_| vector.
  void RunNextHashDatabaseCheck();
  // Called once the hash-prefix real-time check completes. It will store the
  // results of the lookup and then possibly end the experiment if this was the
  // last thing it was waiting on.
  void OnHashRealTimeCheckComplete(
      bool timed_out,
      absl::optional<
          std::unique_ptr<SafeBrowsingLookupMechanism::CompleteCheckResult>>
          result);
  // Kicks off the next hash-prefix real-time lookup available from the
  // |checks_to_run_| vector.
  void RunNextHashRealTimeCheck();

  // If all individual results are in, this will mark the experiment as
  // complete, aggregate the results, and log them. It will then clear out
  // |checks_to_run_| within the |EndExperiment| helper function, which can
  // delete the entire experimenter object if there are no remaining consumer
  // pointers to it.
  void MaybeCompleteExperiment();
  // Combines the results of all the lookups and logs them. This function should
  // only be called if at least one check is "eligible," i.e. that it has true
  // for |would_check_show_warning_if_unsafe|.
  void LogExperimentResults();
  // If none of the mechanisms timed out or reported they failed, but they did
  // not all agree on whether the URL was safe, this logs a
  // ClientSafeBrowsingReport for validation purposes that the results are
  // expected as part of ensuring that the new hash real-time mechanism is
  // working correctly. Note that the hash database mechanism never reports it
  // fails even if it does. This happens infrequently and isn't interesting for
  // debugging, so we ignore that possibility and log the results anyway. This
  // method is only called if there is only one check total.
  void MaybeLogUrlLevelResults() const;
  // Logs a ClientSafeBrowsingReport (as described above
  // |MaybeLogUrlLevelResults|). This method is static because it runs on the UI
  // thread.
  static void SendUrlLevelValidationReport(
      std::unique_ptr<ClientSafeBrowsingReportRequest> report,
      base::WeakPtr<PingManager> ping_manager_on_ui);
  // Once this is called, this object will be cleaned up either immediately or
  // shortly afterwards once other objects (SafeBrowsingUrlCheckerImpl or
  // BrowserUrlLoaderThrottle) with pointers to it have also been destructed.
  void EndExperiment();
  // Stores derived information onto |run_details| from the rest of th
  // parameters. It can also trigger ending the experiment if this result was
  // the last piece of information the experiment was waiting on.
  void StoreCheckResults(
      bool timed_out,
      absl::optional<SBThreatType> threat_type,
      absl::optional<SBThreatType> locally_cached_results_threat_type,
      absl::optional<bool> real_time_request_failed,
      CheckToRun::RunDetails& run_details);

  // Helper function to combine boolean results from each of the three
  // mechanisms into a single enum for logging.
  static ExperimentAllInOneResult CombineBoolResults(
      bool url_real_time_result,
      bool hash_database_result,
      bool hash_real_time_result);
  // Converts a |SBThreatType| to the one used for the hash real-time experiment
  // CSBRR.
  static absl::optional<ClientSafeBrowsingReportRequest::
                            HashRealTimeExperimentDetails::ExperimentThreatType>
  GetExperimentDetailsThreatType(absl::optional<SBThreatType> threat_type);
  // Returns details on whether the result caused a delay, and if so, by how
  // much.
  DelayInformation GetDelayInformation(MechanismResults& results) const;
  // Combines the mechanism redirect results all into a single result.
  MechanismResults AggregateRedirectInfo(
      base::RepeatingCallback<MechanismResults&(std::unique_ptr<CheckToRun>&)>
          get_results);
  // Kicks off combined, paired, and individual logs for the results.
  void LogAggregatedResults(const std::string& redirects_qualifier,
                            MechanismResults& url_real_time_results,
                            MechanismResults& hash_database_results,
                            MechanismResults& hash_real_time_results) const;
  // Combines the results of the 3 mechanisms into one log (and logs it).
  void LogCombinedResults(const std::string& redirects_qualifier,
                          MechanismResults& url_real_time_results,
                          MechanismResults& hash_database_results,
                          MechanismResults& hash_real_time_results) const;
  // Combines the results of 2 mechanisms into one log (and logs it).
  void LogPairedResults(const std::string& redirects_qualifier,
                        MechanismResults& results1,
                        MechanismResults& results2,
                        const std::string& acronym1,
                        const std::string& acronym2) const;
  // Logs details on the elapsed time, timeout status, and delay response
  // status for an individual mechanism result.
  void LogIndividualMechanismResult(const std::string& redirects_qualifier,
                                    MechanismResults& results,
                                    const std::string& acronym) const;
  // Returns whether any check has |would_check_show_warning_if_unsafe|
  // set to true.
  bool AreAnyChecksEligibleForLogging();

  // This represents the checks that the experiment will or already did run.
  // Each |CheckToRun| has the inputs that all 3 mechanism lookups need to be
  // run, and eventually it stores the outputs of all 3 mechanism lookups.
  std::vector<std::unique_ptr<CheckToRun>> checks_to_run_;
  // If there is an active hash-prefix database lookup being run, this
  // represents its index within |checks_to_run|. If not, then it's equal to the
  // size of |checks_to_run|.
  size_t hash_database_check_index_ = 0;
  // If there is an active hash-prefix real-time lookup being run, this
  // represents its index within |checks_to_run|. If not, then it's equal to the
  // size of |checks_to_run|.
  size_t hash_real_time_check_index_ = 0;
  // Represents the time WillProcessResponse was called in
  // BrowserUrlLoaderThrottle. It's not populated until that result is
  // available.
  absl::optional<base::TimeTicks> will_process_response_reached_time_;
  // Set to true when BrowserUrlLoaderThrottle::CheckerOnSB gets destructed.
  // If the check results in a warning, WillProcessResponse might never get
  // called, so this property being set ensures that the experiment doesn't keep
  // waiting for a call that will not come.
  bool is_browser_url_loader_throttle_checker_on_sb_destructed_ = false;
  // A check is not eligible for the experiment if it wouldn't even show a
  // warning if the result was unsafe. This eligibility can only be computed on
  // the UI thread, and must be done before the user sees a blocking page. To
  // avoid slowing down the real-time URL check, this eligibility is determined
  // in parallel with the lookup, so the experiment is not allowed to conclude
  // until the eligibility has been determined for all run checks.
  size_t num_checks_with_eligibility_determined_ = 0;
  // Mapping of the |urls_| index in SafeBrowsingUrlCheckerImpl to the
  // |checks_to_run_| index for a particular check. This is used to populate a
  // particular check's |would_check_show_warning_if_unsafe| value async.
  std::map<size_t, size_t>
      safe_browsing_url_checker_index_to_experimenter_index_;
#if DCHECK_IS_ON()
  // Used only for a DCHECK to confirm that the experiment is only completed
  // once.
  bool is_experiment_complete_ = false;
#endif
  // Set to the current time when the first check starts being run. This is used
  // to decide how long it took before WillProcessResponse was called.
  absl::optional<base::TimeTicks> first_check_start_time_;

  // Specifies whether the request is a prefetch request. If so, there will not
  // be any results logged. However, the experiment is still run so that the 2
  // backgrounded mechanisms are able to cache the results as they would
  // normally. Then, later requests within the experiment can benefit from those
  // cached results.
  bool is_prefetch_ = false;

  // Reference to the profile's PingManager. Used for conditionally logging a
  // CSBRR when the experiment ends for URL-level validation.
  base::WeakPtr<PingManager> ping_manager_on_ui_;

  // The task runner for the UI thread.
  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;

  base::WeakPtrFactory<SafeBrowsingLookupMechanismExperimenter> weak_factory_{
      this};
};
}  // namespace safe_browsing
#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_SAFE_BROWSING_LOOKUP_MECHANISM_EXPERIMENTER_H_
