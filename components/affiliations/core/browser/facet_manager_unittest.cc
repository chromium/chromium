// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/affiliations/core/browser/facet_manager.h"

#include <stddef.h>

#include <algorithm>
#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/rand_util.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "components/affiliations/core/browser/facet_manager_host.h"
#include "components/affiliations/core/browser/mock_affiliation_consumer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace affiliations {

// Helpers --------------------------------------------------------------------

namespace {

using StrategyOnCacheMiss = FacetManager::StrategyOnCacheMiss;
enum class NotificationAccuracy { PERFECT, TOO_LATE, TOO_EARLY, NEVER_CALLED };

// Helper class to post callbacks to FacetManager::NotifyAtRequestedTime(),
// delayed by the requested time plus/minus a configurable error term to
// simulate a real-life task runner.
class TestFacetManagerNotifier {
 public:
  TestFacetManagerNotifier(
      scoped_refptr<base::TestMockTimeTaskRunner> task_runner,
      base::TimeDelta too_late_delay)
      : accuracy_(NotificationAccuracy::PERFECT),
        too_late_delay_(too_late_delay),
        task_runner_(task_runner),
        facet_manager_(nullptr) {}

  TestFacetManagerNotifier(const TestFacetManagerNotifier&) = delete;
  TestFacetManagerNotifier& operator=(const TestFacetManagerNotifier&) = delete;

  void Notify(base::Time time) {
    base::TimeDelta delay = time - task_runner_->Now();
    if (accuracy_ == NotificationAccuracy::TOO_LATE) {
      delay += too_late_delay_;
    } else if (accuracy_ == NotificationAccuracy::TOO_EARLY) {
      // This formula is a simple stateless solution for notifying FacetManagers
      // prematurely multiple times in a row while also ensuring that the tests
      // are still fast, with no more than log2(delay.InSeconds()) repetitions.
      delay = std::min(delay, delay / 2 + base::Seconds(1));
    } else if (accuracy_ == NotificationAccuracy::NEVER_CALLED) {
      return;
    }
    task_runner_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&FacetManager::NotifyAtRequestedTime,
                       base::Unretained(facet_manager_)),
        delay);
  }

  void set_accuracy(NotificationAccuracy accuracy) { accuracy_ = accuracy; }
  void set_facet_manager(FacetManager* facet_manager_under_test) {
    facet_manager_ = facet_manager_under_test;
  }

 private:
  NotificationAccuracy accuracy_;
  const base::TimeDelta too_late_delay_;
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  raw_ptr<FacetManager> facet_manager_;
};

// Stub/mock implementation for FacetManagerHost.
class MockFacetManagerHost : public FacetManagerHost {
 public:
  explicit MockFacetManagerHost(TestFacetManagerNotifier* notifier)
      : notifier_(notifier) {}

  MockFacetManagerHost(const MockFacetManagerHost&) = delete;
  MockFacetManagerHost& operator=(const MockFacetManagerHost&) = delete;

  // Sets the |facet_uri| that will be expected to appear in calls coming from
  // the FacetManager under test.
  void set_expected_facet_uri(const FacetURI& facet_uri) {
    expected_facet_uri_ = facet_uri;
  }

  // Returns the facet URI that will be expected to appear in calls coming from
  // the FacetManager under test.
  const FacetURI& expected_facet_uri() const { return expected_facet_uri_; }

  // Sets up fake |database_content| as the canned response to be returned to
  // the FacetManager every time it calls
  // ReadAffiliationsAndBrandingFromDatabase().
  void set_fake_database_content(
      const AffiliatedFacetsWithUpdateTime& database_content) {
    fake_database_content_ = database_content;
  }

  void clear_fake_database_content() {
    fake_database_content_.last_update_time = base::Time();
  }

  // Returns whether SignalNeedNetworkRequest() has been called at least once.
  size_t signaled_need_network_request() const {
    return signaled_need_network_request_;
  }

  void reset_need_network_request() { signaled_need_network_request_ = false; }

 private:
  // FacetManagerHost:
  bool ReadAffiliationsAndBrandingFromDatabase(
      const FacetURI& facet_uri,
      AffiliatedFacetsWithUpdateTime* affiliations) override {
    EXPECT_EQ(expected_facet_uri_, facet_uri);
    if (fake_database_content_.last_update_time.is_null())
      return false;
    *affiliations = fake_database_content_;
    return true;
  }

  void SignalNeedNetworkRequest() override {
    signaled_need_network_request_ = true;
  }

  void RequestNotificationAtTime(const FacetURI& facet_uri,
                                 base::Time time) override {
    EXPECT_EQ(expected_facet_uri_, facet_uri);
    // The absolute timing of notification requests is not all that interesting,
    // only the ability to perturb it slightly, which is done by the notifier.
    notifier_->Notify(time);
  }

  raw_ptr<TestFacetManagerNotifier> notifier_;

  FacetURI expected_facet_uri_;
  AffiliatedFacetsWithUpdateTime fake_database_content_;
  bool signaled_need_network_request_ = false;
};

const bool kFalseTrue[] = {false, true};

const char kTestFacetURI1[] = "https://one.example.com";
const char kTestFacetURI2[] = "https://two.example.com";
const char kTestFacetURI3[] = "https://three.example.com";

AffiliatedFacets GetTestEquivalenceClass() {
  return {
      Facet(FacetURI::FromCanonicalSpec(kTestFacetURI1)),
      Facet(FacetURI::FromCanonicalSpec(kTestFacetURI2)),
      Facet(FacetURI::FromCanonicalSpec(kTestFacetURI3)),
  };
}

AffiliatedFacetsWithUpdateTime GetTestEquivalenceClassWithUpdateTime(
    base::Time last_update_time) {
  AffiliatedFacetsWithUpdateTime affiliation;
  affiliation.last_update_time = last_update_time;
  affiliation.facets = GetTestEquivalenceClass();
  return affiliation;
}

base::TimeDelta GetCacheHardExpiryPeriod() {
  return base::Hours(FacetManager::kCacheHardExpiryInHours);
}

base::TimeDelta GetCacheSoftExpiryPeriod() {
  return base::Hours(FacetManager::kCacheSoftExpiryInHours);
}

base::TimeDelta GetShortTestPeriod() {
  return base::Hours(1);
}

// Returns a smallest time difference that this test cares about.
base::TimeDelta Epsilon() {
  return base::Microseconds(1);
}

// Returns |time| + |delay| or the maximum time if |delay| is the maximum delta.
base::Time SafeAdd(base::Time time, base::TimeDelta delay) {
  if (delay == base::TimeDelta::Max())
    return base::Time::Max();
  return time + delay;
}

// Subdivides a time interval of a given |duration| into zero or more intervals
// that lend themselves to be used for sampling a quantity every that often.
// More specifically, returns the minimum number of intervals so that each
// sub-interval is at most GetTestShortInterval() long, and that the last of the
// sub-intervals (if any) is exactly Epsilon() long. No intervals are returned
// if |duration| is of length zero.
std::vector<base::TimeDelta> SamplingPoints(base::TimeDelta duration) {
  std::vector<base::TimeDelta> deltas;
  if (duration.is_positive()) {
    while (duration > Epsilon()) {
      deltas.push_back(std::min(GetShortTestPeriod(), duration - Epsilon()));
      duration -= deltas.back();
    }
    deltas.push_back(Epsilon());
  }
  return deltas;
}

}  // namespace

// Test framework -------------------------------------------------------------

class FacetManagerTest : public testing::Test {
 public:
  FacetManagerTest()
      : consumer_task_runner_(new base::TestSimpleTaskRunner),
        main_task_runner_(new base::TestMockTimeTaskRunner),
        facet_manager_notifier_(main_task_runner_, GetShortTestPeriod()),
        facet_manager_host_(&facet_manager_notifier_) {}

  FacetManagerTest(const FacetManagerTest&) = delete;
  FacetManagerTest& operator=(const FacetManagerTest&) = delete;

 protected:
  struct ExpectedFetchDetails {
    // The expected time of the fetch being triggered.
    base::Time time;

    // A simulated delay after which the fetch will be completed.
    // base::TimeDelta::Max() means that the fetch will be left hanging.
    base::TimeDelta completion_delay;
  };

  void CreateFacetManager() {
    // The order is important: FacetManager will read the DB in its constructor.
    facet_manager_host_.set_expected_facet_uri(
        FacetURI::FromCanonicalSpec(kTestFacetURI1));
    facet_manager_ = std::make_unique<FacetManager>(
        FacetURI::FromCanonicalSpec(kTestFacetURI1), fake_facet_manager_host(),
        main_task_runner_->GetMockClock());
    facet_manager_notifier_.set_facet_manager(facet_manager_.get());
    facet_manager_creation_ = Now();
  }

  void DestroyFacetManager() {
    main_task_runner_->ClearPendingTasks();
    facet_manager_host_.set_expected_facet_uri(FacetURI());
    facet_manager_notifier_.set_facet_manager(nullptr);
    facet_manager_.reset();
  }

  void AdvanceTime(base::TimeDelta delta) {
    main_task_runner_->FastForwardBy(delta);
  }

  base::Time Now() { return main_task_runner_->Now(); }

  // Returns the elapsed time since CreateFacetManager() was last called.
  base::TimeDelta DeltaNow() { return Now() - facet_manager_creation_; }

  void GetAffiliationsAndBranding(StrategyOnCacheMiss cache_miss_strategy) {
    facet_manager()->GetAffiliationsAndBranding(
        cache_miss_strategy, mock_consumer()->GetResultCallback(),
        consumer_task_runner());
  }

  void Prefetch(base::Time until) { facet_manager()->Prefetch(until); }
  void CancelPrefetch(base::Time until) {
    facet_manager()->CancelPrefetch(until);
  }

  void SchedulePrefetch(base::Time start, base::Time end) {
    main_task_runner_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&FacetManager::Prefetch,
                       base::Unretained(facet_manager()), end),
        start - Now());
  }

  void ScheduleCancelPrefetch(base::Time cancellation_time,
                              base::Time original_end_of_prefetch) {
    main_task_runner_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&FacetManager::CancelPrefetch,
                       base::Unretained(facet_manager()),
                       original_end_of_prefetch),
        cancellation_time - Now());
  }

  // Advances time |until_time|, and verifies that a Prefetch() request has the
  // expected effects now and then (but not at |until_time|), namely that:
  //   * the FacetManager cannot be discarded, and
  //   * requests are served from the cache;
  // and that no fetches are triggered in this interval.
  void AdvanceTimeAndVerifyPrefetch(base::Time until_time) {
    for (base::TimeDelta step : SamplingPoints(until_time - Now())) {
      SCOPED_TRACE(testing::Message() << "dT: " << DeltaNow());
      EXPECT_FALSE(facet_manager()->CanBeDiscarded());
      EXPECT_FALSE(facet_manager()->CanCachedDataBeDiscarded());
      ExpectRequestsServedFromCache();
      ExpectNoFetchNeeded();
      AdvanceTime(step);
    }
  }

  // Advances time |until_time|, and verifies that between now and then, the
  // FacetManager cannot be discarded, and a fetch is needed all the time.
  void AdvanceTimeAndExpectFetchNeeded(base::Time until_time) {
    for (base::TimeDelta step : SamplingPoints(until_time - Now())) {
      SCOPED_TRACE(testing::Message() << "dT: " << DeltaNow());
      EXPECT_FALSE(facet_manager()->CanBeDiscarded());
      ASSERT_NO_FATAL_FAILURE(ExpectFetchNeeded());
      AdvanceTime(step);
    }
  }

  // Advances time |until_time|, and verifies that a Prefetch() request has the
  // expected effects between now and then (but not at |until_time|), namely:
  //   * the FacetManager cannot be discarded, and
  //   * requests are served from the cache;
  // and that no fetches are made exactly as prescribed in |expected_fetches|.
  void AdvanceTimeAndVerifyPrefetchWithFetchesAt(
      base::Time until_time,
      const std::vector<ExpectedFetchDetails>& expected_fetches) {
    for (const auto& next_fetch : expected_fetches) {
      AdvanceTimeAndVerifyPrefetch(next_fetch.time);
      ASSERT_NO_FATAL_FAILURE(AdvanceTimeAndExpectFetchNeeded(
          std::min(until_time, SafeAdd(Now(), next_fetch.completion_delay))));
      if (next_fetch.completion_delay < base::TimeDelta::Max())
        CompleteFetch();
    }
    AdvanceTimeAndVerifyPrefetch(until_time);
  }

  void ExpectFetchNeeded() {
    ASSERT_TRUE(facet_manager()->DoesRequireFetch());
    ASSERT_TRUE(fake_facet_manager_host()->signaled_need_network_request());
  }

  void ExpectNoFetchNeeded() {
    ASSERT_FALSE(facet_manager()->DoesRequireFetch());
    ASSERT_FALSE(fake_facet_manager_host()->signaled_need_network_request());
  }

  void CompleteFetch() {
    AffiliatedFacetsWithUpdateTime fetch_result(
        GetTestEquivalenceClassWithUpdateTime(Now()));
    fake_facet_manager_host()->set_fake_database_content(fetch_result);
    fake_facet_manager_host()->reset_need_network_request();
    facet_manager()->OnFetchSucceeded(fetch_result);

    main_task_runner_->RunUntilIdle();
    ASSERT_NO_FATAL_FAILURE(ExpectNoFetchNeeded());
    ASSERT_TRUE(facet_manager()->IsCachedDataFresh());
  }

  void ExpectConsumerSuccessCallback() {
    const auto equivalence_class(GetTestEquivalenceClass());
    mock_consumer()->ExpectSuccessWithResult(equivalence_class);
    EXPECT_THAT(
        equivalence_class,
        testing::Contains(testing::Field(
            &Facet::uri, fake_facet_manager_host()->expected_facet_uri())));

    consumer_task_runner()->RunUntilIdle();
    testing::Mock::VerifyAndClearExpectations(mock_consumer());
  }

  void ExpectConsumerFailureCallback() {
    mock_consumer()->ExpectFailure();
    consumer_task_runner()->RunUntilIdle();
    testing::Mock::VerifyAndClearExpectations(mock_consumer());
  }

  void ExpectRequestsServedFromCache() {
    EXPECT_TRUE(facet_manager()->IsCachedDataFresh());
    GetAffiliationsAndBranding(StrategyOnCacheMiss::FAIL);
    ExpectConsumerSuccessCallback();
  }

  MockAffiliationConsumer* mock_consumer() { return &mock_consumer_; }

  base::TestSimpleTaskRunner* consumer_task_runner() {
    return consumer_task_runner_.get();
  }

  base::TestMockTimeTaskRunner* main_task_runner() {
    return main_task_runner_.get();
  }

  MockFacetManagerHost* fake_facet_manager_host() {
    return &facet_manager_host_;
  }

  TestFacetManagerNotifier* facet_manager_notifier() {
    return &facet_manager_notifier_;
  }

  FacetManager* facet_manager() { return facet_manager_.get(); }

 private:
  // testing::Test:
  void SetUp() override {
    ASSERT_LT(3 * GetShortTestPeriod(), GetCacheSoftExpiryPeriod());
    ASSERT_LT(GetShortTestPeriod(),
              GetCacheHardExpiryPeriod() - GetCacheSoftExpiryPeriod());
  }

  void TearDown() override { DestroyFacetManager(); }

  MockAffiliationConsumer mock_consumer_;
  scoped_refptr<base::TestSimpleTaskRunner> consumer_task_runner_;
  scoped_refptr<base::TestMockTimeTaskRunner> main_task_runner_;
  TestFacetManagerNotifier facet_manager_notifier_;
  MockFacetManagerHost facet_manager_host_;

  std::unique_ptr<FacetManager> facet_manager_;
  base::Time facet_manager_creation_;
};

// Tests ----------------------------------------------------------------------

TEST_F(FacetManagerTest, NewInstanceCanBeDiscarded) {
  CreateFacetManager();
  EXPECT_TRUE(facet_manager()->CanBeDiscarded());
  EXPECT_TRUE(facet_manager()->CanCachedDataBeDiscarded());
  EXPECT_FALSE(facet_manager()->DoesRequireFetch());
  EXPECT_FALSE(main_task_runner()->HasPendingTask());
}

// Both cached-only and on-demand GetAffiliationsAndBranding() requests should
// be served from cache if it contains fresh data. Nothing should happen on
// cache expiry.
TEST_F(FacetManagerTest, GetAffiliationsAndBrandingServedFromCache) {
  fake_facet_manager_host()->set_fake_database_content(
      GetTestEquivalenceClassWithUpdateTime(Now()));
  AdvanceTime(GetCacheHardExpiryPeriod() - Epsilon());

  CreateFacetManager();
  EXPECT_TRUE(facet_manager()->IsCachedDataFresh());

  GetAffiliationsAndBranding(StrategyOnCacheMiss::FAIL);
  ExpectConsumerSuccessCallback();
  EXPECT_TRUE(facet_manager()->CanBeDiscarded());
  ASSERT_NO_FATAL_FAILURE(ExpectNoFetchNeeded());

  GetAffiliationsAndBranding(StrategyOnCacheMiss::FETCH_OVER_NETWORK);
  ExpectConsumerSuccessCallback();
  EXPECT_TRUE(facet_manager()->CanBeDiscarded());
  ASSERT_NO_FATAL_FAILURE(ExpectNoFetchNeeded());

  AdvanceTime(Epsilon());

  EXPECT_FALSE(facet_manager()->IsCachedDataFresh());
  EXPECT_TRUE(facet_manager()->CanBeDiscarded());
  ASSERT_NO_FATAL_FAILURE(ExpectNoFetchNeeded());
  EXPECT_FALSE(main_task_runner()->HasPendingTask());
}

// On-demand GetAffiliationsAndBranding() requests should trigger a fetch if the
// cache has already stale data, or no corresponding data whatsoever. Nothing
// should happen once the newly fetched data expires.
TEST_F(FacetManagerTest,
       OnDemandGetAffiliationsAndBrandingRequestTriggersFetch) {
  for (const bool cache_initially_has_stale_data : kFalseTrue) {
    SCOPED_TRACE(cache_initially_has_stale_data);

    if (cache_initially_has_stale_data) {
      fake_facet_manager_host()->set_fake_database_content(
          GetTestEquivalenceClassWithUpdateTime(Now()));
      AdvanceTime(GetCacheHardExpiryPeriod());
    } else {
      fake_facet_manager_host()->clear_fake_database_content();
    }

    CreateFacetManager();
    EXPECT_FALSE(facet_manager()->IsCachedDataFresh());

    GetAffiliationsAndBranding(StrategyOnCacheMiss::FETCH_OVER_NETWORK);
    ASSERT_NO_FATAL_FAILURE(ExpectFetchNeeded());
    EXPECT_FALSE(facet_manager()->CanBeDiscarded());
    ASSERT_NO_FATAL_FAILURE(CompleteFetch());
    ExpectConsumerSuccessCallback();

    AdvanceTime(GetCacheHardExpiryPeriod() - Epsilon());
    EXPECT_TRUE(facet_manager()->IsCachedDataFresh());

    GetAffiliationsAndBranding(StrategyOnCacheMiss::FAIL);
    ExpectConsumerSuccessCallback();
    EXPECT_TRUE(facet_manager()->CanBeDiscarded());
    ASSERT_NO_FATAL_FAILURE(ExpectNoFetchNeeded());

    GetAffiliationsAndBranding(StrategyOnCacheMiss::FETCH_OVER_NETWORK);
    ExpectConsumerSuccessCallback();
    EXPECT_TRUE(facet_manager()->CanBeDiscarded());
    ASSERT_NO_FATAL_FAILURE(ExpectNoFetchNeeded());

    AdvanceTime(Epsilon());

    ASSERT_NO_FATAL_FAILURE(ExpectNoFetchNeeded());
    EXPECT_FALSE(facet_manager()->IsCachedDataFresh());
    EXPECT_TRUE(facet_manager()->CanBeDiscarded());
    EXPECT_FALSE(main_task_runner()->HasPendingTask());
    DestroyFacetManager();
  }
}

TEST_F(FacetManagerTest,
       CachedOnlyGetAffiliationsAndBrandingFailsDueToStaleCache) {
  CreateFacetManager();
  EXPECT_FALSE(facet_manager()->IsCachedDataFresh());

  GetAffiliationsAndBranding(StrategyOnCacheMiss::FAIL);
  ExpectConsumerFailureCallback();
  ASSERT_NO_FATAL_FAILURE(ExpectNoFetchNeeded());
  EXPECT_TRUE(facet_manager()->CanBeDiscarded());
  EXPECT_FALSE(main_task_runner()->HasPendingTask());
}

TEST_F(FacetManagerTest,
       GetAffiliationsAndBrandingFailureCallbackInvokedOnDestruction) {
  CreateFacetManager();
  EXPECT_FALSE(facet_manager()->IsCachedDataFresh());

  GetAffiliationsAndBranding(StrategyOnCacheMiss::FETCH_OVER_NETWORK);
  ASSERT_NO_FATAL_FAILURE(ExpectFetchNeeded());
  EXPECT_FALSE(facet_manager()->CanBeDiscarded());

  // Leave the fetch hanging and destroy the facet manager.
  DestroyFacetManager();

  ExpectConsumerFailureCallback();
  fake_facet_manager_host()->reset_need_network_request();
}

// The following tests verify both typical and edge case behavior of Prefetch()
// requests: they should prevent the FacetManager from being discarded, and keep
// the data fresh by initial fetches and refetches (scheduled as described in
// facet_manager.cc).
//
// Legend:
//   [---): Interval representing a finite Prefetch request (open from right).
//          The data should be kept fresh, the FacetManager not discarded.
//   [--->: Interval representing a indefinite Prefetch request.
//          The data should be kept fresh, the FacetManager not discarded.
//   F:     Fetch (initial or refetch) should take place here.
//   Fn:    The time of the n-th fetch (starting from 1).
//   D:     Time interval equal to GetShortTestPeriod().
//   N:     Fetch is signaled to be needed here.
//   X:     A corresponding CancelPrefetch call is placed here.
//   S:     |kCacheSoftExpiryInHours| hours
//   H:     |kCacheHardExpiryInHours| hours
//
// Note: It is guaranteed that S < H and that H < 2*S.
//
// Prefetches with the cache is initially stale/empty:
//
//      t=0                        S       H               F2+S   F2+H
//      /                          /       /               /      /
//  ---o--------------------------o-------o---------------o-------o---------> t
//     :                          :       :               :       :
//     [)                         :       :               :       :
//     [F--)                      :       :               :       :
//     [F------------------------):       :               :       :
//     [F--------------------------------):               :       :
//     [F-------------------------F----------)            :       :
//     [F-------------------------F----------------------):       :
//     [F-------------------------F------------------------------):
//     [F-------------------------F-----------------------F------------------>
//
TEST_F(FacetManagerTest, PrefetchWithEmptyOrStaleCache) {
  struct {
    base::TimeDelta prefetch_length;
    size_t expected_num_fetches;
  } const kTestCases[] = {
      // Note: Zero length prefetches are tested later.
      {GetShortTestPeriod(), 1},
      {GetCacheSoftExpiryPeriod(), 1},
      {GetCacheHardExpiryPeriod(), 1},
      {GetCacheHardExpiryPeriod() + GetShortTestPeriod(), 2},
      {GetCacheSoftExpiryPeriod() + GetCacheSoftExpiryPeriod(), 2},
      {GetCacheHardExpiryPeriod() + GetCacheSoftExpiryPeriod(), 2},
      {base::TimeDelta::Max(), 3}};

  const base::TimeDelta kExpectedFetchTimes[] = {
      base::TimeDelta(),
      GetCacheSoftExpiryPeriod(),
      2 * GetCacheSoftExpiryPeriod()};

  const base::TimeDelta kMaximumTestDuration = 2 * GetCacheHardExpiryPeriod();

  for (const bool cache_initially_stale : kFalseTrue) {
    for (size_t i = 0; i < std::size(kTestCases); ++i) {
      SCOPED_TRACE(testing::Message() << "Test case: #" << i);
      SCOPED_TRACE(cache_initially_stale ? "Cache initially stale"
                                         : "Cache initially empty");

      if (cache_initially_stale) {
        fake_facet_manager_host()->set_fake_database_content(
            GetTestEquivalenceClassWithUpdateTime(Now()));
        AdvanceTime(GetCacheHardExpiryPeriod());
      } else {
        fake_facet_manager_host()->clear_fake_database_content();
      }

      std::vector<ExpectedFetchDetails> expected_fetches;
      expected_fetches.resize(kTestCases[i].expected_num_fetches);
      for (size_t f = 0; f < kTestCases[i].expected_num_fetches; ++f)
        expected_fetches[f].time = Now() + kExpectedFetchTimes[f];

      CreateFacetManager();
      Prefetch(SafeAdd(Now(), kTestCases[i].prefetch_length));
      ASSERT_NO_FATAL_FAILURE(AdvanceTimeAndVerifyPrefetchWithFetchesAt(
          Now() + std::min(kTestCases[i].prefetch_length, kMaximumTestDuration),
          expected_fetches));
      ASSERT_NO_FATAL_FAILURE(ExpectNoFetchNeeded());
      if (kTestCases[i].prefetch_length < base::TimeDelta::Max()) {
        EXPECT_TRUE(facet_manager()->CanBeDiscarded());
        EXPECT_FALSE(main_task_runner()->HasPendingTask());
      } else {
        EXPECT_FALSE(facet_manager()->CanBeDiscarded());
      }

      DestroyFacetManager();
    }
  }
}

// Prefetches with cached affiliation data that is fresh to some extent:
//
// Suppose an unrelated fetch at t=0 has resulted in affiliation information
// being stored into the cache (freshness interval marked with '='). See legend
// above.
//
//      t=0                        S       H               F2+S   F2+H
//      /                          /       /               /      /
//  ---o--------------------------o-------o---------------o-------o-----> t
//     [F================================):               :       :
//     :                          :       :               :       :
//     [)                         :       :               :       :
//     [--)                       :       :               :       :
//     [-------------------------):       :               :       :
//     [---------------------------------):               :       :
//     [--------------------------F-----------)           :       :
//     [--------------------------F----------------------):       :
//     [--------------------------F------------------------------):
//     [--------------------------F-----------------------F------------->
//                                :       :               :       :
//         [)                     :       :               :       :
//         [--)                   :       :               :       :
//         [---------------------):       :               :       :
//         [-----------------------------):               :       :
//         [----------------------F-----------)           :       :
//         [----------------------F----------------------):       :
//         [----------------------F------------------------------):
//         [----------------------F-----------------------F------------->
//                                :       :               :       :
//                                [)      :               :       :
//                                [----)  :               :       :
//                                [------):               :       :
//                                [F----------)           :       :
//                                [F---------------------):       :
//                                [F-----------------------------):
//                                [F----------------------F------------->
//
//      t=0                      S   S+D   H            F2+S         F2+H
//      /                        \   /     /               \         /
//  ---o--------------------------o-o-----o-----------------o-------o-----> t
//     [F================================):                 :       :
//                                :       :                 :       :
//                                : [)    :                 :       :
//                                : [----):                 :       :
//                                : [F------)               :       :
//                                : [F---------------------):       :
//                                : [F-----------------------------):
//                                : [F----------------------F------------->
//
TEST_F(FacetManagerTest, PrefetchTriggeredFetchSchedulingAfterNonEmptyCache) {
  struct {
    base::TimeDelta prefetch_start;
    base::TimeDelta prefetch_end;
    size_t expected_num_fetches;
  } const kTestCases[] = {
      // Note: Zero length prefetches are tested later.

      // Prefetch starts at the exact time the data was incidentally fetched.
      {base::TimeDelta(), GetShortTestPeriod(), 0},
      {base::TimeDelta(), GetCacheSoftExpiryPeriod(), 0},
      {base::TimeDelta(), GetCacheHardExpiryPeriod(), 0},
      {base::TimeDelta(), GetCacheHardExpiryPeriod() + GetShortTestPeriod(), 1},
      {base::TimeDelta(), 2 * GetCacheSoftExpiryPeriod(), 1},
      {base::TimeDelta(),
       GetCacheSoftExpiryPeriod() + GetCacheHardExpiryPeriod(),
       1},
      {base::TimeDelta(), base::TimeDelta::Max(), 2},

      // Prefetch starts a short time after the unrelated fetch.
      {GetShortTestPeriod(), 2 * GetShortTestPeriod(), 0},
      {GetShortTestPeriod(), GetCacheSoftExpiryPeriod(), 0},
      {GetShortTestPeriod(), GetCacheHardExpiryPeriod(), 0},
      {GetShortTestPeriod(),
       GetCacheHardExpiryPeriod() + GetShortTestPeriod(),
       1},
      {GetShortTestPeriod(), 2 * GetCacheSoftExpiryPeriod(), 1},
      {GetShortTestPeriod(),
       GetCacheSoftExpiryPeriod() + GetCacheHardExpiryPeriod(),
       1},
      {GetShortTestPeriod(), base::TimeDelta::Max(), 2},

      // Prefetch starts at the soft expiry time of the unrelated fetch.
      {GetCacheSoftExpiryPeriod(),
       GetCacheSoftExpiryPeriod() + GetShortTestPeriod(),
       0},
      {GetCacheSoftExpiryPeriod(), GetCacheHardExpiryPeriod(), 0},
      {GetShortTestPeriod(),
       GetCacheHardExpiryPeriod() + GetShortTestPeriod(),
       1},
      {GetCacheSoftExpiryPeriod(), 2 * GetCacheSoftExpiryPeriod(), 1},
      {GetCacheSoftExpiryPeriod(),
       GetCacheSoftExpiryPeriod() + GetCacheHardExpiryPeriod(),
       1},
      {GetCacheSoftExpiryPeriod(), base::TimeDelta::Max(), 2}};

  const base::TimeDelta kExpectedFetchTimes[] = {
      GetCacheSoftExpiryPeriod(), 2 * GetCacheSoftExpiryPeriod()};

  const base::TimeDelta kMaximumTestDuration = 2 * GetCacheHardExpiryPeriod();

  for (size_t i = 0; i < std::size(kTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << "Test case: #" << i);

    fake_facet_manager_host()->set_fake_database_content(
        GetTestEquivalenceClassWithUpdateTime(Now()));

    const base::Time prefetch_end = SafeAdd(Now(), kTestCases[i].prefetch_end);
    const base::Time testing_end =
        std::min(prefetch_end, Now() + kMaximumTestDuration);

    std::vector<ExpectedFetchDetails> expected_fetches;
    expected_fetches.resize(kTestCases[i].expected_num_fetches);
    for (size_t f = 0; f < kTestCases[i].expected_num_fetches; ++f)
      expected_fetches[f].time = Now() + kExpectedFetchTimes[f];

    AdvanceTime(kTestCases[i].prefetch_start);

    CreateFacetManager();
    Prefetch(prefetch_end);
    ASSERT_NO_FATAL_FAILURE(AdvanceTimeAndVerifyPrefetchWithFetchesAt(
        testing_end, expected_fetches));
    ASSERT_NO_FATAL_FAILURE(ExpectNoFetchNeeded());
    if (kTestCases[i].prefetch_end < base::TimeDelta::Max()) {
      EXPECT_TRUE(facet_manager()->CanBeDiscarded());
      EXPECT_FALSE(main_task_runner()->HasPendingTask());
    } else {
      EXPECT_FALSE(facet_manager()->CanBeDiscarded());
    }
    DestroyFacetManager();
  }
}

// Last block of tests from above.
TEST_F(FacetManagerTest, PrefetchTriggeredFetchSchedulingAfterNonEmptyCache2) {
  struct {
    base::TimeDelta prefetch_start;
    base::TimeDelta prefetch_end;
    size_t expected_num_fetches;
  } const kTestCases[] = {
      // Note: Zero length prefetches are tested later.

      // Prefetch starts between the soft and hard expiry time.
      {GetCacheSoftExpiryPeriod() + GetShortTestPeriod(),
       GetCacheHardExpiryPeriod(),
       0},
      {GetCacheSoftExpiryPeriod() + GetShortTestPeriod(),
       GetCacheHardExpiryPeriod() + GetShortTestPeriod(),
       1},
      {GetCacheSoftExpiryPeriod() + GetShortTestPeriod(),
       2 * GetCacheSoftExpiryPeriod() + GetShortTestPeriod(),
       1},
      {GetCacheSoftExpiryPeriod() + GetShortTestPeriod(),
       GetCacheHardExpiryPeriod() + GetCacheSoftExpiryPeriod() +
           GetShortTestPeriod(),
       1},
      {GetCacheSoftExpiryPeriod() + GetShortTestPeriod(),
       base::TimeDelta::Max(),
       2}};

  const base::TimeDelta kExpectedFetchTimes[] = {
      GetCacheSoftExpiryPeriod() + GetShortTestPeriod(),
      2 * GetCacheSoftExpiryPeriod() + GetShortTestPeriod()};

  const base::TimeDelta kMaximumTestDuration = 2 * GetCacheHardExpiryPeriod();

  for (size_t i = 0; i < std::size(kTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << "Test case: #" << i);

    fake_facet_manager_host()->set_fake_database_content(
        GetTestEquivalenceClassWithUpdateTime(Now()));

    const base::Time prefetch_end = SafeAdd(Now(), kTestCases[i].prefetch_end);
    const base::Time testing_end =
        std::min(prefetch_end, Now() + kMaximumTestDuration);

    std::vector<ExpectedFetchDetails> expected_fetches;
    expected_fetches.resize(kTestCases[i].expected_num_fetches);
    for (size_t f = 0; f < kTestCases[i].expected_num_fetches; ++f)
      expected_fetches[f].time = Now() + kExpectedFetchTimes[f];

    AdvanceTime(kTestCases[i].prefetch_start);

    CreateFacetManager();
    Prefetch(prefetch_end);
    ASSERT_NO_FATAL_FAILURE(AdvanceTimeAndVerifyPrefetchWithFetchesAt(
        testing_end, expected_fetches));
    ASSERT_NO_FATAL_FAILURE(ExpectNoFetchNeeded());
    if (kTestCases[i].prefetch_end < base::TimeDelta::Max()) {
      EXPECT_TRUE(facet_manager()->CanBeDiscarded());
      EXPECT_FALSE(main_task_runner()->HasPendingTask());
    } else {
      EXPECT_FALSE(facet_manager()->CanBeDiscarded());
    }
    DestroyFacetManager();
  }
}

// Prefetches from above that have zero length.
TEST_F(FacetManagerTest, ExpiredPrefetchDoesNothing) {
  base::TimeDelta kPrefetchStart[] = {base::TimeDelta(),
                                      GetShortTestPeriod(),
                                      GetCacheSoftExpiryPeriod(),
                                      GetCacheHardExpiryPeriod(),
                                      base::TimeDelta::Max()};

  for (base::TimeDelta prefetch_start : kPrefetchStart) {
    SCOPED_TRACE(testing::Message() << "Prefetch start: " << prefetch_start);

    if (prefetch_start < base::TimeDelta::Max()) {
      fake_facet_manager_host()->set_fake_database_content(
          GetTestEquivalenceClassWithUpdateTime(Now()));
      AdvanceTime(prefetch_start);
    } else {
      fake_facet_manager_host()->clear_fake_database_content();
    }

    CreateFacetManager();
    Prefetch(Now());
    ASSERT_NO_FATAL_FAILURE(ExpectNoFetchNeeded());
    EXPECT_TRUE(facet_manager()->CanBeDiscarded());
    EXPECT_FALSE(main_task_runner()->HasPendingTask());
    DestroyFacetManager();
  }
}

// Nested prefetches. See legend above.
//
//      t=0                        S       H               F2+S   F2+H
//      /                          /       /               /      /
//  ---o--------------------------o-------o---------------o-------o---------> t
//     :                          :       :               :       :
//     [F=========================F==============================):
//     [F=========================F=======================F===========>
//     [--)                       :       :               :       :
//     :  [--)                    :       :               :       :
//     :  [----------------------):       :               :       :
//     :  [------------------------------):               :       :
//     :  [----------------------------------------------):       :
//     :  [------------------------------------------------------):
//     :                          [------):               :       :
//     :                          [----------------------):       :
//     :                          [------------------------------):
//     :                          : [----):               :       :
//     :                          : [--------------------):       :
//     :                          : [----------------------------):
//     :                          :                       [------):
//     :                          :                       : [----):
//
TEST_F(FacetManagerTest, NestedPrefetches) {
  struct {
    base::TimeDelta prefetch_length;
    size_t expected_num_fetches;
  } const kFirstPrefetchParams[] = {
      {GetCacheHardExpiryPeriod() + GetCacheSoftExpiryPeriod(), 2},
      {base::TimeDelta::Max(), 3},
  };

  struct {
    base::TimeDelta second_prefetch_start;
    base::TimeDelta second_prefetch_end;
  } const kSecondPrefetchParams[] = {
      {base::TimeDelta(), GetShortTestPeriod()},
      {GetShortTestPeriod(), 2 * GetShortTestPeriod()},
      {GetShortTestPeriod(), GetCacheSoftExpiryPeriod()},
      {GetShortTestPeriod(), GetCacheHardExpiryPeriod()},
      {GetShortTestPeriod(), 2 * GetCacheSoftExpiryPeriod()},
      {GetShortTestPeriod(),
       GetCacheSoftExpiryPeriod() + GetCacheHardExpiryPeriod()},
      {GetCacheSoftExpiryPeriod(), GetCacheHardExpiryPeriod()},
      {GetCacheSoftExpiryPeriod(), 2 * GetCacheSoftExpiryPeriod()},
      {GetCacheSoftExpiryPeriod(),
       GetCacheSoftExpiryPeriod() + GetCacheHardExpiryPeriod()},
      {GetCacheSoftExpiryPeriod() + GetShortTestPeriod(),
       GetCacheHardExpiryPeriod()},
      {GetCacheSoftExpiryPeriod() + GetShortTestPeriod(),
       2 * GetCacheSoftExpiryPeriod()},
      {GetCacheSoftExpiryPeriod() + GetShortTestPeriod(),
       GetCacheSoftExpiryPeriod() + GetCacheHardExpiryPeriod()},
      {2 * GetCacheSoftExpiryPeriod(),
       GetCacheHardExpiryPeriod() + GetCacheSoftExpiryPeriod()},
      {2 * GetCacheSoftExpiryPeriod() + GetShortTestPeriod(),
       GetCacheHardExpiryPeriod() + GetCacheSoftExpiryPeriod()}};

  const base::TimeDelta kExpectedFetchTimes[] = {
      base::TimeDelta(),
      GetCacheSoftExpiryPeriod(),
      2 * GetCacheSoftExpiryPeriod()};

  const base::TimeDelta kTestDuration =
      GetCacheSoftExpiryPeriod() + GetCacheHardExpiryPeriod();

  for (size_t j = 0; j < std::size(kFirstPrefetchParams); ++j) {
    for (size_t i = 0; i < std::size(kSecondPrefetchParams); ++i) {
      SCOPED_TRACE(testing::Message() << "Test case: #" << j << "." << i);

      fake_facet_manager_host()->clear_fake_database_content();

      std::vector<ExpectedFetchDetails> expected_fetches;
      expected_fetches.resize(kFirstPrefetchParams[j].expected_num_fetches);
      for (size_t f = 0; f < kFirstPrefetchParams[j].expected_num_fetches; ++f)
        expected_fetches[f].time = Now() + kExpectedFetchTimes[f];

      CreateFacetManager();
      Prefetch(SafeAdd(Now(), kFirstPrefetchParams[j].prefetch_length));
      SchedulePrefetch(Now() + kSecondPrefetchParams[i].second_prefetch_start,
                       Now() + kSecondPrefetchParams[i].second_prefetch_end);
      ASSERT_NO_FATAL_FAILURE(AdvanceTimeAndVerifyPrefetchWithFetchesAt(
          Now() + kTestDuration, expected_fetches));
      ASSERT_NO_FATAL_FAILURE(ExpectNoFetchNeeded());
      if (kFirstPrefetchParams[j].prefetch_length < base::TimeDelta::Max()) {
        EXPECT_TRUE(facet_manager()->CanBeDiscarded());
        EXPECT_FALSE(main_task_runner()->HasPendingTask());
      } else {
        EXPECT_FALSE(facet_manager()->CanBeDiscarded());
      }

      DestroyFacetManager();
    }
  }
}

// Overlapping prefetches. See legend above.
//
//      t=0                        S       H               F2+S    F2+H
//      /                          /       /               /      /
//  ---o--------------------------o-------o---------------o-------o---------> t
//     :                          :       :               :       :
//     [F================================):               :       :
//     :     [--------------------F------------------------------):
//     :     [--------------------F-----------------------F----------->
//     :                          [F-----------------------------):
//     :                          [F----------------------F----------->
//
TEST_F(FacetManagerTest, OverlappingPrefetches) {
  struct {
    base::TimeDelta second_prefetch_start;
    base::TimeDelta second_prefetch_end;
    size_t expected_num_fetches;
  } const kTestCases[] = {
      {GetShortTestPeriod(),
       GetCacheSoftExpiryPeriod() + GetCacheHardExpiryPeriod(),
       2},
      {GetShortTestPeriod(), base::TimeDelta::Max(), 3},
      {GetCacheSoftExpiryPeriod(),
       GetCacheSoftExpiryPeriod() + GetCacheHardExpiryPeriod(),
       2},
      {GetCacheSoftExpiryPeriod(), base::TimeDelta::Max(), 3}};

  const base::TimeDelta kExpectedFetchTimes[] = {
      base::TimeDelta(),
      GetCacheSoftExpiryPeriod(),
      2 * GetCacheSoftExpiryPeriod()};

  const base::TimeDelta kTestDuration =
      GetCacheSoftExpiryPeriod() + GetCacheHardExpiryPeriod();

  for (size_t i = 0; i < std::size(kTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << "Test case: #" << i);

    fake_facet_manager_host()->clear_fake_database_content();

    std::vector<ExpectedFetchDetails> expected_fetches;
    expected_fetches.resize(kTestCases[i].expected_num_fetches);
    for (size_t f = 0; f < kTestCases[i].expected_num_fetches; ++f)
      expected_fetches[f].time = Now() + kExpectedFetchTimes[f];

    CreateFacetManager();
    Prefetch(SafeAdd(Now(), GetCacheHardExpiryPeriod()));
    SchedulePrefetch(Now() + kTestCases[i].second_prefetch_start,
                     SafeAdd(Now(), kTestCases[i].second_prefetch_end));
    ASSERT_NO_FATAL_FAILURE(AdvanceTimeAndVerifyPrefetchWithFetchesAt(
        Now() + kTestDuration, expected_fetches));
    ASSERT_NO_FATAL_FAILURE(ExpectNoFetchNeeded());
    if (kTestCases[i].second_prefetch_end < base::TimeDelta::Max()) {
      EXPECT_TRUE(facet_manager()->CanBeDiscarded());
      EXPECT_FALSE(main_task_runner()->HasPendingTask());
    } else {
      EXPECT_FALSE(facet_manager()->CanBeDiscarded());
    }

    DestroyFacetManager();
  }
}

// Prefetches with network fetches taking non-zero time. See legend above.
//
//      t=0                      S   S+D   H                   S+H     S+H+2*D
//      /                        \   /     /                     \     /
//  ---o--------------------------o-o-----o-----------------------o-o-o-----> t
//     :                          : :     :                       : : :
//     [NNF------------------------------):                       : : :
//     [F-------------------------NNF----------------------------): : :
//     [NNNNNNNNNNNNNNNNNNNNNNNNNNF------------------------------): : :
//     :                          : :     :                       : : :
//     [NNF----------------------------------)                    : : :
//     [F-------------------------NNF------------------------------): :
//     [NNNNNNNNNNNNNNNNNNNNNNNNNNNNF------------------------------): :
//     [NNF-------------------------NNF------------------------------):
//     :                          : :     :                       :
//     [NNN)                      : :     :                       :
//     [NNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNN):                       :
//     [F-------------------------NNNNNNNNNNNNNNNNNNNNNNNNNNNNNNN):
//     [NNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNN):
//     [NNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNN...
//
TEST_F(FacetManagerTest, PrefetchWithNonInstantFetches) {
  struct {
    base::TimeDelta prefetch_length;
    base::TimeDelta expected_fetch_time1;
    base::TimeDelta fetch_completion_delay1;
    base::TimeDelta expected_fetch_time2;
    base::TimeDelta fetch_completion_delay2;
  } const kTestCases[] = {
      {GetCacheHardExpiryPeriod(),
       base::TimeDelta(),
       GetShortTestPeriod(),
       base::TimeDelta::Max(),
       base::TimeDelta::Max()},
      {GetCacheHardExpiryPeriod() + GetCacheSoftExpiryPeriod(),
       base::TimeDelta(),
       base::TimeDelta(),
       GetCacheSoftExpiryPeriod(),
       GetCacheSoftExpiryPeriod() + GetShortTestPeriod()},
      {GetCacheHardExpiryPeriod() + GetCacheSoftExpiryPeriod(),
       base::TimeDelta(),
       GetCacheSoftExpiryPeriod(),
       base::TimeDelta::Max(),
       base::TimeDelta::Max()},
      {GetCacheHardExpiryPeriod() + GetShortTestPeriod(),
       base::TimeDelta(),
       GetShortTestPeriod(),
       base::TimeDelta::Max(),
       base::TimeDelta::Max()},
      {GetCacheHardExpiryPeriod() + GetCacheSoftExpiryPeriod() +
           GetShortTestPeriod(),
       base::TimeDelta(),
       base::TimeDelta(),
       GetCacheSoftExpiryPeriod(),
       GetCacheSoftExpiryPeriod() + GetShortTestPeriod()},
      {GetCacheHardExpiryPeriod() + GetCacheSoftExpiryPeriod() +
           GetShortTestPeriod(),
       base::TimeDelta(),
       GetCacheSoftExpiryPeriod() + GetShortTestPeriod(),
       base::TimeDelta::Max(),
       base::TimeDelta::Max()},
      {GetCacheHardExpiryPeriod() + GetCacheSoftExpiryPeriod() +
           2 * GetShortTestPeriod(),
       base::TimeDelta(),
       GetShortTestPeriod(),
       GetCacheSoftExpiryPeriod() + GetShortTestPeriod(),
       GetCacheSoftExpiryPeriod() + 2 * GetShortTestPeriod()},
      {GetShortTestPeriod(),
       base::TimeDelta(),
       base::TimeDelta::Max(),
       base::TimeDelta::Max(),
       base::TimeDelta::Max()},
      {GetCacheHardExpiryPeriod(),
       base::TimeDelta(),
       base::TimeDelta::Max(),
       base::TimeDelta::Max(),
       base::TimeDelta::Max()},
      {GetCacheSoftExpiryPeriod() + GetCacheHardExpiryPeriod(),
       base::TimeDelta(),
       base::TimeDelta(),
       GetCacheSoftExpiryPeriod(),
       base::TimeDelta::Max()},
      {GetCacheSoftExpiryPeriod() + GetCacheHardExpiryPeriod(),
       base::TimeDelta(),
       base::TimeDelta::Max(),
       base::TimeDelta::Max(),
       base::TimeDelta::Max()},
      {base::TimeDelta::Max(),
       base::TimeDelta(),
       base::TimeDelta::Max(),
       base::TimeDelta::Max(),
       base::TimeDelta::Max()}};

  const base::TimeDelta kMaximumTestDuration = GetCacheSoftExpiryPeriod() +
                                               GetCacheHardExpiryPeriod() +
                                               2 * GetShortTestPeriod();

  for (size_t i = 0; i < std::size(kTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << "Test case: #" << i);

    fake_facet_manager_host()->clear_fake_database_content();

    const base::Time testing_end =
        Now() + std::min(kTestCases[i].prefetch_length, kMaximumTestDuration);

    std::vector<ExpectedFetchDetails> expected_fetches(1);
    expected_fetches[0].time = Now() + kTestCases[i].expected_fetch_time1,
    expected_fetches[0].completion_delay =
        kTestCases[i].fetch_completion_delay1;
    if (kTestCases[i].expected_fetch_time2 != base::TimeDelta::Max()) {
      expected_fetches.resize(2);
      expected_fetches[1].time = Now() + kTestCases[i].expected_fetch_time2,
      expected_fetches[1].completion_delay =
          kTestCases[i].fetch_completion_delay2;
    }

    CreateFacetManager();
    Prefetch(SafeAdd(Now(), kTestCases[i].prefetch_length));
    ASSERT_NO_FATAL_FAILURE(AdvanceTimeAndVerifyPrefetchWithFetchesAt(
        testing_end, expected_fetches));
    if (kTestCases[i].prefetch_length < base::TimeDelta::Max()) {
      EXPECT_FALSE(facet_manager()->DoesRequireFetch());
      EXPECT_TRUE(facet_manager()->CanBeDiscarded());
      EXPECT_FALSE(main_task_runner()->HasPendingTask());
    } else {
      ASSERT_NO_FATAL_FAILURE(ExpectFetchNeeded());
      EXPECT_FALSE(facet_manager()->CanBeDiscarded());
    }
    DestroyFacetManager();
  }
}

// Canceling prefetches. See legend above.
//
//      t=0                        S       H               F2+S   F2+H
//      /                          /       /               /      /
//  ---o--------------------------o-------o---------------o-------o---------> t
//     :                          :       :               :       :
//     [F--X- - - - - - - - - - - - - - -):               :       :
//     [F-------------------------X - - -):               :       :
//     [F----------------------------X- -):               :       :
//     [F--X- - - - - - - - - - - - - - - - - - - - - - - - - - - - - ->
//     [F-------------------------X - - - - - - - - - - - - - - - - - ->
//     [F-------------------------F--X- - - - - - - - - - - - - - - - ->
//     [F-------------------------F----------X- - - - - - - - - - - - ->
//     [F-------------------------F-----------------------X - - - - - ->
//     [F-------------------------F-----------------------F--X- - - - ->
//
TEST_F(FacetManagerTest, CancelPrefetch) {
  struct {
    base::TimeDelta prefetch_length;
    base::TimeDelta cancel_time;
    size_t expected_num_fetches;
  } const kTestCases[] = {
      {GetCacheHardExpiryPeriod(), GetShortTestPeriod(), 1},
      {GetCacheHardExpiryPeriod(), GetCacheSoftExpiryPeriod(), 1},
      {GetCacheHardExpiryPeriod(),
       GetCacheSoftExpiryPeriod() + GetShortTestPeriod(),
       1},
      {base::TimeDelta::Max(), GetShortTestPeriod(), 1},
      {base::TimeDelta::Max(), GetCacheSoftExpiryPeriod(), 1},
      {base::TimeDelta::Max(),
       GetCacheSoftExpiryPeriod() + GetShortTestPeriod(),
       2},
      {base::TimeDelta::Max(),
       GetCacheHardExpiryPeriod() + GetShortTestPeriod(),
       2},
      {base::TimeDelta::Max(), 2 * GetCacheSoftExpiryPeriod(), 2},
      {base::TimeDelta::Max(),
       2 * GetCacheSoftExpiryPeriod() + GetShortTestPeriod(),
       3}};

  const base::TimeDelta kExpectedFetchTimes[] = {
      base::TimeDelta(),
      GetCacheSoftExpiryPeriod(),
      2 * GetCacheSoftExpiryPeriod()};

  for (size_t i = 0; i < std::size(kTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << "Test case: #" << i);

    fake_facet_manager_host()->clear_fake_database_content();

    std::vector<ExpectedFetchDetails> expected_fetches;
    expected_fetches.resize(kTestCases[i].expected_num_fetches);
    for (size_t f = 0; f < kTestCases[i].expected_num_fetches; ++f)
      expected_fetches[f].time = Now() + kExpectedFetchTimes[f];

    CreateFacetManager();
    Prefetch(SafeAdd(Now(), kTestCases[i].prefetch_length));
    ScheduleCancelPrefetch(Now() + kTestCases[i].cancel_time,
                           SafeAdd(Now(), kTestCases[i].prefetch_length));
    ASSERT_NO_FATAL_FAILURE(AdvanceTimeAndVerifyPrefetchWithFetchesAt(
        Now() + kTestCases[i].cancel_time, expected_fetches));
    ASSERT_NO_FATAL_FAILURE(ExpectNoFetchNeeded());
    EXPECT_TRUE(facet_manager()->CanBeDiscarded());
    AdvanceTime(GetCacheHardExpiryPeriod());
    EXPECT_FALSE(main_task_runner()->HasPendingTask());
    DestroyFacetManager();
  }
}

// Canceling in case of multiple nested prefetches. See legend above.
//
//      t=0                        S       H               F2+S   F2+H
//      /                          /       /               /      /
//  ---o--------------------------o-------o---------------o-------o---------> t
//     :                          :       :               :       :
//     [F-------------------------F---------------------------X- ):
//        [---------------------------------X- - - - - - - - - - - -)
//           [--X- - - - - - - - - - - - - - - - - - - - - - - - - - -)
//
TEST_F(FacetManagerTest, CancelNestedPrefetches) {
  const base::TimeDelta kPrefetchLength =
      GetCacheSoftExpiryPeriod() + GetCacheHardExpiryPeriod();
  const base::TimeDelta kTestDuration =
      kPrefetchLength + 2 * GetShortTestPeriod();
  const base::TimeDelta kLastCancelTime =
      2 * GetCacheSoftExpiryPeriod() + GetShortTestPeriod();

  std::vector<ExpectedFetchDetails> expected_fetches(2);
  expected_fetches[0].time = Now();
  expected_fetches[1].time = Now() + GetCacheSoftExpiryPeriod();

  CreateFacetManager();
  Prefetch(Now() + kPrefetchLength);
  SchedulePrefetch(Now() + GetShortTestPeriod(),
                   Now() + kPrefetchLength + GetShortTestPeriod());
  SchedulePrefetch(Now() + 2 * GetShortTestPeriod(),
                   Now() + kPrefetchLength + 2 * GetShortTestPeriod());
  ScheduleCancelPrefetch(Now() + 3 * GetShortTestPeriod(),
                         Now() + 2 * GetShortTestPeriod() + kPrefetchLength);
  ScheduleCancelPrefetch(
      Now() + GetCacheHardExpiryPeriod() + GetShortTestPeriod(),
      Now() + kPrefetchLength + GetShortTestPeriod());
  ScheduleCancelPrefetch(Now() + kLastCancelTime, Now() + kPrefetchLength);
  ASSERT_NO_FATAL_FAILURE(AdvanceTimeAndVerifyPrefetchWithFetchesAt(
      Now() + kLastCancelTime, expected_fetches));
  EXPECT_TRUE(facet_manager()->CanBeDiscarded());
  ASSERT_NO_FATAL_FAILURE(ExpectNoFetchNeeded());
  AdvanceTime(kTestDuration - kLastCancelTime);
  EXPECT_FALSE(main_task_runner()->HasPendingTask());
  DestroyFacetManager();
}

// Canceling in case of duplicate prefetches with the same |until| value. See
// legend above.
//
//      t=0                        S       H               F2+S   F2+H
//      /                          /       /               /      /
//  ---o--------------------------o-------o---------------o-------o---------> t
//     :                          :       :               :       :
//     [F-------------------------F-----------------------F--X- - - - ->
//     [--------------------------X - - - - - - - - - - - - - - - - - ->
//     [--X - - - - - - - - - - - - - - - - - - - - - - - - - - - - - ->
//
TEST_F(FacetManagerTest, CancelNestedPrefetchesWithMultiplicity) {
  const base::TimeDelta kTestPeriod = 3 * GetCacheSoftExpiryPeriod();
  const base::TimeDelta kLastCancelTime =
      2 * GetCacheSoftExpiryPeriod() + GetShortTestPeriod();

  std::vector<ExpectedFetchDetails> expected_fetches(3);
  expected_fetches[0].time = Now();
  expected_fetches[1].time = Now() + GetCacheSoftExpiryPeriod();
  expected_fetches[2].time = Now() + 2 * GetCacheSoftExpiryPeriod();

  CreateFacetManager();
  Prefetch(base::Time::Max());
  SchedulePrefetch(Now() + GetShortTestPeriod(), base::Time::Max());
  SchedulePrefetch(Now() + 2 * GetShortTestPeriod(), base::Time::Max());
  ScheduleCancelPrefetch(Now() + GetShortTestPeriod(), base::Time::Max());
  ScheduleCancelPrefetch(Now() + GetCacheSoftExpiryPeriod(), base::Time::Max());
  ScheduleCancelPrefetch(Now() + kLastCancelTime, base::Time::Max());
  ASSERT_NO_FATAL_FAILURE(AdvanceTimeAndVerifyPrefetchWithFetchesAt(
      Now() + kLastCancelTime, expected_fetches));
  EXPECT_TRUE(facet_manager()->CanBeDiscarded());
  ASSERT_NO_FATAL_FAILURE(ExpectNoFetchNeeded());
  AdvanceTime(kTestPeriod - kLastCancelTime);
  EXPECT_FALSE(main_task_runner()->HasPendingTask());
  DestroyFacetManager();
}

TEST_F(FacetManagerTest, CancelingNonexistentPrefetchesIsSilentlyIgnored) {
  const base::TimeDelta kPrefetchLength =
      GetCacheSoftExpiryPeriod() + GetCacheHardExpiryPeriod();

  std::vector<ExpectedFetchDetails> expected_fetches(2);
  expected_fetches[0].time = Now();
  expected_fetches[1].time = Now() + GetCacheSoftExpiryPeriod();

  CreateFacetManager();
  CancelPrefetch(Now() + GetShortTestPeriod());
  CancelPrefetch(base::Time::Max());
  EXPECT_TRUE(facet_manager()->CanBeDiscarded());
  ASSERT_NO_FATAL_FAILURE(ExpectNoFetchNeeded());
  EXPECT_FALSE(main_task_runner()->HasPendingTask());

  Prefetch(Now() + kPrefetchLength);
  ScheduleCancelPrefetch(Now() + GetShortTestPeriod(),
                         Now() + GetShortTestPeriod());
  ScheduleCancelPrefetch(Now() + GetShortTestPeriod(), base::Time::Max());
  ASSERT_NO_FATAL_FAILURE(AdvanceTimeAndVerifyPrefetchWithFetchesAt(
      Now() + kPrefetchLength, expected_fetches));
  EXPECT_TRUE(facet_manager()->CanBeDiscarded());
  ASSERT_NO_FATAL_FAILURE(ExpectNoFetchNeeded());
  EXPECT_FALSE(main_task_runner()->HasPendingTask());
  DestroyFacetManager();
}

TEST_F(FacetManagerTest, CachedDataCannotBeDiscarded) {
  CreateFacetManager();

  const base::TimeDelta kPrefetchLength =
      2 * GetCacheSoftExpiryPeriod() + GetCacheHardExpiryPeriod();

  facet_manager_notifier()->set_accuracy(NotificationAccuracy::NEVER_CALLED);

  const base::Time prefetch_end = Now() + kPrefetchLength;
  std::vector<ExpectedFetchDetails> expected_fetches(1);
  expected_fetches[0].time = Now();

  Prefetch(prefetch_end);
  ASSERT_NO_FATAL_FAILURE(AdvanceTimeAndVerifyPrefetchWithFetchesAt(
      Now() + GetCacheSoftExpiryPeriod(), expected_fetches));
  for (base::TimeDelta step : SamplingPoints(prefetch_end - Now())) {
    SCOPED_TRACE(testing::Message() << "dT: " << DeltaNow());
    EXPECT_FALSE(facet_manager()->CanBeDiscarded());
    ASSERT_TRUE(facet_manager()->DoesRequireFetch());
    if (DeltaNow() < GetCacheHardExpiryPeriod())
      ExpectRequestsServedFromCache();
    AdvanceTime(step);
  }
  EXPECT_TRUE(facet_manager()->CanBeDiscarded());
  EXPECT_FALSE(main_task_runner()->HasPendingTask());
  DestroyFacetManager();
}

// RequestNotificationAtTime() ends up calling NotifyAtRequestedTime() always
// a bit earlier than needed. This should result in NotifyAtRequestedTime()
// being called repeatedly until the callback is finally on time, but should
// not otherwise result in a change of behavior.
TEST_F(FacetManagerTest, RequestedNotificationsComeTooEarly) {
  const base::TimeDelta kTestPeriod =
      2 * GetCacheSoftExpiryPeriod() + GetCacheHardExpiryPeriod();

  facet_manager_notifier()->set_accuracy(NotificationAccuracy::TOO_EARLY);

  std::vector<ExpectedFetchDetails> expected_fetches(3);
  expected_fetches[0].time = Now();
  expected_fetches[1].time = Now() + GetCacheSoftExpiryPeriod();
  expected_fetches[2].time = Now() + 2 * GetCacheSoftExpiryPeriod();

  CreateFacetManager();
  Prefetch(Now() + kTestPeriod);
  ASSERT_NO_FATAL_FAILURE(AdvanceTimeAndVerifyPrefetchWithFetchesAt(
      Now() + kTestPeriod, expected_fetches));
  EXPECT_TRUE(facet_manager()->CanBeDiscarded());
  EXPECT_FALSE(main_task_runner()->HasPendingTask());
  DestroyFacetManager();
}

// RequestNotificationAtTime() ends up calling NotifyAtRequestedTime() always
// a short time after desired. This may result in SignalNeedNetworkRequest()
// coming in late, but DoesRequireFetch() should get set at the correct time.
TEST_F(FacetManagerTest, RequestedNotificationsComeTooLate) {
  const base::TimeDelta kPrefetchLength =
      2 * GetCacheSoftExpiryPeriod() + GetCacheHardExpiryPeriod();

  facet_manager_notifier()->set_accuracy(NotificationAccuracy::TOO_LATE);

  const base::Time prefetch_end = Now() + kPrefetchLength;
  std::vector<ExpectedFetchDetails> expected_fetches(1);
  expected_fetches[0].time = Now();

  CreateFacetManager();
  Prefetch(prefetch_end);
  ASSERT_NO_FATAL_FAILURE(AdvanceTimeAndVerifyPrefetchWithFetchesAt(
      Now() + GetCacheSoftExpiryPeriod(), expected_fetches));

  for (int cycle = 0; cycle < 2; ++cycle) {
    for (base::TimeDelta step : SamplingPoints(GetShortTestPeriod())) {
      SCOPED_TRACE(testing::Message() << "dT: " << DeltaNow());
      EXPECT_FALSE(facet_manager()->CanBeDiscarded());
      ASSERT_TRUE(facet_manager()->DoesRequireFetch());
      ExpectRequestsServedFromCache();
      AdvanceTime(step);
    }

    ASSERT_NO_FATAL_FAILURE(ExpectFetchNeeded());
    CompleteFetch();

    const base::TimeDelta idle_period =
        cycle ? prefetch_end - Now() : GetCacheSoftExpiryPeriod();
    for (base::TimeDelta step : SamplingPoints(idle_period)) {
      SCOPED_TRACE(testing::Message() << "dT: " << DeltaNow());
      EXPECT_FALSE(facet_manager()->CanBeDiscarded());
      ExpectNoFetchNeeded();
      ExpectRequestsServedFromCache();
      AdvanceTime(step);
    }
  }

  ASSERT_EQ(kPrefetchLength, DeltaNow());
  EXPECT_TRUE(facet_manager()->CanBeDiscarded());
  AdvanceTime(GetShortTestPeriod());
  EXPECT_FALSE(main_task_runner()->HasPendingTask());
  DestroyFacetManager();
}

// RequestNotificationAtTime() ends up not calling NotifyAtRequestedTime() at
// all. This should result in SignalNeedNetworkRequest() not being called, but
// DoesRequireFetch() should be set as long as the prefetch is active.
TEST_F(FacetManagerTest, RequestedNotificationsNeverCome) {
  const base::TimeDelta kPrefetchLength =
      2 * GetCacheSoftExpiryPeriod() + GetCacheHardExpiryPeriod();

  facet_manager_notifier()->set_accuracy(NotificationAccuracy::NEVER_CALLED);

  const base::Time prefetch_end = Now() + kPrefetchLength;
  std::vector<ExpectedFetchDetails> expected_fetches(1);
  expected_fetches[0].time = Now();

  CreateFacetManager();
  Prefetch(prefetch_end);
  ASSERT_NO_FATAL_FAILURE(AdvanceTimeAndVerifyPrefetchWithFetchesAt(
      Now() + GetCacheSoftExpiryPeriod(), expected_fetches));
  for (base::TimeDelta step : SamplingPoints(prefetch_end - Now())) {
    SCOPED_TRACE(testing::Message() << "dT: " << DeltaNow());
    EXPECT_FALSE(facet_manager()->CanBeDiscarded());
    ASSERT_TRUE(facet_manager()->DoesRequireFetch());
    if (DeltaNow() < GetCacheHardExpiryPeriod())
      ExpectRequestsServedFromCache();
    AdvanceTime(step);
  }
  EXPECT_TRUE(facet_manager()->CanBeDiscarded());
  EXPECT_FALSE(main_task_runner()->HasPendingTask());
  DestroyFacetManager();
}

TEST_F(FacetManagerTest, StaleCachedDataBeCanDiscardedWhilePendingFetch) {
  CreateFacetManager();
  ASSERT_FALSE(facet_manager()->IsCachedDataFresh());

  GetAffiliationsAndBranding(StrategyOnCacheMiss::FETCH_OVER_NETWORK);
  ASSERT_NO_FATAL_FAILURE(ExpectFetchNeeded());
  EXPECT_FALSE(facet_manager()->CanBeDiscarded());
  EXPECT_TRUE(facet_manager()->CanCachedDataBeDiscarded());

  fake_facet_manager_host()->reset_need_network_request();
}

TEST_F(FacetManagerTest, CachedDataBeCanDiscardedAfterOnDemandGetAffiliatons) {
  CreateFacetManager();
  ASSERT_FALSE(facet_manager()->IsCachedDataFresh());

  GetAffiliationsAndBranding(StrategyOnCacheMiss::FETCH_OVER_NETWORK);
  ASSERT_NO_FATAL_FAILURE(ExpectFetchNeeded());
  ASSERT_NO_FATAL_FAILURE(CompleteFetch());
  ExpectConsumerSuccessCallback();

  EXPECT_TRUE(facet_manager()->IsCachedDataFresh());
  EXPECT_TRUE(facet_manager()->CanBeDiscarded());
  EXPECT_TRUE(facet_manager()->CanCachedDataBeDiscarded());
}

// The cached data can be discarded (indicated by 'd') if and only if it is no
// longer needed to be kept fresh, or if it already stale.
//
//      t=0                        S       H                  F2+S   F2+H
//      /                          /       /                  /      /
//  ---o--------------------------o-------o------------------o-------o------> t
//     :                          :       :
//     [F-------------------------NNNNNNNNNNNF---)
//                                        ddd    ddd...
//
TEST_F(FacetManagerTest,
       CachedDataCanBeDiscardedAfterAndSometimesDuringPrefetch) {
  CreateFacetManager();
  Prefetch(Now() + GetCacheHardExpiryPeriod() + 2 * GetShortTestPeriod());
  ASSERT_NO_FATAL_FAILURE(ExpectFetchNeeded());
  ASSERT_NO_FATAL_FAILURE(CompleteFetch());

  for (base::TimeDelta step : SamplingPoints(GetCacheHardExpiryPeriod())) {
    SCOPED_TRACE(testing::Message() << "dT: " << DeltaNow());
    EXPECT_FALSE(facet_manager()->CanCachedDataBeDiscarded());
    AdvanceTime(step);
  }

  for (base::TimeDelta step : SamplingPoints(GetShortTestPeriod())) {
    SCOPED_TRACE(testing::Message() << "dT: " << DeltaNow());
    EXPECT_TRUE(facet_manager()->CanCachedDataBeDiscarded());
    AdvanceTime(step);
  }

  ASSERT_NO_FATAL_FAILURE(ExpectFetchNeeded());
  ASSERT_NO_FATAL_FAILURE(CompleteFetch());

  for (base::TimeDelta step : SamplingPoints(GetShortTestPeriod())) {
    SCOPED_TRACE(testing::Message() << "dT: " << DeltaNow());
    EXPECT_FALSE(facet_manager()->CanCachedDataBeDiscarded());
    AdvanceTime(step);
  }

  EXPECT_TRUE(facet_manager()->CanBeDiscarded());
  EXPECT_TRUE(facet_manager()->CanCachedDataBeDiscarded());
}

TEST_F(FacetManagerTest, CachedDataBeCanDiscardedAfterCancelledPrefetch) {
  CreateFacetManager();
  Prefetch(base::Time::Max());
  ASSERT_NO_FATAL_FAILURE(ExpectFetchNeeded());
  ASSERT_NO_FATAL_FAILURE(CompleteFetch());

  EXPECT_FALSE(facet_manager()->CanCachedDataBeDiscarded());

  AdvanceTime(GetShortTestPeriod());
  CancelPrefetch(base::Time::Max());

  EXPECT_TRUE(facet_manager()->CanBeDiscarded());
  EXPECT_TRUE(facet_manager()->CanCachedDataBeDiscarded());
}

TEST_F(FacetManagerTest, GetAffiliationsAndBrandingOnceOverNetworkSuccess) {
  CreateFacetManager();
  EXPECT_FALSE(facet_manager()->IsCachedDataFresh());

  GetAffiliationsAndBranding(StrategyOnCacheMiss::TRY_ONCE_OVER_NETWORK);
  ASSERT_NO_FATAL_FAILURE(ExpectFetchNeeded());
  EXPECT_FALSE(facet_manager()->CanBeDiscarded());
  ASSERT_NO_FATAL_FAILURE(CompleteFetch());
  ExpectConsumerSuccessCallback();
}

TEST_F(FacetManagerTest, GetAffiliationsAndBrandingOnceOverNetworkFailure) {
  CreateFacetManager();
  EXPECT_FALSE(facet_manager()->IsCachedDataFresh());

  GetAffiliationsAndBranding(StrategyOnCacheMiss::TRY_ONCE_OVER_NETWORK);
  ASSERT_NO_FATAL_FAILURE(ExpectFetchNeeded());
  EXPECT_FALSE(facet_manager()->CanBeDiscarded());

  // Simulate failure.
  fake_facet_manager_host()->reset_need_network_request();
  facet_manager()->OnFetchFailed();
  main_task_runner()->RunUntilIdle();
  ASSERT_NO_FATAL_FAILURE(ExpectNoFetchNeeded());

  ExpectConsumerFailureCallback();
}

}  // namespace affiliations
