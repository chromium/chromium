// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/safe_browsing_lookup_mechanism_experimenter.h"
#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/browser/safe_browsing_lookup_mechanism_runner.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace safe_browsing {

using Experimenter = SafeBrowsingLookupMechanismExperimenter;
using CompleteCheckCallbackWithTimeout =
    SafeBrowsingLookupMechanismRunner::CompleteCheckCallbackWithTimeout;

MATCHER_P(Matches, threat_type, "") {
  return arg.value()->threat_type == threat_type &&
         arg.value()->is_from_url_real_time_check;
}

class MockSafeBrowsingLookupMechanism : public SafeBrowsingLookupMechanism {
 public:
  explicit MockSafeBrowsingLookupMechanism(bool is_safe_synchronously,
                                           SBThreatType threat_type,
                                           base::TimeDelta time_to_completion,
                                           bool is_url_real_time)
      : SafeBrowsingLookupMechanism(
            GURL(),
            SBThreatTypeSet({}),
            /*database_manager=*/nullptr,
            /*can_check_db=*/true,
            MechanismExperimentHashDatabaseCache::kNoExperiment),
        is_safe_synchronously_(is_safe_synchronously),
        time_to_completion_(time_to_completion),
        threat_type_(threat_type),
        is_url_real_time_(is_url_real_time) {}

 private:
  StartCheckResult StartCheckInternal() override {
    if (!is_safe_synchronously_) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&MockSafeBrowsingLookupMechanism::CompleteCheck,
                         weak_factory_.GetWeakPtr(),
                         std::make_unique<CompleteCheckResult>(
                             url_, threat_type_, ThreatMetadata(),
                             /*is_from_url_real_time_check=*/is_url_real_time_,
                             /*url_real_time_lookup_response=*/nullptr)),
          time_to_completion_);
    }
    return StartCheckResult(is_safe_synchronously_,
                            /*did_check_url_real_time_allowlist=*/false);
  }

  // StartCheckInternal will return this value. Also, if it is true, the
  // callback won't be called.
  bool is_safe_synchronously_;
  // How long until the callback is called. (If greater than 5 seconds, the
  // runner should destruct this object before it had the chance to call the
  // callback.) If |is_safe_synchronously_| is true, this value will be ignored.
  base::TimeDelta time_to_completion_;
  // This is the resulting threat type that the mechanism will return. If
  // |is_safe_synchronously_| is true, this value will be ignored.
  SBThreatType threat_type_;
  // Whether this is the URL real-time mechanism. Used for the |CompleteCheck|
  // callback.
  bool is_url_real_time_;

  base::WeakPtrFactory<MockSafeBrowsingLookupMechanism> weak_factory_{this};
};

struct EligibilityConfig {
  EligibilityConfig(size_t safe_browsing_url_checker_index,
                    base::TimeDelta time_to_resolution,
                    bool eligibility)
      : safe_browsing_url_checker_index(safe_browsing_url_checker_index),
        time_to_resolution(time_to_resolution),
        eligibility(eligibility) {}
  size_t safe_browsing_url_checker_index;
  base::TimeDelta time_to_resolution;
  bool eligibility;
};

// This class is meant to simulate how the UrlCheckerDelegate interacts with the
// experimenter.
class PretendUrlCheckerDelegate {
 public:
  void SetEligibilityConfigs(
      scoped_refptr<SafeBrowsingLookupMechanismExperimenter>
          mechanism_experimenter,
      absl::optional<std::vector<EligibilityConfig>> eligibility_configs) {
    if (eligibility_configs.has_value()) {
      for (const auto& config : eligibility_configs.value()) {
        SetEligibilityConfig(mechanism_experimenter, config);
      }
    }
  }
  void SetEligibilityConfig(
      scoped_refptr<SafeBrowsingLookupMechanismExperimenter>
          mechanism_experimenter,
      EligibilityConfig config) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&PretendUrlCheckerDelegate::SetEligibility,
                       weak_factory_.GetWeakPtr(), mechanism_experimenter,
                       config.safe_browsing_url_checker_index,
                       config.eligibility),
        config.time_to_resolution);
  }

 private:
  void SetEligibility(scoped_refptr<SafeBrowsingLookupMechanismExperimenter>
                          mechanism_experimenter,
                      size_t safe_browsing_url_checker_index,
                      bool is_eligible_for_experiment) {
    mechanism_experimenter->SetCheckExperimentEligibility(
        safe_browsing_url_checker_index, is_eligible_for_experiment);
  }

  base::WeakPtrFactory<PretendUrlCheckerDelegate> weak_factory_{this};
};

// This class is meant to simulate how the SafeBrowsingUrlCheckerImpl interacts
// with the experimenter.
class PretendSafeBrowsingUrlCheckerImpl {
 public:
  explicit PretendSafeBrowsingUrlCheckerImpl(
      scoped_refptr<SafeBrowsingLookupMechanismExperimenter>
          mechanism_experimenter) {
    mechanism_experimenter_ = mechanism_experimenter;
  }
  ~PretendSafeBrowsingUrlCheckerImpl() {
    mechanism_experimenter_->OnSafeBrowsingUrlCheckerImplDestructed();
  }

 private:
  scoped_refptr<SafeBrowsingLookupMechanismExperimenter>
      mechanism_experimenter_;
};

// This class is meant to simulate how the BrowserUrlLoaderThrottle interacts
// with the experimenter.
class PretendCheckerOnIO {
 public:
  explicit PretendCheckerOnIO(base::TimeDelta time_to_will_process_response,
                              base::TimeDelta time_to_self_destruct,
                              bool is_prefetch) {
    mechanism_experimenter_ =
        base::MakeRefCounted<SafeBrowsingLookupMechanismExperimenter>(
            is_prefetch);
    safe_browsing_url_checker_impl_ =
        std::make_unique<PretendSafeBrowsingUrlCheckerImpl>(
            mechanism_experimenter_);
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&PretendCheckerOnIO::CallWillProcessResponse,
                       weak_factory_.GetWeakPtr()),
        time_to_will_process_response);
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&PretendCheckerOnIO::SelfDestruct,
                       weak_factory_.GetWeakPtr()),
        time_to_self_destruct);
  }
  ~PretendCheckerOnIO() {
    mechanism_experimenter_->OnBrowserUrlLoaderThrottleCheckerOnIODestructed();
  }
  scoped_refptr<SafeBrowsingLookupMechanismExperimenter> GetExperimenter() {
    return mechanism_experimenter_;
  }

 private:
  void CallWillProcessResponse() {
    mechanism_experimenter_->OnWillProcessResponseReached(
        base::TimeTicks::Now());
  }
  void SelfDestruct() { delete this; }

  std::unique_ptr<PretendSafeBrowsingUrlCheckerImpl>
      safe_browsing_url_checker_impl_;
  scoped_refptr<SafeBrowsingLookupMechanismExperimenter>
      mechanism_experimenter_;
  base::WeakPtrFactory<PretendCheckerOnIO> weak_factory_{this};
};

class SafeBrowsingLookupMechanismExperimenterTest : public PlatformTest {
 protected:
  using UnknownNoYesResult = Experimenter::ExperimentUnknownNoYesResult;
  using AllInOneResult = Experimenter::ExperimentAllInOneResult;

  struct DelayedResponseInfo {
    std::vector<UnknownNoYesResult> urt_hpd_hprt_delayed_responses;
    absl::optional<AllInOneResult> delayed_response_result;
  };

  void ResetMetrics() {
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  AllInOneResult CombineBoolResults(bool url_real_time_result,
                                    bool hash_database_result,
                                    bool hash_real_time_result) {
    return Experimenter::CombineBoolResults(
        url_real_time_result, hash_database_result, hash_real_time_result);
  }

  void RunChecks(
      scoped_refptr<Experimenter> mechanism_experimenter,
      size_t safe_browsing_url_checker_index,
      std::unique_ptr<SafeBrowsingLookupMechanism> url_real_time_mechanism,
      std::unique_ptr<SafeBrowsingLookupMechanism> hash_database_mechanism,
      std::unique_ptr<SafeBrowsingLookupMechanism> hash_real_time_mechanism,
      CompleteCheckCallbackWithTimeout url_real_time_result_callback) {
    mechanism_experimenter->RunChecksInternal(
        safe_browsing_url_checker_index, std::move(url_real_time_mechanism),
        std::move(hash_database_mechanism), std::move(hash_real_time_mechanism),
        std::move(url_real_time_result_callback));
  }
  void CreateAndRunChecks(
      scoped_refptr<SafeBrowsingLookupMechanismExperimenter>
          mechanism_experimenter,
      size_t safe_browsing_url_checker_index,
      std::vector<base::TimeDelta> urt_hpd_hprt_times_taken,
      std::vector<SBThreatType> urt_hpd_hprt_threat_types,
      CompleteCheckCallbackWithTimeout url_real_time_result_callback,
      bool immediately_resolve_eligibility) {
    DCHECK(urt_hpd_hprt_times_taken[1] != base::Seconds(0) ||
           urt_hpd_hprt_threat_types[1] == SB_THREAT_TYPE_SAFE);
    RunChecks(
        mechanism_experimenter, safe_browsing_url_checker_index,
        CreateUrlRealTimeMechanism(urt_hpd_hprt_threat_types[0],
                                   urt_hpd_hprt_times_taken[0]),
        urt_hpd_hprt_times_taken[1] == base::Seconds(0)
            ? CreateSyncHashDatabaseMechanism()
            : CreateAsyncHashDatabaseMechanism(urt_hpd_hprt_threat_types[1],
                                               urt_hpd_hprt_times_taken[1]),
        CreateHashRealTimeMechanism(urt_hpd_hprt_threat_types[2],
                                    urt_hpd_hprt_times_taken[2]),
        std::move(url_real_time_result_callback));

    if (immediately_resolve_eligibility) {
      mechanism_experimenter->SetCheckExperimentEligibility(
          safe_browsing_url_checker_index, true);
    }
  }

  scoped_refptr<SafeBrowsingLookupMechanismExperimenter> SetUpExperimenter(
      base::TimeDelta will_process_response_time_taken,
      base::TimeDelta checker_on_io_self_destruct_time,
      absl::optional<std::vector<EligibilityConfig>> eligibility_configs,
      bool is_prefetch) {
    // Created with 'new' so that it can live on past the end of this method but
    // also control its own lifetime via |time_to_self_destruct|.
    auto* checker_on_io = new PretendCheckerOnIO(
        /*time_to_will_process_response=*/will_process_response_time_taken,
        /*time_to_self_destruct=*/checker_on_io_self_destruct_time,
        is_prefetch);
    url_checker_delegate_->SetEligibilityConfigs(
        checker_on_io->GetExperimenter(), eligibility_configs);
    return checker_on_io->GetExperimenter();
  }

  // Creates a parent PretendCheckerOnIO that creates the experimenter. Then
  // creates the three lookup mechanisms to be tied to the experimenter, and
  // kicks off RunChecks. If the input HPD time taken (through
  // |urt_hpd_hprt_times_taken|) equals 0, the hash-database mechanism completes
  // as safe synchronously.
  scoped_refptr<SafeBrowsingLookupMechanismExperimenter>
  SetUpExperimenterAndChecks(
      std::vector<base::TimeDelta> urt_hpd_hprt_times_taken,
      std::vector<SBThreatType> urt_hpd_hprt_threat_types,
      base::TimeDelta will_process_response_time_taken,
      base::TimeDelta checker_on_io_self_destruct_time,
      CompleteCheckCallbackWithTimeout url_real_time_result_callback,
      absl::optional<std::vector<EligibilityConfig>> eligibility_configs,
      bool is_prefetch) {
    auto mechanism_experimenter = SetUpExperimenter(
        will_process_response_time_taken, checker_on_io_self_destruct_time,
        eligibility_configs, is_prefetch);
    CreateAndRunChecks(
        mechanism_experimenter, /*safe_browsing_url_checker_index=*/0,
        urt_hpd_hprt_times_taken, urt_hpd_hprt_threat_types,
        std::move(url_real_time_result_callback),
        /*immediately_resolve_eligibility=*/!eligibility_configs.has_value());
    return mechanism_experimenter;
  }

  std::unique_ptr<MockSafeBrowsingLookupMechanism> CreateUrlRealTimeMechanism(
      SBThreatType threat_type,
      base::TimeDelta time_to_completion) {
    return std::make_unique<MockSafeBrowsingLookupMechanism>(
        /*is_safe_synchronously=*/false, /*threat_type=*/threat_type,
        /*time_to_completion=*/time_to_completion, /*is_url_real_time=*/true);
  }
  std::unique_ptr<MockSafeBrowsingLookupMechanism> CreateHashRealTimeMechanism(
      SBThreatType threat_type,
      base::TimeDelta time_to_completion) {
    return std::make_unique<MockSafeBrowsingLookupMechanism>(
        /*is_safe_synchronously=*/false, /*threat_type=*/threat_type,
        /*time_to_completion=*/time_to_completion, /*is_url_real_time=*/false);
  }
  std::unique_ptr<MockSafeBrowsingLookupMechanism>
  CreateSyncHashDatabaseMechanism() {
    return std::make_unique<MockSafeBrowsingLookupMechanism>(
        /*is_safe_synchronously=*/true,
        /*threat_type=*/SB_THREAT_TYPE_SAFE,       // not used
        /*time_to_completion=*/base::TimeDelta(),  // not used
        /*is_url_real_time=*/false);
  }
  std::unique_ptr<MockSafeBrowsingLookupMechanism>
  CreateAsyncHashDatabaseMechanism(SBThreatType threat_type,
                                   base::TimeDelta time_to_completion) {
    return std::make_unique<MockSafeBrowsingLookupMechanism>(
        /*is_safe_synchronously=*/false,
        /*threat_type=*/threat_type,
        /*time_to_completion=*/time_to_completion,
        /*is_url_real_time=*/false);
  }

  void VerifyNoLogs() {
    // WarningsResult is always logged if there are any logs at all, so we only
    // need to check this. We do not want to use:
    // EXPECT_EQ(histogram_tester_->GetAllHistogramsRecorded().size(), 0u);
    // This is because there can be logs unrelated to HPRTExperiment that occur,
    // such as Scheduler.TaskQueueImpl.* logs.
    histogram_tester_->ExpectTotalCount(
        /*name=*/"SafeBrowsing.HPRTExperiment.WarningsResult",
        /*expected_count=*/0);
    histogram_tester_->ExpectTotalCount(
        /*name=*/"SafeBrowsing.HPRTExperiment.Redirects.WarningsResult",
        /*expected_count=*/0);
  }
  void VerifyLogs(
      std::vector<base::TimeDelta> expected_urt_hpd_hprt_times_taken,
      std::vector<bool> expected_urt_hpd_hprt_had_warnings,
      AllInOneResult expected_warnings_result,
      AllInOneResult expected_timed_out_result,
      base::TimeDelta will_process_response_time_taken,
      base::TimeDelta checker_on_io_self_destruct_time) {
    VerifyLogsAllowingRedirects(
        expected_urt_hpd_hprt_times_taken, expected_urt_hpd_hprt_times_taken,
        expected_urt_hpd_hprt_had_warnings,
        /*expected_had_redirects=*/false, /*expected_some_ineligible=*/false,
        expected_warnings_result, expected_timed_out_result,
        will_process_response_time_taken, checker_on_io_self_destruct_time);
  }
  void VerifyLogsAllowingRedirects(
      std::vector<base::TimeDelta> expected_summed_urt_hpd_hprt_times_taken,
      std::vector<base::TimeDelta> expected_max_urt_hpd_hprt_times_taken,
      std::vector<bool> expected_urt_hpd_hprt_had_warnings,
      bool expected_had_redirects,
      bool expected_some_ineligible,
      AllInOneResult expected_warnings_result,
      AllInOneResult expected_timed_out_result,
      base::TimeDelta will_process_response_time_taken,
      base::TimeDelta checker_on_io_self_destruct_time) {
    auto expected_delayed_response_info = GetExpectedDelayedResponseInfo(
        expected_summed_urt_hpd_hprt_times_taken,
        will_process_response_time_taken, checker_on_io_self_destruct_time);
    auto expected_urt_hpd_hprt_delayed_responses =
        expected_delayed_response_info.urt_hpd_hprt_delayed_responses;
    auto expected_delayed_response_result =
        expected_delayed_response_info.delayed_response_result;

    DCHECK(expected_summed_urt_hpd_hprt_times_taken.size() == 3);
    DCHECK(expected_max_urt_hpd_hprt_times_taken.size() == 3);
    DCHECK(expected_urt_hpd_hprt_delayed_responses.size() == 3);
    DCHECK(expected_urt_hpd_hprt_had_warnings.size() == 3);

    std::string histogram_prefix =
        expected_had_redirects ? "SafeBrowsing.HPRTExperiment.Redirects."
                               : "SafeBrowsing.HPRTExperiment.";
    std::vector<std::string> mechanisms = {"URT", "HPD", "HPRT"};

    for (auto i = 0; i < 3; i++) {
      histogram_tester_->ExpectTotalCount(
          /*name=*/histogram_prefix + mechanisms[i] + ".TimeTaken",
          /*expected_count=*/1);
      histogram_tester_->ExpectUniqueSample(
          /*name=*/histogram_prefix + mechanisms[i] + ".TimedOut",
          /*sample=*/expected_max_urt_hpd_hprt_times_taken[i] >
              base::Seconds(5),
          /*expected_bucket_count=*/1);
      histogram_tester_->ExpectUniqueSample(
          /*name=*/histogram_prefix + mechanisms[i] + ".DelayedResponse",
          /*sample=*/expected_urt_hpd_hprt_delayed_responses[i],
          /*expected_bucket_count=*/1);
      histogram_tester_->ExpectTotalCount(
          /*name=*/histogram_prefix + mechanisms[i] + ".DelayedResponseAmount",
          /*expected_count=*/expected_urt_hpd_hprt_delayed_responses[i] ==
                  UnknownNoYesResult::kUnknown
              ? 0
              : 1);
      if (expected_urt_hpd_hprt_delayed_responses[i] ==
          UnknownNoYesResult::kNo) {
        histogram_tester_->ExpectUniqueSample(
            /*name=*/histogram_prefix + mechanisms[i] +
                ".DelayedResponseAmount",
            /*sample=*/0,
            /*expected_bucket_count=*/1);
      }
    }

    bool urt_faster_than_hpd = expected_summed_urt_hpd_hprt_times_taken[0] <
                               expected_summed_urt_hpd_hprt_times_taken[1];
    bool urt_faster_than_hprt = expected_summed_urt_hpd_hprt_times_taken[0] <
                                expected_summed_urt_hpd_hprt_times_taken[2];
    bool hpd_faster_than_hprt = expected_summed_urt_hpd_hprt_times_taken[1] <
                                expected_summed_urt_hpd_hprt_times_taken[2];

    histogram_tester_->ExpectTotalCount(
        /*name=*/histogram_prefix + "URTFasterThanHPDAmount",
        /*expected_count=*/urt_faster_than_hpd ? 1 : 0);
    histogram_tester_->ExpectTotalCount(
        /*name=*/histogram_prefix + "HPDFasterThanURTAmount",
        /*expected_count=*/urt_faster_than_hpd ? 0 : 1);
    histogram_tester_->ExpectTotalCount(
        /*name=*/histogram_prefix + "URTFasterThanHPRTAmount",
        /*expected_count=*/urt_faster_than_hprt ? 1 : 0);
    histogram_tester_->ExpectTotalCount(
        /*name=*/histogram_prefix + "HPRTFasterThanURTAmount",
        /*expected_count=*/urt_faster_than_hprt ? 0 : 1);
    histogram_tester_->ExpectTotalCount(
        /*name=*/histogram_prefix + "HPDFasterThanHPRTAmount",
        /*expected_count=*/hpd_faster_than_hprt ? 1 : 0);
    histogram_tester_->ExpectTotalCount(
        /*name=*/histogram_prefix + "HPRTFasterThanHPDAmount",
        /*expected_count=*/hpd_faster_than_hprt ? 0 : 1);

    histogram_tester_->ExpectUniqueSample(
        /*name=*/histogram_prefix + "WarningsResult",
        /*sample=*/expected_warnings_result,
        /*expected_bucket_count=*/1);
    histogram_tester_->ExpectUniqueSample(
        /*name=*/histogram_prefix + "TimedOutResult",
        /*sample=*/expected_timed_out_result,
        /*expected_bucket_count=*/1);
    if (expected_delayed_response_result.has_value()) {
      histogram_tester_->ExpectUniqueSample(
          /*name=*/histogram_prefix + "DelayedResponseResult",
          /*sample=*/expected_delayed_response_result.value(),
          /*expected_bucket_count=*/1);
    }

    if (expected_had_redirects) {
      histogram_tester_->ExpectUniqueSample(
          /*name=*/"SafeBrowsing.HPRTExperiment.Redirects.AllChecksEligible",
          /*sample=*/!expected_some_ineligible,
          /*expected_bucket_count=*/1);
    } else {
      histogram_tester_->ExpectTotalCount(
          /*name=*/"SafeBrowsing.HPRTExperiment.Redirects.AllChecksEligible",
          /*expected_count=*/0);
    }
  }

  UnknownNoYesResult ToUnknownNoYesResult(bool input,
                                          bool should_be_unknown = false) {
    if (should_be_unknown) {
      return UnknownNoYesResult::kUnknown;
    }
    return input ? UnknownNoYesResult::kYes : UnknownNoYesResult::kNo;
  }

  DelayedResponseInfo GetExpectedDelayedResponseInfo(
      std::vector<base::TimeDelta> expected_urt_hpd_hprt_times_taken,
      base::TimeDelta will_process_response_time_taken,
      base::TimeDelta checker_on_io_self_destruct_time) {
    auto will_process_response_completes =
        will_process_response_time_taken < checker_on_io_self_destruct_time;
    auto urt_delayed_response =
        expected_urt_hpd_hprt_times_taken[0] > will_process_response_time_taken;
    auto hpd_delayed_response =
        expected_urt_hpd_hprt_times_taken[1] > will_process_response_time_taken;
    auto hprt_delayed_response =
        expected_urt_hpd_hprt_times_taken[2] > will_process_response_time_taken;
    auto expected_delayed_response_result =
        will_process_response_completes
            ? absl::optional<AllInOneResult>(Experimenter::CombineBoolResults(
                  urt_delayed_response, hpd_delayed_response,
                  hprt_delayed_response))
            : absl::nullopt;

    auto expected_urt_hpd_hprt_delayed_responses = {
        ToUnknownNoYesResult(
            urt_delayed_response,
            /*should_be_unknown=*/!will_process_response_completes),
        ToUnknownNoYesResult(
            hpd_delayed_response,
            /*should_be_unknown=*/!will_process_response_completes),
        ToUnknownNoYesResult(
            hprt_delayed_response,
            /*should_be_unknown=*/!will_process_response_completes)};

    DelayedResponseInfo info;
    info.delayed_response_result = expected_delayed_response_result;
    info.urt_hpd_hprt_delayed_responses =
        expected_urt_hpd_hprt_delayed_responses;
    return info;
  }

  // Should be used to test different combinations of eligibility configs.
  void RunEligibilityTest(std::vector<EligibilityConfig> eligibility_configs,
                          bool is_prefetch,
                          bool expect_logs) {
    std::vector<base::TimeDelta> urt_hpd_hprt_times_taken = {
        base::Seconds(0), base::Seconds(1), base::Seconds(2)};
    base::TimeDelta will_process_response_time_taken = base::Seconds(10);
    base::TimeDelta checker_on_io_self_destruct_time = base::Seconds(20);
    base::MockCallback<CompleteCheckCallbackWithTimeout>
        url_real_time_result_callback;
    EXPECT_CALL(url_real_time_result_callback,
                Run(false, Matches(SB_THREAT_TYPE_SAFE)))
        .Times(1);
    SetUpExperimenterAndChecks(
        urt_hpd_hprt_times_taken,
        /*urt_hpd_hprt_threat_types=*/
        {SB_THREAT_TYPE_SAFE, SB_THREAT_TYPE_SAFE, SB_THREAT_TYPE_SAFE},
        will_process_response_time_taken, checker_on_io_self_destruct_time,
        url_real_time_result_callback.Get(), eligibility_configs, is_prefetch);

    task_environment_.FastForwardUntilNoTasksRemain();

    if (expect_logs) {
      VerifyLogs(
          /*expected_urt_hpd_hprt_times_taken=*/urt_hpd_hprt_times_taken,
          /*expected_urt_hpd_hprt_had_warnings=*/{false, false, false},
          /*expected_warnings_result=*/AllInOneResult::kNoMechanism,
          /*expected_timed_out_result=*/AllInOneResult::kNoMechanism,
          will_process_response_time_taken, checker_on_io_self_destruct_time);
    } else {
      VerifyNoLogs();
    }
  }

  // Helper function for that runs different orderings of mechanism completions
  // / WillProcessResponse reached / CheckerOnIO destruction. The goal of this
  // is to ensure that the lifetime of the experimenter is handled correctly, as
  // well as to ensure that all results are included in the final logs.
  void RunLifetimesTest(std::vector<base::TimeDelta> urt_hpd_hprt_times_taken,
                        base::TimeDelta will_process_response_time_taken,
                        base::TimeDelta checker_on_io_self_destruct_time,
                        bool will_be_canceled) {
    base::MockCallback<CompleteCheckCallbackWithTimeout>
        url_real_time_result_callback;
    EXPECT_CALL(url_real_time_result_callback,
                Run(false, Matches(SB_THREAT_TYPE_SAFE)))
        .Times(will_be_canceled ? 0 : 1);
    SetUpExperimenterAndChecks(
        urt_hpd_hprt_times_taken,
        /*urt_hpd_hprt_threat_types=*/
        {SB_THREAT_TYPE_SAFE, SB_THREAT_TYPE_SAFE, SB_THREAT_TYPE_SAFE},
        will_process_response_time_taken, checker_on_io_self_destruct_time,
        url_real_time_result_callback.Get(),
        /*eligibility_configs=*/absl::nullopt,
        /*is_prefetch=*/false);

    task_environment_.FastForwardUntilNoTasksRemain();

    if (will_be_canceled) {
      VerifyNoLogs();
    } else {
      VerifyLogs(
          /*expected_urt_hpd_hprt_times_taken=*/urt_hpd_hprt_times_taken,
          /*expected_urt_hpd_hprt_had_warnings=*/{false, false, false},
          /*expected_warnings_result=*/AllInOneResult::kNoMechanism,
          /*expected_timed_out_result=*/AllInOneResult::kNoMechanism,
          will_process_response_time_taken, checker_on_io_self_destruct_time);
    }
  }

  // Similar to RunEligibilityTest except that |RunChecks| is called a total of
  // 3 times on the same experimenter.
  void RunEligibilityWithTwoRedirectsTest(
      absl::optional<std::vector<EligibilityConfig>> eligibility_configs,
      std::vector<std::vector<SBThreatType>> threat_types,
      bool is_prefetch) {
    auto urt_hpd_hprt_times_taken = {base::Seconds(2), base::Seconds(1),
                                     base::Seconds(3)};
    auto will_process_response_time_taken = base::Seconds(8);
    auto checker_on_io_self_destruct_time = base::Seconds(15);
    std::vector<bool> all_in_time = {false, false, false};
    RunRedirectsTestBase(threat_types,
                         {urt_hpd_hprt_times_taken, urt_hpd_hprt_times_taken,
                          urt_hpd_hprt_times_taken},
                         {all_in_time, all_in_time, all_in_time},
                         will_process_response_time_taken,
                         checker_on_io_self_destruct_time, eligibility_configs,
                         is_prefetch);
  }

  // Similar to RunLifetimesTest except that |RunChecks| is called a total of 3
  // times on the same experimenter.
  void RunLifetimesWithTwoRedirectsTest(
      std::vector<std::vector<base::TimeDelta>> urt_hpd_hprt_times_taken,
      base::TimeDelta will_process_response_time_taken,
      base::TimeDelta checker_on_io_self_destruct_time) {
    DCHECK(urt_hpd_hprt_times_taken.size() == 3u);
    std::vector<SBThreatType> all_safe = {
        SB_THREAT_TYPE_SAFE, SB_THREAT_TYPE_SAFE, SB_THREAT_TYPE_SAFE};
    std::vector<bool> all_in_time = {false, false, false};
    RunRedirectsTestBase(
        {all_safe, all_safe, all_safe}, urt_hpd_hprt_times_taken,
        {all_in_time, all_in_time, all_in_time},
        will_process_response_time_taken, checker_on_io_self_destruct_time,
        /*eligibility_configs=*/absl::nullopt, /*is_prefetch=*/false);
  }

  // Similar to RunWarningsTest except that |RunChecks| is called a total of 3
  // times on the same experimenter.
  void RunWarningsWithTwoRedirectsTest(
      std::vector<std::vector<SBThreatType>> urt_hpd_hprt_threat_types) {
    auto urt_hpd_hprt_times_taken = {base::Seconds(2), base::Seconds(1),
                                     base::Seconds(3)};
    auto will_process_response_time_taken = base::Seconds(12);
    auto checker_on_io_self_destruct_time = base::Seconds(15);
    std::vector<bool> all_in_time = {false, false, false};
    RunRedirectsTestBase(
        urt_hpd_hprt_threat_types,
        {urt_hpd_hprt_times_taken, urt_hpd_hprt_times_taken,
         urt_hpd_hprt_times_taken},
        {all_in_time, all_in_time, all_in_time},
        will_process_response_time_taken, checker_on_io_self_destruct_time,
        /*eligibility_configs=*/absl::nullopt, /*is_prefetch=*/false);
  }

  // Similar to RunTimeoutTest except that |RunChecks| is called a total of 3
  // times on the same experimenter.
  void RunTimeoutWithTwoRedirectsTest(
      std::vector<std::vector<bool>> urt_hpd_hprt_time_outs) {
    std::vector<std::vector<base::TimeDelta>> urt_hpd_hprt_times_taken = {
        {urt_hpd_hprt_time_outs[0][0] ? base::Seconds(6) : base::Seconds(0.1),
         urt_hpd_hprt_time_outs[0][1] ? base::Seconds(6) : base::Seconds(0.2),
         urt_hpd_hprt_time_outs[0][2] ? base::Seconds(6) : base::Seconds(0.3)},
        {urt_hpd_hprt_time_outs[1][0] ? base::Seconds(6) : base::Seconds(1.1),
         urt_hpd_hprt_time_outs[1][1] ? base::Seconds(6) : base::Seconds(1.2),
         urt_hpd_hprt_time_outs[1][2] ? base::Seconds(6) : base::Seconds(1.3)},
        {urt_hpd_hprt_time_outs[2][0] ? base::Seconds(6) : base::Seconds(2.1),
         urt_hpd_hprt_time_outs[2][1] ? base::Seconds(6) : base::Seconds(2.2),
         urt_hpd_hprt_time_outs[2][2] ? base::Seconds(6) : base::Seconds(2.3)}};
    auto will_process_response_time_taken = base::Seconds(20);
    auto checker_on_io_self_destruct_time = base::Seconds(25);
    std::vector<SBThreatType> all_safe = {
        SB_THREAT_TYPE_SAFE, SB_THREAT_TYPE_SAFE, SB_THREAT_TYPE_SAFE};
    RunRedirectsTestBase(
        {all_safe, all_safe, all_safe}, urt_hpd_hprt_times_taken,
        urt_hpd_hprt_time_outs, will_process_response_time_taken,
        checker_on_io_self_destruct_time,
        /*eligibility_configs=*/absl::nullopt, /*is_prefetch=*/false);
  }

  // Used by the RunRedirectsTestBase function to add 2 redirect checks.
  void CallbackForRedirects(
      scoped_refptr<Experimenter> experimenter,
      std::vector<std::vector<base::TimeDelta>> urt_hpd_hprt_times_taken,
      std::vector<std::vector<SBThreatType>> urt_hpd_hprt_threat_types,
      absl::optional<std::vector<EligibilityConfig>> eligibility_configs,
      int index,
      bool timed_out,
      absl::optional<
          std::unique_ptr<SafeBrowsingLookupMechanism::CompleteCheckResult>>
          result) {
    if (index == 3) {
      // Only do 2 subsequent redirects.
      return;
    }
    CreateAndRunChecks(
        experimenter,
        /*safe_browsing_url_checker_index=*/index,
        urt_hpd_hprt_times_taken[index], /*urt_hpd_hprt_threat_types=*/
        urt_hpd_hprt_threat_types[index],
        base::BindOnce(
            &SafeBrowsingLookupMechanismExperimenterTest::CallbackForRedirects,
            base::Unretained(this), experimenter, urt_hpd_hprt_times_taken,
            urt_hpd_hprt_threat_types, eligibility_configs, index + 1),
        /*immediately_resolve_eligibility=*/!eligibility_configs.has_value());
    if (eligibility_configs.has_value()) {
      url_checker_delegate_->SetEligibilityConfig(
          experimenter, eligibility_configs.value()[index]);
    }
  }

  void RunRedirectsTestBase(
      std::vector<std::vector<SBThreatType>> urt_hpd_hprt_threat_types,
      std::vector<std::vector<base::TimeDelta>> urt_hpd_hprt_times_taken,
      std::vector<std::vector<bool>> urt_hpd_hprt_time_outs,
      base::TimeDelta will_process_response_time_taken,
      base::TimeDelta checker_on_io_self_destruct_time,
      absl::optional<std::vector<EligibilityConfig>> eligibility_configs,
      bool is_prefetch) {
    DCHECK(!eligibility_configs.has_value() ||
           eligibility_configs.value().size() == 3u);
    // Used for deciding which mechanism was faster and for deciding if a
    // mechanism finished slower than WillProcessResponse.
    std::vector<base::TimeDelta> summed_urt_hpd_hprt_times_taken = {
        urt_hpd_hprt_times_taken[0][0] + urt_hpd_hprt_times_taken[1][0] +
            urt_hpd_hprt_times_taken[2][0],
        urt_hpd_hprt_times_taken[0][1] + urt_hpd_hprt_times_taken[1][1] +
            urt_hpd_hprt_times_taken[2][1],
        urt_hpd_hprt_times_taken[0][2] + urt_hpd_hprt_times_taken[1][2] +
            urt_hpd_hprt_times_taken[2][2],
    };
    // Used for deciding if a specific lookup timed out.
    std::vector<base::TimeDelta> max_urt_hpd_hprt_times_taken = {
        std::max({urt_hpd_hprt_times_taken[0][0],
                  urt_hpd_hprt_times_taken[1][0],
                  urt_hpd_hprt_times_taken[2][0]}),
        std::max({urt_hpd_hprt_times_taken[0][1],
                  urt_hpd_hprt_times_taken[1][1],
                  urt_hpd_hprt_times_taken[2][1]}),
        std::max({urt_hpd_hprt_times_taken[0][2],
                  urt_hpd_hprt_times_taken[1][2],
                  urt_hpd_hprt_times_taken[2][2]}),
    };
    // Run within a block to avoid this function having a reference to
    // the experimenter keeping it alive.
    {
      absl::optional<std::vector<EligibilityConfig>> just_first_config_maybe;
      if (eligibility_configs.has_value()) {
        std::vector<EligibilityConfig> just_first_config;
        just_first_config.push_back(eligibility_configs.value()[0]);
        just_first_config_maybe = just_first_config;
      }
      scoped_refptr<Experimenter> experimenter = SetUpExperimenter(
          will_process_response_time_taken, checker_on_io_self_destruct_time,
          /*eligibility_configs=*/just_first_config_maybe, is_prefetch);
      CreateAndRunChecks(
          experimenter, /*safe_browsing_url_checker_index=*/0,
          urt_hpd_hprt_times_taken[0],
          /*urt_hpd_hprt_threat_types=*/urt_hpd_hprt_threat_types[0],
          base::BindOnce(&SafeBrowsingLookupMechanismExperimenterTest::
                             CallbackForRedirects,
                         base::Unretained(this), experimenter,
                         urt_hpd_hprt_times_taken, urt_hpd_hprt_threat_types,
                         eligibility_configs,
                         /*index=*/1),
          /*immediately_resolve_eligibility=*/
          !eligibility_configs.has_value());
    }
    task_environment_.FastForwardUntilNoTasksRemain();

    auto expected_lookup_had_warning = [urt_hpd_hprt_threat_types,
                                        eligibility_configs](int i, int j) {
      if (eligibility_configs.has_value() &&
          !eligibility_configs.value()[i].eligibility) {
        // Ineligible lookups are automatically safe.
        return false;
      }
      return urt_hpd_hprt_threat_types[i][j] == SB_THREAT_TYPE_URL_PHISHING;
    };
    auto expected_mechanism_had_warning = [expected_lookup_had_warning](int j) {
      return expected_lookup_had_warning(0, j) ||
             expected_lookup_had_warning(1, j) ||
             expected_lookup_had_warning(2, j);
    };
    std::vector<bool> expected_urt_hpd_hprt_had_warnings = {
        expected_mechanism_had_warning(0), expected_mechanism_had_warning(1),
        expected_mechanism_had_warning(2)};
    std::vector<bool> expected_urt_hpd_hprt_time_outs = {
        urt_hpd_hprt_time_outs[0][0] || urt_hpd_hprt_time_outs[1][0] ||
            urt_hpd_hprt_time_outs[2][0],
        urt_hpd_hprt_time_outs[0][1] || urt_hpd_hprt_time_outs[1][1] ||
            urt_hpd_hprt_time_outs[2][1],
        urt_hpd_hprt_time_outs[0][2] || urt_hpd_hprt_time_outs[1][2] ||
            urt_hpd_hprt_time_outs[2][2],
    };
    bool all_eligible = !eligibility_configs.has_value() ||
                        (eligibility_configs.value()[0].eligibility &&
                         eligibility_configs.value()[1].eligibility &&
                         eligibility_configs.value()[2].eligibility);
    bool all_ineligible = eligibility_configs.has_value() &&
                          (!eligibility_configs.value()[0].eligibility &&
                           !eligibility_configs.value()[1].eligibility &&
                           !eligibility_configs.value()[2].eligibility);
    if (all_ineligible || is_prefetch) {
      VerifyNoLogs();
    } else {
      VerifyLogsAllowingRedirects(
          summed_urt_hpd_hprt_times_taken, max_urt_hpd_hprt_times_taken,
          /*expected_urt_hpd_hprt_had_warnings=*/
          expected_urt_hpd_hprt_had_warnings,
          /*expected_had_redirects=*/true,
          /*expected_some_ineligible=*/!all_eligible,
          /*expected_warnings_result=*/
          CombineBoolResults(expected_urt_hpd_hprt_had_warnings[0],
                             expected_urt_hpd_hprt_had_warnings[1],
                             expected_urt_hpd_hprt_had_warnings[2]),
          /*expected_timed_out_result=*/
          CombineBoolResults(expected_urt_hpd_hprt_time_outs[0],
                             expected_urt_hpd_hprt_time_outs[1],
                             expected_urt_hpd_hprt_time_outs[2]),
          will_process_response_time_taken, checker_on_io_self_destruct_time);
    }
  }

  // Should be used to test different combinations of mechanisms resulting in
  // warnings.
  void RunWarningsTest(std::vector<SBThreatType> urt_hpd_hprt_threat_types) {
    base::MockCallback<CompleteCheckCallbackWithTimeout>
        url_real_time_result_callback;
    EXPECT_CALL(url_real_time_result_callback,
                Run(false, Matches(urt_hpd_hprt_threat_types[0])))
        .Times(1);
    auto urt_hpd_hprt_times_taken = {base::Seconds(2), base::Seconds(1),
                                     base::Seconds(3)};
    auto will_process_response_time_taken = base::Seconds(4);
    auto checker_on_io_self_destruct_time = base::Seconds(4.5);
    SetUpExperimenterAndChecks(
        urt_hpd_hprt_times_taken, urt_hpd_hprt_threat_types,
        will_process_response_time_taken, checker_on_io_self_destruct_time,
        url_real_time_result_callback.Get(),
        /*eligibility_configs=*/absl::nullopt, /*is_prefetch=*/false);
    task_environment_.FastForwardUntilNoTasksRemain();
    auto warning_threat_types = {
        SB_THREAT_TYPE_URL_PHISHING, SB_THREAT_TYPE_URL_MALWARE,
        SB_THREAT_TYPE_URL_UNWANTED, SB_THREAT_TYPE_BILLING};
    std::vector<bool> expected_urt_hpd_hprt_had_warnings = {
        base::Contains(warning_threat_types, urt_hpd_hprt_threat_types[0]),
        base::Contains(warning_threat_types, urt_hpd_hprt_threat_types[1]),
        base::Contains(warning_threat_types, urt_hpd_hprt_threat_types[2])};
    VerifyLogs(
        /*expected_urt_hpd_hprt_times_taken=*/urt_hpd_hprt_times_taken,
        expected_urt_hpd_hprt_had_warnings,
        /*expected_warnings_result=*/
        Experimenter::CombineBoolResults(expected_urt_hpd_hprt_had_warnings[0],
                                         expected_urt_hpd_hprt_had_warnings[1],
                                         expected_urt_hpd_hprt_had_warnings[2]),
        /*expected_timed_out_result=*/AllInOneResult::kNoMechanism,
        will_process_response_time_taken, checker_on_io_self_destruct_time);
  }

  // Should be used to test different combinations of mechanisms resulting in
  // timeouts.
  void RunTimeoutTest(std::vector<bool> urt_hpd_hprt_time_out) {
    base::MockCallback<CompleteCheckCallbackWithTimeout>
        url_real_time_result_callback;
    if (urt_hpd_hprt_time_out[0]) {
      EXPECT_CALL(url_real_time_result_callback,
                  Run(true, testing::Eq(absl::nullopt)))
          .Times(1);
    } else {
      EXPECT_CALL(url_real_time_result_callback,
                  Run(false, Matches((SB_THREAT_TYPE_URL_PHISHING))))
          .Times(1);
    }
    auto urt_hpd_hprt_times_taken = {
        urt_hpd_hprt_time_out[0] ? base::Seconds(6) : base::Seconds(2),
        urt_hpd_hprt_time_out[1] ? base::Seconds(6) : base::Seconds(1),
        urt_hpd_hprt_time_out[2] ? base::Seconds(6) : base::Seconds(3)};
    auto will_process_response_time_taken = base::Seconds(4);
    auto checker_on_io_self_destruct_time = base::Seconds(8);
    SetUpExperimenterAndChecks(
        urt_hpd_hprt_times_taken, /*urt_hpd_hprt_threat_types=*/
        {SB_THREAT_TYPE_URL_PHISHING, SB_THREAT_TYPE_URL_PHISHING,
         SB_THREAT_TYPE_URL_PHISHING},
        will_process_response_time_taken, checker_on_io_self_destruct_time,
        url_real_time_result_callback.Get(),
        /*eligibility_configs=*/absl::nullopt, /*is_prefetch=*/false);
    task_environment_.FastForwardUntilNoTasksRemain();
    std::vector<bool> expected_urt_hpd_hprt_had_warnings = {
        !urt_hpd_hprt_time_out[0], !urt_hpd_hprt_time_out[1],
        !urt_hpd_hprt_time_out[2]};
    VerifyLogs(
        /*expected_urt_hpd_hprt_times_taken=*/urt_hpd_hprt_times_taken,
        expected_urt_hpd_hprt_had_warnings,
        /*expected_warnings_result=*/
        Experimenter::CombineBoolResults(expected_urt_hpd_hprt_had_warnings[0],
                                         expected_urt_hpd_hprt_had_warnings[1],
                                         expected_urt_hpd_hprt_had_warnings[2]),
        /*expected_timed_out_result=*/
        Experimenter::CombineBoolResults(urt_hpd_hprt_time_out[0],
                                         urt_hpd_hprt_time_out[1],
                                         urt_hpd_hprt_time_out[2]),
        will_process_response_time_taken, checker_on_io_self_destruct_time);
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<base::HistogramTester> histogram_tester_ =
      std::make_unique<base::HistogramTester>();
  std::unique_ptr<PretendUrlCheckerDelegate> url_checker_delegate_ =
      std::make_unique<PretendUrlCheckerDelegate>();
};

TEST_F(SafeBrowsingLookupMechanismExperimenterTest, TestLifetimes) {
  // Test that all the different permutations of time_taken for the 5 different
  // MaybeCompleteExperiment events succeed.
  // Intentionally include base::Seconds(0) as an option to test synchronous
  // hash-prefix database lookups as well as async ones (see the description
  // above |SetUpExperimenterAndChecks|).
  // This excludes test cases that might cancel the check, which could cause the
  // test to be flaky due to race conditions.
  std::vector<base::TimeDelta> times = {base::Seconds(0), base::Seconds(1),
                                        base::Seconds(2), base::Seconds(3),
                                        base::Seconds(4)};
  do {
    std::vector<base::TimeDelta> urt_hpd_hprt_times_taken = {times[0], times[1],
                                                             times[2]};
    base::TimeDelta will_process_response_time_taken = times[3];
    base::TimeDelta checker_on_io_self_destruct_time = times[4];
    if (urt_hpd_hprt_times_taken[0] < checker_on_io_self_destruct_time) {
      RunLifetimesTest(
          urt_hpd_hprt_times_taken, will_process_response_time_taken,
          checker_on_io_self_destruct_time, /*will_be_canceled=*/false);
      ResetMetrics();
    }
  } while (std::next_permutation(times.begin(), times.end()));
}

TEST_F(SafeBrowsingLookupMechanismExperimenterTest, TestLifetimes_Canceled) {
  std::vector<base::TimeDelta> urt_hpd_hprt_times_taken = {
      base::Seconds(4), base::Seconds(3), base::Seconds(2)};
  base::TimeDelta will_process_response_time_taken = base::Seconds(5);
  base::TimeDelta checker_on_io_self_destruct_time = base::Seconds(0);
  RunLifetimesTest(urt_hpd_hprt_times_taken, will_process_response_time_taken,
                   checker_on_io_self_destruct_time,
                   /*will_be_canceled=*/true);
  ResetMetrics();
}

TEST_F(SafeBrowsingLookupMechanismExperimenterTest, TestWarnings) {
  std::vector<SBThreatType> threat_types = {
      SB_THREAT_TYPE_SAFE,        SB_THREAT_TYPE_URL_PHISHING,
      SB_THREAT_TYPE_URL_MALWARE, SB_THREAT_TYPE_URL_UNWANTED,
      SB_THREAT_TYPE_BILLING,     SB_THREAT_TYPE_SUSPICIOUS_SITE};
  for (SBThreatType urt_threat_type : threat_types) {
    for (SBThreatType hpd_threat_type : threat_types) {
      for (SBThreatType hprt_threat_type : threat_types) {
        RunWarningsTest({urt_threat_type, hpd_threat_type, hprt_threat_type});
        ResetMetrics();
      }
    }
  }
}

TEST_F(SafeBrowsingLookupMechanismExperimenterTest, TestTimeouts) {
  std::vector<bool> options = {false, true};
  for (bool urt_timeout : options) {
    for (bool hpd_timeout : options) {
      for (bool hprt_timeout : options) {
        RunTimeoutTest({urt_timeout, hpd_timeout, hprt_timeout});
        ResetMetrics();
      }
    }
  }
}

TEST_F(SafeBrowsingLookupMechanismExperimenterTest, TestEmptyExperiment) {
  // After one second, calls into mechanism_experimenter's
  // |OnBrowserUrlLoaderThrottleCheckerOnIODestructed|, which ends the
  // experiment.
  new PretendCheckerOnIO(
      /*time_to_will_process_response=*/base::Seconds(3),
      /*time_to_self_destruct=*/base::Seconds(1), /*is_prefetch=*/false);
  task_environment_.FastForwardUntilNoTasksRemain();
  VerifyNoLogs();
}

TEST_F(SafeBrowsingLookupMechanismExperimenterTest, TestCombineBoolResults) {
  struct TestCase {
    bool url_real_time;
    bool hash_database;
    bool hash_real_time;
    AllInOneResult expected_result;
  } test_cases[] = {
      {/*url_real_time=*/false, /*hash_database=*/false,
       /*hash_real_time=*/false,
       /*expected_result=*/AllInOneResult::kNoMechanism},
      {/*url_real_time=*/false, /*hash_database=*/false,
       /*hash_real_time=*/true,
       /*expected_result=*/AllInOneResult::kHashRealTimeOnly},
      {/*url_real_time=*/false, /*hash_database=*/true,
       /*hash_real_time=*/false,
       /*expected_result=*/AllInOneResult::kHashDatabaseOnly},
      {/*url_real_time=*/false, /*hash_database=*/true, /*hash_real_time=*/true,
       /*expected_result=*/AllInOneResult::kHashDatabaseAndHashRealTime},
      {/*url_real_time=*/true, /*hash_database=*/false,
       /*hash_real_time=*/false,
       /*expected_result=*/AllInOneResult::kUrlRealTimeOnly},
      {/*url_real_time=*/true, /*hash_database=*/false, /*hash_real_time=*/true,
       /*expected_result=*/AllInOneResult::kUrlRealTimeAndHashRealTime},
      {/*url_real_time=*/true, /*hash_database=*/true, /*hash_real_time=*/false,
       /*expected_result=*/AllInOneResult::kUrlRealTimeAndHashDatabase},
      {/*url_real_time=*/true, /*hash_database=*/true, /*hash_real_time=*/true,
       /*expected_result=*/AllInOneResult::kAllMechanisms},
  };
  for (const auto& test_case : test_cases) {
    EXPECT_EQ(
        CombineBoolResults(test_case.url_real_time, test_case.hash_database,
                           test_case.hash_real_time),
        test_case.expected_result);
  }
}

TEST_F(SafeBrowsingLookupMechanismExperimenterTest, TestRedirectLifetimes) {
  auto get_initial_times = []() {
    std::vector<base::TimeDelta> times = {base::Seconds(0), base::Seconds(1),
                                          base::Seconds(2)};
    return times;
  };
  auto get_urt_hpd_hprt_times_taken = [](std::vector<base::TimeDelta> times) {
    // The times have small increases on them to avoid ties between mechanisms
    // for the total amount of time taken.
    std::vector<base::TimeDelta> urt_hpd_hprt_times_taken = {
        times[0] + base::Seconds(0.2), times[1], times[2] + base::Seconds(0.1)};
    return urt_hpd_hprt_times_taken;
  };
  // Each test case kicks off a lookup with two subsequent redirects. The test
  // cases consist of all permutations of time_taken for the 3 main mechanisms,
  // with the WillProcessResponse and CheckerOnIO destruct times interspersed.
  auto times = get_initial_times();
  do {
    auto times2 = get_initial_times();
    do {
      auto times3 = get_initial_times();
      do {
        auto urt_hpd_hprt_times_taken = get_urt_hpd_hprt_times_taken(times);
        auto urt_hpd_hprt_times_taken2 = get_urt_hpd_hprt_times_taken(times2);
        auto urt_hpd_hprt_times_taken3 = get_urt_hpd_hprt_times_taken(times3);
        auto urt_first_check_end_time = urt_hpd_hprt_times_taken[0];
        auto urt_last_check_begin_time =
            urt_first_check_end_time + urt_hpd_hprt_times_taken2[0];
        auto urt_last_check_end_time =
            urt_last_check_begin_time + urt_hpd_hprt_times_taken3[0];
        // WillProcessResponse after 2nd URT check
        {
          base::TimeDelta will_process_response_time_taken =
              urt_last_check_begin_time + base::Seconds(0.5);
          base::TimeDelta checker_on_io_self_destruct_time = base::Seconds(15);
          RunLifetimesWithTwoRedirectsTest(
              {urt_hpd_hprt_times_taken, urt_hpd_hprt_times_taken2,
               urt_hpd_hprt_times_taken3},
              will_process_response_time_taken,
              checker_on_io_self_destruct_time);
          ResetMetrics();
        }
        // WillProcessResponse after 3rd URT check
        {
          base::TimeDelta will_process_response_time_taken =
              urt_last_check_end_time + base::Seconds(0.5);
          base::TimeDelta checker_on_io_self_destruct_time = base::Seconds(15);
          RunLifetimesWithTwoRedirectsTest(
              {urt_hpd_hprt_times_taken, urt_hpd_hprt_times_taken2,
               urt_hpd_hprt_times_taken3},
              will_process_response_time_taken,
              checker_on_io_self_destruct_time);
          ResetMetrics();
        }
        // WillProcessResponse after all checks done
        {
          base::TimeDelta will_process_response_time_taken = base::Seconds(10);
          base::TimeDelta checker_on_io_self_destruct_time = base::Seconds(15);
          RunLifetimesWithTwoRedirectsTest(
              {urt_hpd_hprt_times_taken, urt_hpd_hprt_times_taken2,
               urt_hpd_hprt_times_taken3},
              will_process_response_time_taken,
              checker_on_io_self_destruct_time);
          ResetMetrics();
        }
        // CheckerOnIO destruct before WillProcessResponse has completed
        {
          base::TimeDelta will_process_response_time_taken = base::Seconds(15);
          base::TimeDelta checker_on_io_self_destruct_time =
              urt_last_check_end_time + base::Seconds(0.5);
          RunLifetimesWithTwoRedirectsTest(
              {urt_hpd_hprt_times_taken, urt_hpd_hprt_times_taken2,
               urt_hpd_hprt_times_taken3},
              will_process_response_time_taken,
              checker_on_io_self_destruct_time);
          ResetMetrics();
        }
      } while (std::next_permutation(times3.begin(), times3.end()));
    } while (std::next_permutation(times2.begin(), times2.end()));
  } while (std::next_permutation(times.begin(), times.end()));
}

TEST_F(SafeBrowsingLookupMechanismExperimenterTest, TestRedirectWarnings) {
  auto safe = SB_THREAT_TYPE_SAFE;
  auto unsafe = SB_THREAT_TYPE_URL_PHISHING;
  std::vector<std::vector<std::vector<SBThreatType>>> test_cases = {
      // Each mechanism returns unsafe once (different lookups).
      {{safe, safe, unsafe}, {safe, unsafe, safe}, {unsafe, safe, safe}},
      // Each mechanism returns unsafe once (same lookup).
      {{safe, safe, safe}, {safe, safe, safe}, {unsafe, unsafe, unsafe}},
      // Only URL real-time is unsafe.
      {{safe, safe, safe}, {safe, safe, safe}, {unsafe, safe, safe}},
      // Only hash-prefix database is unsafe.
      {{safe, safe, safe}, {safe, safe, safe}, {safe, unsafe, safe}},
      // Only hash-prefix real-time is unsafe.
      {{safe, safe, safe}, {safe, safe, safe}, {safe, safe, unsafe}},
      // Mechanisms return unsafe multiple times.
      {{safe, unsafe, unsafe}, {safe, unsafe, safe}, {unsafe, safe, unsafe}}};
  for (const auto& test_case : test_cases) {
    RunWarningsWithTwoRedirectsTest(test_case);
    ResetMetrics();
  }
}

TEST_F(SafeBrowsingLookupMechanismExperimenterTest, TestRedirectTimeouts) {
  std::vector<std::vector<std::vector<bool>>> test_cases = {
      // Each mechanism times out once (different lookups).
      {{false, false, true}, {false, true, false}, {true, false, false}},
      // Each mechanism times out once (same lookup).
      {{false, false, false}, {false, false, false}, {true, true, true}},
      // Only URL real-time times out.
      {{false, false, false}, {false, false, false}, {true, false, false}},
      // Only hash-prefix database times out.
      {{false, false, false}, {false, false, false}, {false, true, false}},
      // Only hash-prefix real-time times out.
      {{false, false, false}, {false, false, false}, {false, false, true}},
      // Mechanisms time out multiple times.
      {{false, true, true}, {false, true, false}, {true, false, true}}};
  for (const auto& test_case : test_cases) {
    RunTimeoutWithTwoRedirectsTest(test_case);
    ResetMetrics();
  }
}

TEST_F(SafeBrowsingLookupMechanismExperimenterTest, TestEligibility) {
  auto run_basic_test = [this](base::TimeDelta eligibility_resolution_time,
                               bool eligibility, bool is_prefetch,
                               bool expect_logs) {
    std::vector<EligibilityConfig> eligibility_configs = {EligibilityConfig(
        /*safe_browsing_url_checker_index=*/0, eligibility_resolution_time,
        eligibility)};
    RunEligibilityTest(eligibility_configs, is_prefetch, expect_logs);
    ResetMetrics();
  };

  // Is eligible and eligibility finishes last. Should have logs.
  {
    run_basic_test(base::Seconds(50), /*eligibility=*/true,
                   /*is_prefetch=*/false, /*expect_logs=*/true);
  }
  // Is not eligible and eligibility finishes last. Should not have logs.
  {
    run_basic_test(base::Seconds(50), /*eligibility=*/false,
                   /*is_prefetch=*/false, /*expect_logs=*/false);
  }
  // Is eligible and eligibility does not finish last. Should have logs.
  {
    run_basic_test(base::Seconds(5), /*eligibility=*/true,
                   /*is_prefetch=*/false, /*expect_logs=*/true);
  }
  // Is not eligible and eligibility does not finish last. Should not have logs.
  {
    run_basic_test(base::Seconds(5), /*eligibility=*/false,
                   /*is_prefetch=*/false, /*expect_logs=*/false);
  }
  // Is not eligible due to prefetch. Should not have logs.
  {
    run_basic_test(base::Seconds(50), /*eligibility=*/true,
                   /*is_prefetch=*/true, /*expect_logs=*/false);
  }
  // When eligibility finishes last, a 2nd call to defining it does not crash.
  {
    std::vector<EligibilityConfig> eligibility_configs = {
        EligibilityConfig(
            /*safe_browsing_url_checker_index=*/0, base::Seconds(50), true),
        EligibilityConfig(
            /*safe_browsing_url_checker_index=*/0, base::Seconds(60), true)};
    RunEligibilityTest(eligibility_configs, /*is_prefetch=*/false,
                       /*expect_logs=*/true);
    ResetMetrics();
  }
  // When eligibility doesn't finish last, a 2nd call to defining it does not
  // crash and does not replace the first value.
  {
    std::vector<EligibilityConfig> eligibility_configs = {
        EligibilityConfig(
            /*safe_browsing_url_checker_index=*/0, base::Seconds(5), true),
        EligibilityConfig(
            /*safe_browsing_url_checker_index=*/0, base::Seconds(7), false)};
    RunEligibilityTest(eligibility_configs, /*is_prefetch=*/false,
                       /*expect_logs=*/true);
    ResetMetrics();
  }
}

TEST_F(SafeBrowsingLookupMechanismExperimenterTest, TestRedirectEligibility) {
  std::vector<SBThreatType> all_safe = {
      SB_THREAT_TYPE_SAFE, SB_THREAT_TYPE_SAFE, SB_THREAT_TYPE_SAFE};
  std::vector<SBThreatType> all_unsafe = {SB_THREAT_TYPE_URL_PHISHING,
                                          SB_THREAT_TYPE_URL_PHISHING,
                                          SB_THREAT_TYPE_URL_PHISHING};
  std::vector<std::vector<SBThreatType>> safe_threat_types = {
      all_safe, all_safe, all_safe};
  // All eligible. Resolved in the same order as the checks were run.
  {
    std::vector<EligibilityConfig> eligibility_configs = {
        EligibilityConfig(
            /*safe_browsing_url_checker_index=*/0,
            /*time_to_resolution=*/base::Seconds(10), /*eligibility=*/true),
        EligibilityConfig(
            /*safe_browsing_url_checker_index=*/1,
            /*time_to_resolution=*/base::Seconds(20), /*eligibility=*/true),
        EligibilityConfig(
            /*safe_browsing_url_checker_index=*/2,
            /*time_to_resolution=*/base::Seconds(30), /*eligibility=*/true)};
    RunEligibilityWithTwoRedirectsTest(eligibility_configs, safe_threat_types,
                                       /*is_prefetch=*/false);
    ResetMetrics();
  }
  // All eligible. Resolved in different order than the checks were run.
  {
    std::vector<EligibilityConfig> eligibility_configs = {
        EligibilityConfig(
            /*safe_browsing_url_checker_index=*/0,
            /*time_to_resolution=*/base::Seconds(20), /*eligibility=*/true),
        EligibilityConfig(
            /*safe_browsing_url_checker_index=*/1,
            /*time_to_resolution=*/base::Seconds(30), /*eligibility=*/true),
        EligibilityConfig(
            /*safe_browsing_url_checker_index=*/2,
            /*time_to_resolution=*/base::Seconds(10), /*eligibility=*/true)};
    RunEligibilityWithTwoRedirectsTest(eligibility_configs, safe_threat_types,
                                       /*is_prefetch=*/false);
    ResetMetrics();
  }
  // None eligible. Resolved in the same order as the checks were run.
  {
    std::vector<EligibilityConfig> eligibility_configs = {
        EligibilityConfig(
            /*safe_browsing_url_checker_index=*/0,
            /*time_to_resolution=*/base::Seconds(10), /*eligibility=*/false),
        EligibilityConfig(
            /*safe_browsing_url_checker_index=*/1,
            /*time_to_resolution=*/base::Seconds(20), /*eligibility=*/false),
        EligibilityConfig(
            /*safe_browsing_url_checker_index=*/2,
            /*time_to_resolution=*/base::Seconds(30),
            /*eligibility=*/false)};
    RunEligibilityWithTwoRedirectsTest(eligibility_configs, safe_threat_types,
                                       /*is_prefetch=*/false);
    ResetMetrics();
  }
  // None eligible. Resolved in different order than the checks were run.
  {
    std::vector<EligibilityConfig> eligibility_configs = {
        EligibilityConfig(
            /*safe_browsing_url_checker_index=*/0,
            /*time_to_resolution=*/base::Seconds(20), /*eligibility=*/false),
        EligibilityConfig(
            /*safe_browsing_url_checker_index=*/1,
            /*time_to_resolution=*/base::Seconds(30), /*eligibility=*/false),
        EligibilityConfig(
            /*safe_browsing_url_checker_index=*/2,
            /*time_to_resolution=*/base::Seconds(10),
            /*eligibility=*/false)};
    RunEligibilityWithTwoRedirectsTest(eligibility_configs, safe_threat_types,
                                       /*is_prefetch=*/false);
    ResetMetrics();
  }
  // Only first and last eligible. Middle one being unsafe should not affect the
  // results.
  {
    std::vector<EligibilityConfig> eligibility_configs = {
        EligibilityConfig(
            /*safe_browsing_url_checker_index=*/0,
            /*time_to_resolution=*/base::Seconds(20), /*eligibility=*/true),
        EligibilityConfig(
            /*safe_browsing_url_checker_index=*/1,
            /*time_to_resolution=*/base::Seconds(30), /*eligibility=*/false),
        EligibilityConfig(
            /*safe_browsing_url_checker_index=*/2,
            /*time_to_resolution=*/base::Seconds(10),
            /*eligibility=*/true)};
    RunEligibilityWithTwoRedirectsTest(eligibility_configs,
                                       {all_safe, all_unsafe, all_safe},
                                       /*is_prefetch=*/false);
    ResetMetrics();
  }
  // Only first and last eligible. Middle one and last being unsafe should say
  // it's unsafe.
  {
    std::vector<EligibilityConfig> eligibility_configs = {
        EligibilityConfig(
            /*safe_browsing_url_checker_index=*/0,
            /*time_to_resolution=*/base::Seconds(20), /*eligibility=*/true),
        EligibilityConfig(
            /*safe_browsing_url_checker_index=*/1,
            /*time_to_resolution=*/base::Seconds(30), /*eligibility=*/false),
        EligibilityConfig(
            /*safe_browsing_url_checker_index=*/2,
            /*time_to_resolution=*/base::Seconds(10),
            /*eligibility=*/true)};
    RunEligibilityWithTwoRedirectsTest(eligibility_configs,
                                       {all_safe, all_unsafe, all_unsafe},
                                       /*is_prefetch=*/false);
    ResetMetrics();
  }
  // All eligible but is prefetch, so overall no logs.
  {
    std::vector<EligibilityConfig> eligibility_configs = {
        EligibilityConfig(
            /*safe_browsing_url_checker_index=*/0,
            /*time_to_resolution=*/base::Seconds(10), /*eligibility=*/true),
        EligibilityConfig(
            /*safe_browsing_url_checker_index=*/1,
            /*time_to_resolution=*/base::Seconds(20), /*eligibility=*/true),
        EligibilityConfig(
            /*safe_browsing_url_checker_index=*/2,
            /*time_to_resolution=*/base::Seconds(30), /*eligibility=*/true)};
    RunEligibilityWithTwoRedirectsTest(eligibility_configs, safe_threat_types,
                                       /*is_prefetch=*/true);
    ResetMetrics();
  }
}
}  // namespace safe_browsing
