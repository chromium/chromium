// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/remote/remote_suggestions_scheduler_impl.h"

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "components/ntp_snippets/features.h"
#include "components/ntp_snippets/ntp_snippets_constants.h"
#include "components/ntp_snippets/pref_names.h"
#include "components/ntp_snippets/remote/persistent_scheduler.h"
#include "components/ntp_snippets/remote/remote_suggestions_provider.h"
#include "components/ntp_snippets/remote/test_utils.h"
#include "components/ntp_snippets/status.h"
#include "components/ntp_snippets/user_classifier.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/web_resource/web_resource_pref_names.h"
#include "net/base/mock_network_change_notifier.h"
#include "net/base/network_change_notifier.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::AnyNumber;
using testing::ElementsAre;
using testing::Eq;
using testing::Field;
using testing::InSequence;
using testing::Invoke;
using testing::InvokeArgument;
using testing::IsEmpty;
using testing::Mock;
using testing::MockFunction;
using testing::Not;
using testing::Return;
using testing::SaveArg;
using testing::SaveArgPointee;
using testing::SizeIs;
using testing::StartsWith;
using testing::StrictMock;
using testing::WithArgs;
using testing::_;

namespace ntp_snippets {

class RemoteSuggestionsFetcher;

namespace {

const int kDefaultStartupIntervalHours = 24;

ACTION_TEMPLATE(SaveArgByMove,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_1_VALUE_PARAMS(pointer)) {
  *pointer = std::move(*::testing::get<k>(args));
}

class MockPersistentScheduler : public PersistentScheduler {
 public:
  MOCK_METHOD2(Schedule,
               bool(base::TimeDelta period_wifi,
                    base::TimeDelta period_fallback));
  MOCK_METHOD0(Unschedule, bool());
  MOCK_METHOD0(IsOnUnmeteredConnection, bool());
};

// TODO(jkrcal): Move into its own library to reuse in other unit-tests?
class MockRemoteSuggestionsProvider : public RemoteSuggestionsProvider {
 public:
  MockRemoteSuggestionsProvider(Observer* observer)
      : RemoteSuggestionsProvider(observer) {}
  // Gmock cannot mock a method with movable-only type callback
  // FetchStatusCallback as a parameter. As a work-around, this function calls
  // the mock function with value passed by pointer. The mock function may then
  // be checked with EXPECT_CALL.
  void RefetchInTheBackground(
      RemoteSuggestionsProvider::FetchStatusCallback callback) override {
    RefetchInTheBackground(&callback);
  }
  MOCK_METHOD1(RefetchInTheBackground,
               void(RemoteSuggestionsProvider::FetchStatusCallback*));
  // Gmock cannot mock a method with movable-only type callback
  // FetchStatusCallback as a parameter. As a work-around, this function calls
  // the mock function with value passed by pointer. The mock function may then
  // be checked with EXPECT_CALL.
  void RefetchWhileDisplaying(
      RemoteSuggestionsProvider::FetchStatusCallback callback) override {
    RefetchWhileDisplaying(&callback);
  }
  MOCK_METHOD1(RefetchWhileDisplaying,
               void(RemoteSuggestionsProvider::FetchStatusCallback*));
  MOCK_CONST_METHOD0(suggestions_fetcher_for_debugging,
                     const RemoteSuggestionsFetcher*());
  MOCK_CONST_METHOD1(GetUrlWithFavicon,
                     GURL(const ContentSuggestion::ID& suggestion_id));
  MOCK_CONST_METHOD0(IsDisabled, bool());
  MOCK_CONST_METHOD0(ready, bool());
  MOCK_METHOD1(GetCategoryStatus, CategoryStatus(Category));
  MOCK_METHOD1(GetCategoryInfo, CategoryInfo(Category));
  MOCK_METHOD3(ClearHistory,
               void(base::Time begin,
                    base::Time end,
                    const base::Callback<bool(const GURL& url)>& filter));
  // Gmock cannot mock a method with movable-only type callback
  // FetchDoneCallback as a parameter. As a work-around, this function calls the
  // mock function with value passed by pointer. The mock function may then be
  // checked with EXPECT_CALL.
  void Fetch(const Category& category,
             const std::set<std::string>& set,
             FetchDoneCallback callback) override {
    Fetch(category, set, &callback);
  }
  MOCK_METHOD3(Fetch,
               void(const Category&,
                    const std::set<std::string>&,
                    FetchDoneCallback*));
  MOCK_METHOD0(ReloadSuggestions, void());
  MOCK_METHOD0(ClearCachedSuggestions, void());
  MOCK_METHOD1(ClearDismissedSuggestionsForDebugging, void(Category));
  MOCK_METHOD1(DismissSuggestion, void(const ContentSuggestion::ID&));
  // Gmock cannot mock a method with movable-only type callback
  // ImageFetchedCallback as a parameter. As a work-around, this function calls
  // the mock function with value passed by pointer. The mock function may then
  // be checked with EXPECT_CALL.
  void FetchSuggestionImage(const ContentSuggestion::ID& id,
                            ImageFetchedCallback callback) override {
    FetchSuggestionImage(id, &callback);
  }
  MOCK_METHOD2(FetchSuggestionImage,
               void(const ContentSuggestion::ID&, ImageFetchedCallback*));

  void FetchSuggestionImageData(const ContentSuggestion::ID& suggestion_id,
                                ImageDataFetchedCallback callback) override {
    FetchSuggestionImageData(suggestion_id, &callback);
  }
  MOCK_METHOD2(FetchSuggestionImageData,
               void(const ContentSuggestion::ID&, ImageDataFetchedCallback*));

  void GetDismissedSuggestionsForDebugging(
      Category category,
      DismissedSuggestionsCallback callback) override {
    GetDismissedSuggestionsForDebugging(category, &callback);
  }
  MOCK_METHOD2(GetDismissedSuggestionsForDebugging,
               void(Category category, DismissedSuggestionsCallback* callback));
  MOCK_METHOD1(OnSignInStateChanged, void(bool));
};

class FakeOfflineNetworkChangeNotifier {
 public:
  FakeOfflineNetworkChangeNotifier() {
    notifier_->SetConnectionType(net::NetworkChangeNotifier::CONNECTION_NONE);
  }

 private:
  std::unique_ptr<net::test::MockNetworkChangeNotifier> notifier_ =
      net::test::MockNetworkChangeNotifier::Create();
};

}  // namespace

class RemoteSuggestionsSchedulerImplTest : public ::testing::Test {
 public:
  RemoteSuggestionsSchedulerImplTest()
      :  // For the test we enabled all trigger types.
        default_variation_params_{{"scheduler_trigger_types",
                                   "persistent_scheduler_wake_up,ntp_opened,"
                                   "browser_foregrounded,browser_cold_start"}},
        user_classifier_(/*pref_service=*/nullptr,
                         base::DefaultClock::GetInstance()) {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        ntp_snippets::kArticleSuggestionsFeature, default_variation_params_);

    RemoteSuggestionsSchedulerImpl::RegisterProfilePrefs(
        utils_.pref_service()->registry());
    RequestThrottler::RegisterProfilePrefs(utils_.pref_service()->registry());
    // TODO(jkrcal) Create a static function in EulaAcceptedNotifier that
    // registers this pref and replace the call in browser_process_impl.cc & in
    // eula_accepted_notifier_unittest.cc with the new static function.
    local_state_.registry()->RegisterBooleanPref(::prefs::kEulaAccepted, false);
    // By default pretend we are on WiFi.
    EXPECT_CALL(*persistent_scheduler(), IsOnUnmeteredConnection())
        .WillRepeatedly(Return(true));
    ResetProvider();
  }

  void ResetProvider() {
    provider_ = std::make_unique<StrictMock<MockRemoteSuggestionsProvider>>(
        /*observer=*/nullptr);

    test_clock_.SetNow(base::Time::Now());

    scheduler_ = std::make_unique<RemoteSuggestionsSchedulerImpl>(
        &persistent_scheduler_, &user_classifier_, utils_.pref_service(),
        &local_state_, &test_clock_);
    scheduler_->SetProvider(provider_.get());
  }

  void SetVariationParameter(const std::string& param_name,
                             const std::string& param_value) {
    std::map<std::string, std::string> params = default_variation_params_;
    params[param_name] = param_value;

    scoped_feature_list_.Reset();
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        ntp_snippets::kArticleSuggestionsFeature, params);
  }

  bool IsEulaNotifierAvailable() {
    // Create() returns a unique_ptr, so this is no leak.
    return web_resource::EulaAcceptedNotifier::Create(&local_state_) != nullptr;
  }

  void SetEulaAcceptedPref() {
    local_state_.SetBoolean(::prefs::kEulaAccepted, true);
  }

  // GMock cannot deal with move-only types. We need to pass the vector to the
  // mock function as const ref using this wrapper callback.
  void FetchDoneWrapper(
      MockFunction<void(Status status_code,
                        const std::vector<ContentSuggestion>& suggestions)>*
          fetch_done,
      Status status_code,
      std::vector<ContentSuggestion> suggestions) {
    fetch_done->Call(status_code, suggestions);
  }

 protected:
  std::map<std::string, std::string> default_variation_params_;
  base::test::ScopedFeatureList scoped_feature_list_;

  void ActivateProviderAndEula() {
    SetEulaAcceptedPref();
    EXPECT_CALL(*provider(), ready()).WillRepeatedly(Return(true));
    scheduler_->OnProviderActivated();
  }

  void ActivateProvider() {
    EXPECT_CALL(*provider(), ready()).WillRepeatedly(Return(true));
    scheduler_->OnProviderActivated();
  }

  void DeactivateProvider() {
    scheduler_->OnProviderDeactivated();
    EXPECT_CALL(*provider(), ready()).WillRepeatedly(Return(true));
  }

  void ExpectOneRetiringRefetchInTheBackground() {
    // After a successful fetch, the client updates it's schedule, so we expect
    // another call here.
    EXPECT_CALL(*persistent_scheduler(), Schedule(_, _)).RetiresOnSaturation();
    EXPECT_CALL(*provider(), RefetchInTheBackground(_))
        .WillOnce(Invoke(
            [](RemoteSuggestionsProvider::FetchStatusCallback* callback) {
              std::move(*callback).Run(Status::Success());
            }))
        .RetiresOnSaturation();
  }

  MockPersistentScheduler* persistent_scheduler() {
    return &persistent_scheduler_;
  }

  base::SimpleTestClock* test_clock() { return &test_clock_; }
  MockRemoteSuggestionsProvider* provider() { return provider_.get(); }
  RemoteSuggestionsSchedulerImpl* scheduler() { return scheduler_.get(); }

 private:
  test::RemoteSuggestionsTestUtils utils_;
  UserClassifier user_classifier_;
  TestingPrefServiceSimple local_state_;
  StrictMock<MockPersistentScheduler> persistent_scheduler_;
  base::SimpleTestClock test_clock_;
  std::unique_ptr<MockRemoteSuggestionsProvider> provider_;
  std::unique_ptr<RemoteSuggestionsSchedulerImpl> scheduler_;

  DISALLOW_COPY_AND_ASSIGN(RemoteSuggestionsSchedulerImplTest);
};

TEST_F(RemoteSuggestionsSchedulerImplTest, ShouldIgnoreSignalsWhenNotEnabled) {
  // The signals should be ignored even if the provider itself claims it is
  // ready.
  EXPECT_CALL(*provider(), ready()).WillRepeatedly(Return(true));

  scheduler()->OnPersistentSchedulerWakeUp();
  scheduler()->OnSuggestionsSurfaceOpened();
  scheduler()->OnBrowserForegrounded();
  scheduler()->OnBrowserColdStart();
}

TEST_F(RemoteSuggestionsSchedulerImplTest,
       ShouldIgnoreEulaStateOnPlatformsWhereNotAvaiable) {
  // Only run this tests on platforms that don't support Eula.
  if (IsEulaNotifierAvailable()) {
    return;
  }

  // Activating the provider should schedule the persistent background fetches.
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _));
  ActivateProvider();

  // Verify fetches get triggered.
  ExpectOneRetiringRefetchInTheBackground();
  scheduler()->OnPersistentSchedulerWakeUp();
}

TEST_F(RemoteSuggestionsSchedulerImplTest,
       ShouldIgnoreSignalsWhenEulaNotAccepted) {
  // Only run this tests on platforms supporting Eula.
  if (!IsEulaNotifierAvailable()) {
    return;
  }
  // Activating the provider should schedule the persistent background fetches.
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _));
  ActivateProvider();

  // All signals are ignored because of Eula not being accepted.
  scheduler()->OnPersistentSchedulerWakeUp();
  scheduler()->OnSuggestionsSurfaceOpened();
  scheduler()->OnBrowserForegrounded();
  scheduler()->OnBrowserColdStart();
}

TEST_F(RemoteSuggestionsSchedulerImplTest, ShouldFetchWhenEulaGetsAccepted) {
  // Only run this tests on platforms supporting Eula.
  if (!IsEulaNotifierAvailable()) {
    return;
  }
  // Activating the provider should schedule the persistent background fetches.
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _));
  ActivateProvider();

  // Make one (ignored) call to make sure we are interested in eula state.
  scheduler()->OnPersistentSchedulerWakeUp();

  // Accepting Eula afterwards results in a background fetch.
  ExpectOneRetiringRefetchInTheBackground();
  SetEulaAcceptedPref();
}

TEST_F(RemoteSuggestionsSchedulerImplTest,
       ShouldQueueBackgroundRequestIfEulaIsMissing) {
  // Only run this tests on platforms supporting Eula.
  if (!IsEulaNotifierAvailable()) {
    return;
  }
  // Eula is not ready -- no fetch. But request should get queued.
  EXPECT_CALL(*provider(), ready()).WillRepeatedly(Return(false));
  scheduler()->OnPersistentSchedulerWakeUp();
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _));
  // Activate provider -- this should set up the schedule but cannot trigger a
  // fetch due to Eula missing.
  ActivateProvider();

  // Accepting Eula picks up the queued fetch.
  ExpectOneRetiringRefetchInTheBackground();
  SetEulaAcceptedPref();
}

TEST_F(RemoteSuggestionsSchedulerImplTest,
       ShouldQueueBackgroundRequestBeforeActivated) {
  // Set the Eula bit to be sure we queue them because of not being activated.
  SetEulaAcceptedPref();

  // Activate provider -- this should set up the schedule and store it to prefs.
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _)).RetiresOnSaturation();
  ActivateProvider();

  // Reset the provider after we have the schedule stored in prefs.
  ResetProvider();

  // Provider is not active -- no fetch. But request should get queued.
  EXPECT_CALL(*provider(), ready()).WillRepeatedly(Return(false));
  scheduler()->OnPersistentSchedulerWakeUp();

  ExpectOneRetiringRefetchInTheBackground();
  // Activate provider -- this should trigger the fetch (the schedule was set up
  // previously).
  ActivateProvider();
}

TEST_F(RemoteSuggestionsSchedulerImplTest,
       ShouldIgnoreSignalsWhenDisabledByParam) {
  // First set an empty list of allowed trigger types.
  SetVariationParameter("scheduler_trigger_types", "-");
  ResetProvider();

  // Then enable the scheduler.
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _));
  ActivateProviderAndEula();

  scheduler()->OnPersistentSchedulerWakeUp();
  scheduler()->OnSuggestionsSurfaceOpened();
  scheduler()->OnBrowserForegrounded();
  scheduler()->OnBrowserColdStart();
}

TEST_F(RemoteSuggestionsSchedulerImplTest,
       ShouldHandleEmptyParamForTriggerTypes) {
  // First set an empty param for allowed trigger types -> should result in the
  // default list.
  SetVariationParameter("scheduler_trigger_types", "");
  ResetProvider();

  // Then enable the scheduler.
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _));
  ActivateProviderAndEula();

  // For instance, persistent scheduler wake up should be enabled by default.
  ExpectOneRetiringRefetchInTheBackground();
  scheduler()->OnPersistentSchedulerWakeUp();
}

TEST_F(RemoteSuggestionsSchedulerImplTest,
       ShouldHandleIncorrentParamForTriggerTypes) {
  // First set an invalid list of allowed trigger types.
  SetVariationParameter("scheduler_trigger_types", "ntp_opened,foo;");
  ResetProvider();

  // Then enable the scheduler.
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _));
  ActivateProviderAndEula();

  // For instance, persistent scheduler wake up should be enabled by default.
  ExpectOneRetiringRefetchInTheBackground();
  scheduler()->OnPersistentSchedulerWakeUp();
}

TEST_F(RemoteSuggestionsSchedulerImplTest,
       ShouldFetchOnPersistentSchedulerWakeUp) {
  // First set only this type to be allowed.
  SetVariationParameter("scheduler_trigger_types",
                        "persistent_scheduler_wake_up");
  ResetProvider();

  // Then enable the scheduler.
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _));
  ActivateProviderAndEula();

  ExpectOneRetiringRefetchInTheBackground();
  scheduler()->OnPersistentSchedulerWakeUp();
}

TEST_F(RemoteSuggestionsSchedulerImplTest,
       ShouldFetchOnPersistentSchedulerWakeUpRepeated) {
  RemoteSuggestionsProvider::FetchStatusCallback signal_fetch_done;
  {
    InSequence s;
    EXPECT_CALL(*persistent_scheduler(), Schedule(_, _));
    EXPECT_CALL(*provider(), RefetchInTheBackground(_))
        .WillOnce(SaveArgByMove<0>(&signal_fetch_done));
    EXPECT_CALL(*persistent_scheduler(), Schedule(_, _));
    EXPECT_CALL(*provider(), RefetchInTheBackground(_));
  }
  // First enable the scheduler -- calling Schedule() for the first time.
  ActivateProviderAndEula();
  // Make the first persistent fetch successful -- calling Schedule() again.
  scheduler()->OnPersistentSchedulerWakeUp();
  std::move(signal_fetch_done).Run(Status::Success());
  // Make the second fetch.
  scheduler()->OnPersistentSchedulerWakeUp();
}

TEST_F(RemoteSuggestionsSchedulerImplTest,
       ShouldNotTriggerBackgroundFetchIfAlreadyInProgess) {
  {
    InSequence s;
    EXPECT_CALL(*persistent_scheduler(), Schedule(_, _));
    // We do not capture and execute the callback to keep the fetch in-flight.
    EXPECT_CALL(*provider(), RefetchInTheBackground(_));
    // Refetch is not called after the second trigger.
  }
  // First enable the scheduler -- calling Schedule() for the first time.
  ActivateProviderAndEula();
  // Make the first persistent fetch never finish.
  scheduler()->OnPersistentSchedulerWakeUp();
  // Make the second fetch.
  scheduler()->OnPersistentSchedulerWakeUp();
}

TEST_F(RemoteSuggestionsSchedulerImplTest,
       ShouldFetchOnSuggestionsSurfaceOpenedForTheFirstTime) {
  // First set only this type to be allowed.
  SetVariationParameter("scheduler_trigger_types", "ntp_opened");
  ResetProvider();

  // Then enable the scheduler.
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _));
  ActivateProviderAndEula();

  ExpectOneRetiringRefetchInTheBackground();
  scheduler()->OnSuggestionsSurfaceOpened();
}

TEST_F(RemoteSuggestionsSchedulerImplTest,
       ShouldFetchOnBrowserForegroundedForTheFirstTime) {
  // First set only this type to be allowed.
  SetVariationParameter("scheduler_trigger_types", "browser_foregrounded");
  ResetProvider();

  // Then enable the scheduler.
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _));
  ActivateProviderAndEula();

  ExpectOneRetiringRefetchInTheBackground();
  scheduler()->OnBrowserForegrounded();
}

TEST_F(RemoteSuggestionsSchedulerImplTest,
       ShouldFetchOnBrowserColdStartForTheFirstTime) {
  // Then enable the scheduler.
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _));
  ActivateProviderAndEula();

  ExpectOneRetiringRefetchInTheBackground();
  scheduler()->OnBrowserColdStart();
}

TEST_F(RemoteSuggestionsSchedulerImplTest,
       ShouldQueueBackgroundFetchSignalsOnPersistentSchedulerWakeUp) {
  // Enable EULA to make this test not depend on that setting (or it being
  // flipped)
  SetEulaAcceptedPref();

  // On activation, the Schedule should get updated and the queued background
  // fetch should get propagated.
  EXPECT_CALL(*provider(), ready()).WillRepeatedly(Return(false));
  scheduler()->OnPersistentSchedulerWakeUp();
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _));
  ExpectOneRetiringRefetchInTheBackground();
  ActivateProviderAndEula();
}

TEST_F(RemoteSuggestionsSchedulerImplTest,
       ShouldQueueBackgroundFetchSignalsOnSurfaceOpened) {
  // Enable EULA to make this test not depend on that setting (or it being
  // flipped)
  SetEulaAcceptedPref();

  // On activation, the Schedule should get updated and the queued background
  // fetch should get propagated.
  EXPECT_CALL(*provider(), ready()).WillRepeatedly(Return(false));
  scheduler()->OnSuggestionsSurfaceOpened();
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _));
  ExpectOneRetiringRefetchInTheBackground();
  ActivateProviderAndEula();
}

TEST_F(RemoteSuggestionsSchedulerImplTest,
       ShouldQueueBackgroundFetchSignalsOnBrowserForegrounded) {
  // Enable EULA to make this test not depend on that setting (or it being
  // flipped)
  SetEulaAcceptedPref();

  // On activation, the Schedule should get updated and the queued background
  // fetch should get propagated.
  EXPECT_CALL(*provider(), ready()).WillRepeatedly(Return(false));
  scheduler()->OnBrowserForegrounded();
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _));
  ExpectOneRetiringRefetchInTheBackground();
  ActivateProviderAndEula();
}

TEST_F(RemoteSuggestionsSchedulerImplTest,
       ShouldQueueBackgroundFetchSignalsOnBrowserColdStart) {
  // Enable EULA to make this test not depend on that setting (or it being
  // flipped)
  SetEulaAcceptedPref();

  // On activation, the Schedule should get updated and the queued background
  // fetch should get propagated.
  EXPECT_CALL(*provider(), ready()).WillRepeatedly(Return(false));
  scheduler()->OnBrowserColdStart();
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _));
  ExpectOneRetiringRefetchInTheBackground();
  ActivateProviderAndEula();
}

TEST_F(RemoteSuggestionsSchedulerImplTest,
       ShouldQueueMultipleBackgroundFetchSignals) {
  // Enable EULA to make this test not depend on that setting (or it being
  // flipped)
  SetEulaAcceptedPref();

  // We want to store multiple events to respect lower thresholds for specific
  // events properly. To test this, we do the following setup:
  // (1) Force a fetch.
  // (2) Simulate a stop of the browser, wait until surface-opened would trigger
  // a fetch but start-up would not trigger a fetch yet.
  // (3) simulate a very slow initialization, where the scheduler sees both
  // events (cold start and surface opened) before being activated.
  // (4) make sure that activation triggers a background fetch.
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _)).Times(AnyNumber());
  ActivateProviderAndEula();
  ExpectOneRetiringRefetchInTheBackground();
  scheduler()->OnBrowserColdStart();

  // UserClassifier defaults to UserClass::ACTIVE_NTP_USER - we work with the
  // default interval for this class here. This time would allow for a fetch on
  // NTP open but not on cold start.
  test_clock()->Advance(base::TimeDelta::FromHours(13));
  // This should *not* trigger a fetch.
  scheduler()->OnBrowserColdStart();

  // Simulate a restart.
  EXPECT_CALL(*persistent_scheduler(), Unschedule());
  scheduler()->OnProviderDeactivated();
  ResetProvider();  // Also resets the scheduler and test clock.

  test_clock()->Advance(base::TimeDelta::FromHours(13));
  EXPECT_CALL(*provider(), ready()).WillRepeatedly(Return(false));
  scheduler()->OnSuggestionsSurfaceOpened();
  scheduler()->OnBrowserColdStart();
  ExpectOneRetiringRefetchInTheBackground();

  // Signal the provider is ready (EULA check should still pass from the first
  // start). We don't want to trigger EULA again as it will simulate a
  // persistent fetch.
  ActivateProviderAndEula();
}

TEST_F(RemoteSuggestionsSchedulerImplTest,
       ShouldNotFetchOnSuggestionsSurfaceOpenedAfterSuccessfulSoftFetch) {
  // First enable the scheduler; the second Schedule is called after the
  // successful fetch.
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _)).Times(2);
  ActivateProviderAndEula();

  // Make the first soft fetch successful.
  RemoteSuggestionsProvider::FetchStatusCallback signal_fetch_done;
  EXPECT_CALL(*provider(), RefetchInTheBackground(_))
      .WillOnce(SaveArgByMove<0>(&signal_fetch_done));
  scheduler()->OnSuggestionsSurfaceOpened();
  std::move(signal_fetch_done).Run(Status::Success());
  // The second call is ignored if it happens right after the first one.
  scheduler()->OnSuggestionsSurfaceOpened();
}

TEST_F(RemoteSuggestionsSchedulerImplTest,
       ShouldNotFetchOnSuggestionsSurfaceOpenedAfterSuccessfulPersistentFetch) {
  // First enable the scheduler; the second Schedule is called after the
  // successful fetch.
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _)).Times(2);
  ActivateProviderAndEula();

  // Make the first persistent fetch successful.
  RemoteSuggestionsProvider::FetchStatusCallback signal_fetch_done;
  EXPECT_CALL(*provider(), RefetchInTheBackground(_))
      .WillOnce(SaveArgByMove<0>(&signal_fetch_done));
  scheduler()->OnPersistentSchedulerWakeUp();
  std::move(signal_fetch_done).Run(Status::Success());
  // The second call is ignored if it happens right after the first one.
  scheduler()->OnSuggestionsSurfaceOpened();
}

TEST_F(RemoteSuggestionsSchedulerImplTest,
       ShouldNotFetchOnSuggestionsSurfaceOpenedAfterFailedSoftFetch) {
  // First enable the scheduler.
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _));
  ActivateProviderAndEula();

  // Make the first soft fetch failed.
  RemoteSuggestionsProvider::FetchStatusCallback signal_fetch_done;
  EXPECT_CALL(*provider(), RefetchInTheBackground(_))
      .WillOnce(SaveArgByMove<0>(&signal_fetch_done));
  scheduler()->OnSuggestionsSurfaceOpened();
  std::move(signal_fetch_done).Run(Status(StatusCode::PERMANENT_ERROR, ""));

  // The second call is ignored if it happens right after the first one.
  scheduler()->OnSuggestionsSurfaceOpened();
}

TEST_F(RemoteSuggestionsSchedulerImplTest,
       ShouldNotFetchOnSuggestionsSurfaceOpenedAfterFailedPersistentFetch) {
  // First enable the scheduler.
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _));
  ActivateProviderAndEula();

  // Make the first persistent fetch failed.
  RemoteSuggestionsProvider::FetchStatusCallback signal_fetch_done;
  EXPECT_CALL(*provider(), RefetchInTheBackground(_))
      .WillOnce(SaveArgByMove<0>(&signal_fetch_done));
  scheduler()->OnPersistentSchedulerWakeUp();
  std::move(signal_fetch_done).Run(Status(StatusCode::PERMANENT_ERROR, ""));

  // The second call is ignored if it happens right after the first one.
  scheduler()->OnSuggestionsSurfaceOpened();
}

TEST_F(RemoteSuggestionsSchedulerImplTest,
       ShouldFetchAgainOnBrowserForgroundLaterAgain) {
  RemoteSuggestionsProvider::FetchStatusCallback signal_fetch_done;
  {
    InSequence s;
    // Initial scheduling after being enabled.
    EXPECT_CALL(*persistent_scheduler(), Schedule(_, _));
    // The first call to NTPOpened results in a fetch.
    EXPECT_CALL(*provider(), RefetchInTheBackground(_))
        .WillOnce(SaveArgByMove<0>(&signal_fetch_done));
    // Rescheduling after a succesful fetch.
    EXPECT_CALL(*persistent_scheduler(), Schedule(_, _));
    // The second call to NTPOpened 4hrs later again results in a fetch.
    EXPECT_CALL(*provider(), RefetchInTheBackground(_));
  }

  // First enable the scheduler.
  ActivateProviderAndEula();
  // Make the first soft fetch successful.
  scheduler()->OnBrowserForegrounded();
  std::move(signal_fetch_done).Run(Status::Success());
  test_clock()->Advance(
      base::TimeDelta::FromHours(kDefaultStartupIntervalHours));
  scheduler()->OnBrowserForegrounded();
}

TEST_F(RemoteSuggestionsSchedulerImplTest, ShouldRescheduleOnBrowserUpgraded) {
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _));
  scheduler()->OnBrowserUpgraded();
}

TEST_F(RemoteSuggestionsSchedulerImplTest, ShouldScheduleOnActivation) {
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _));
  ActivateProviderAndEula();
}

TEST_F(RemoteSuggestionsSchedulerImplTest,
       ShouldUnscheduleOnLaterInactivation) {
  {
    InSequence s;
    EXPECT_CALL(*persistent_scheduler(), Schedule(_, _));
    EXPECT_CALL(*persistent_scheduler(), Unschedule());
  }
  ActivateProviderAndEula();
  DeactivateProvider();
}

TEST_F(RemoteSuggestionsSchedulerImplTest, ShouldScheduleOnLaterActivation) {
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _));
  // There is no schedule yet, so inactivation does not trigger unschedule.
  DeactivateProvider();
  ActivateProviderAndEula();
}

TEST_F(RemoteSuggestionsSchedulerImplTest,
       ShouldRescheduleAfterSuccessfulFetch) {
  // First reschedule on becoming active.
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _)).Times(2);
  ActivateProviderAndEula();

  RemoteSuggestionsProvider::FetchStatusCallback signal_fetch_done;
  EXPECT_CALL(*provider(), RefetchInTheBackground(_))
      .WillOnce(SaveArgByMove<0>(&signal_fetch_done));

  // Trigger a fetch.
  scheduler()->OnPersistentSchedulerWakeUp();
  // Second reschedule after a successful fetch.
  std::move(signal_fetch_done).Run(Status::Success());
}

TEST_F(RemoteSuggestionsSchedulerImplTest,
       ShouldNotRescheduleAfterFailedFetch) {
  // Only reschedule on becoming active.
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _));
  ActivateProviderAndEula();

  RemoteSuggestionsProvider::FetchStatusCallback signal_fetch_done;
  EXPECT_CALL(*provider(), RefetchInTheBackground(_))
      .WillOnce(SaveArgByMove<0>(&signal_fetch_done));

  // Trigger a fetch.
  scheduler()->OnPersistentSchedulerWakeUp();
  // No furter reschedule after a failure.
  std::move(signal_fetch_done).Run(Status(StatusCode::PERMANENT_ERROR, ""));
}

TEST_F(RemoteSuggestionsSchedulerImplTest, ShouldScheduleOnlyOnce) {
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _));
  ActivateProviderAndEula();
  // No further call to Schedule on a second status callback.
  ActivateProviderAndEula();
}

TEST_F(RemoteSuggestionsSchedulerImplTest, ShouldUnscheduleOnlyOnce) {
  {
    InSequence s;
    EXPECT_CALL(*persistent_scheduler(), Schedule(_, _));
    EXPECT_CALL(*persistent_scheduler(), Unschedule());
  }
  // First schedule so that later we really unschedule.
  ActivateProviderAndEula();
  DeactivateProvider();
  // No further call to Unschedule on second status callback.
  DeactivateProvider();
}

TEST_F(RemoteSuggestionsSchedulerImplTest,
       ReschedulesWhenPersistentWifiParamChanges) {
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _)).Times(2);
  ActivateProviderAndEula();

  // UserClassifier defaults to UserClass::ACTIVE_NTP_USER if PrefService is
  // null. Change the wifi interval for this class.
  SetVariationParameter("fetching_interval_hours-wifi-active_ntp_user", "1.5");

  // Schedule() should get called for the second time after params have changed.
  ActivateProviderAndEula();
}

TEST_F(RemoteSuggestionsSchedulerImplTest,
       ReschedulesWhenPersistentFallbackParamChanges) {
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _)).Times(2);
  ActivateProviderAndEula();

  // UserClassifier defaults to UserClass::ACTIVE_NTP_USER if PrefService is
  // null. Change the fallback interval for this class.
  SetVariationParameter("fetching_interval_hours-fallback-active_ntp_user",
                        "1.5");

  // Schedule() should get called for the second time after params have changed.
  ActivateProviderAndEula();
}

TEST_F(RemoteSuggestionsSchedulerImplTest,
       ReschedulesWhenShownWifiParamChanges) {
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _)).Times(2);
  ActivateProviderAndEula();

  // UserClassifier defaults to UserClass::ACTIVE_NTP_USER if PrefService is
  // null. Change the on usage interval for this class.
  SetVariationParameter("soft_fetching_interval_hours-wifi-active_ntp_user",
                        "1.5");

  // Schedule() should get called for the second time after params have changed.
  ActivateProviderAndEula();
}

TEST_F(RemoteSuggestionsSchedulerImplTest,
       ReschedulesWhenShownFallbackParamChanges) {
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _)).Times(2);
  ActivateProviderAndEula();

  // UserClassifier defaults to UserClass::ACTIVE_NTP_USER if PrefService is
  // null. Change the fallback interval for this class.
  SetVariationParameter("soft_fetching_interval_hours-fallback-active_ntp_user",
                        "1.5");

  // Schedule() should get called for the second time after params have changed.
  ActivateProviderAndEula();
}

TEST_F(RemoteSuggestionsSchedulerImplTest,
       ReschedulesWhenStartupWifiParamChanges) {
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _)).Times(2);
  ActivateProviderAndEula();

  // UserClassifier defaults to UserClass::ACTIVE_NTP_USER if PrefService is
  // null. Change the on usage interval for this class.
  SetVariationParameter("startup_fetching_interval_hours-wifi-active_ntp_user",
                        "1.5");

  // Schedule() should get called for the second time after params have changed.
  ActivateProviderAndEula();
}

TEST_F(RemoteSuggestionsSchedulerImplTest,
       ReschedulesWhenStartupFallbackParamChanges) {
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _)).Times(2);
  ActivateProviderAndEula();

  // UserClassifier defaults to UserClass::ACTIVE_NTP_USER if PrefService is
  // null. Change the fallback interval for this class.
  SetVariationParameter(
      "startup_fetching_interval_hours-fallback-active_ntp_user", "1.5");

  // Schedule() should get called for the second time after params have changed.
  ActivateProviderAndEula();
}

TEST_F(RemoteSuggestionsSchedulerImplTest, FetchIntervalForShownTriggerOnWifi) {
  // Pretend we are on WiFi (already done in ctor, we make it explicit here).
  EXPECT_CALL(*persistent_scheduler(), IsOnUnmeteredConnection())
      .WillRepeatedly(Return(true));

  // Initial scheduling after being enabled.
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _));
  ActivateProviderAndEula();

  // The first call to NTPOpened results in a fetch.
  RemoteSuggestionsProvider::FetchStatusCallback signal_fetch_done;
  EXPECT_CALL(*provider(), RefetchInTheBackground(_))
      .WillOnce(SaveArgByMove<0>(&signal_fetch_done));
  scheduler()->OnSuggestionsSurfaceOpened();
  // Rescheduling after a succesful fetch.
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _));
  std::move(signal_fetch_done).Run(Status::Success());

  // Open NTP again after too short delay (one minute missing). UserClassifier
  // defaults to UserClass::ACTIVE_NTP_USER - we work with the default interval
  // for this class here. This time no fetch is executed.
  test_clock()->Advance(base::TimeDelta::FromHours(4) -
                        base::TimeDelta::FromMinutes(1));
  scheduler()->OnSuggestionsSurfaceOpened();

  // Open NTP after another delay, now together long enough to issue a fetch.
  test_clock()->Advance(base::TimeDelta::FromMinutes(2));
  EXPECT_CALL(*provider(), RefetchInTheBackground(_));
  scheduler()->OnSuggestionsSurfaceOpened();
}

TEST_F(RemoteSuggestionsSchedulerImplTest,
       OverrideFetchIntervalForShownTriggerOnWifi) {
  // Pretend we are on WiFi (already done in ctor, we make it explicit here).
  EXPECT_CALL(*persistent_scheduler(), IsOnUnmeteredConnection())
      .WillRepeatedly(Return(true));
  // UserClassifier defaults to UserClass::ACTIVE_NTP_USER if PrefService is
  // null. Change the interval for this class from 4h to 30min.
  SetVariationParameter("soft_fetching_interval_hours-wifi-active_ntp_user",
                        "0.5");

  // Initial scheduling after being enabled.
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _));
  ActivateProviderAndEula();

  // The first call to NTPOpened results in a fetch.
  RemoteSuggestionsProvider::FetchStatusCallback signal_fetch_done;
  EXPECT_CALL(*provider(), RefetchInTheBackground(_))
      .WillOnce(SaveArgByMove<0>(&signal_fetch_done));
  scheduler()->OnSuggestionsSurfaceOpened();
  // Rescheduling after a succesful fetch.
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _));
  std::move(signal_fetch_done).Run(Status::Success());

  // Open NTP again after too short delay. This time no fetch is executed.
  test_clock()->Advance(base::TimeDelta::FromMinutes(20));
  scheduler()->OnSuggestionsSurfaceOpened();

  // Open NTP after another delay, now together long enough to issue a fetch.
  test_clock()->Advance(base::TimeDelta::FromMinutes(10));
  EXPECT_CALL(*provider(), RefetchInTheBackground(_));
  scheduler()->OnSuggestionsSurfaceOpened();
}

TEST_F(RemoteSuggestionsSchedulerImplTest,
       FetchIntervalForShownTriggerOnFallback) {
  // Pretend we are not on wifi -> fallback connection.
  EXPECT_CALL(*persistent_scheduler(), IsOnUnmeteredConnection())
      .WillRepeatedly(Return(false));
  // UserClassifier defaults to UserClass::ACTIVE_NTP_USER which uses a 12h time
  // interval by default for shown trigger not on WiFi.

  // Initial scheduling after being enabled.
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _));
  ActivateProviderAndEula();

  // The first call to NTPOpened results in a fetch.
  RemoteSuggestionsProvider::FetchStatusCallback signal_fetch_done;
  EXPECT_CALL(*provider(), RefetchInTheBackground(_))
      .WillOnce(SaveArgByMove<0>(&signal_fetch_done));
  scheduler()->OnSuggestionsSurfaceOpened();
  // Rescheduling after a succesful fetch.
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _));
  std::move(signal_fetch_done).Run(Status::Success());

  // Open NTP again after too short delay. This time no fetch is executed.
  test_clock()->Advance(base::TimeDelta::FromHours(4) -
                        base::TimeDelta::FromMinutes(1));
  scheduler()->OnSuggestionsSurfaceOpened();

  // Open NTP after another delay, now together long enough to issue a fetch.
  test_clock()->Advance(base::TimeDelta::FromMinutes(2));
  EXPECT_CALL(*provider(), RefetchInTheBackground(_));
  scheduler()->OnSuggestionsSurfaceOpened();
}

TEST_F(RemoteSuggestionsSchedulerImplTest,
       OverrideFetchIntervalForShownTriggerOnFallback) {
  // Pretend we are not on wifi -> fallback connection.
  EXPECT_CALL(*persistent_scheduler(), IsOnUnmeteredConnection())
      .WillRepeatedly(Return(false));
  // UserClassifier defaults to UserClass::ACTIVE_NTP_USER if PrefService is
  // null. Change the interval for this class from 4h to 30min.
  SetVariationParameter("soft_fetching_interval_hours-fallback-active_ntp_user",
                        "0.5");

  // Initial scheduling after being enabled.
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _));
  ActivateProviderAndEula();

  // The first call to NTPOpened results in a fetch.
  RemoteSuggestionsProvider::FetchStatusCallback signal_fetch_done;
  EXPECT_CALL(*provider(), RefetchInTheBackground(_))
      .WillOnce(SaveArgByMove<0>(&signal_fetch_done));
  scheduler()->OnSuggestionsSurfaceOpened();
  // Rescheduling after a succesful fetch.
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _));
  std::move(signal_fetch_done).Run(Status::Success());

  // Open NTP again after too short delay. This time no fetch is executed.
  test_clock()->Advance(base::TimeDelta::FromMinutes(20));
  scheduler()->OnSuggestionsSurfaceOpened();

  // Open NTP after another delay, now together long enough to issue a fetch.
  test_clock()->Advance(base::TimeDelta::FromMinutes(10));
  EXPECT_CALL(*provider(), RefetchInTheBackground(_));
  scheduler()->OnSuggestionsSurfaceOpened();
}

TEST_F(RemoteSuggestionsSchedulerImplTest,
       ShouldBlockFetchingForSomeTimeAfterHistoryCleared) {
  // First enable the scheduler -- this will trigger the persistent scheduling.
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _));
  ActivateProviderAndEula();
  // Clear the history.
  scheduler()->OnHistoryCleared();

  // A trigger after 15 minutes is ignored.
  test_clock()->Advance(base::TimeDelta::FromMinutes(15));
  scheduler()->OnBrowserForegrounded();

  // A trigger after another 16 minutes is performed (more than 30m after
  // clearing the history).
  EXPECT_CALL(*provider(), RefetchWhileDisplaying(_));
  test_clock()->Advance(base::TimeDelta::FromMinutes(16));
  scheduler()->OnBrowserForegrounded();
}

TEST_F(RemoteSuggestionsSchedulerImplTest,
       ShouldImmediatelyFetchAfterSuggestionsCleared) {
  RemoteSuggestionsProvider::FetchStatusCallback signal_fetch_done;

  // First enable the scheduler -- this will trigger the persistent scheduling.
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _));
  ActivateProviderAndEula();

  // The first trigger results in a fetch.
  EXPECT_CALL(*provider(), RefetchInTheBackground(_))
      .WillOnce(SaveArgByMove<0>(&signal_fetch_done));
  scheduler()->OnBrowserForegrounded();
  // Make the fetch successful -- this results in rescheduling.
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _));
  std::move(signal_fetch_done).Run(Status::Success());

  // Clear the suggestions - results in an immediate fetch.
  EXPECT_CALL(*provider(), RefetchWhileDisplaying(_));
  scheduler()->OnSuggestionsCleared();
}

TEST_F(RemoteSuggestionsSchedulerImplTest, ShouldThrottleInteractiveRequests) {
  // Change the quota for interactive requests ("active NTP user" is the default
  // class in tests).
  SetVariationParameter("interactive_quota_SuggestionFetcherActiveNTPUser",
                        "10");
  ResetProvider();

  for (int x = 0; x < 10; ++x) {
    EXPECT_THAT(scheduler()->AcquireQuotaForInteractiveFetch(), Eq(true));
  }

  // Now the quota is over.
  EXPECT_THAT(scheduler()->AcquireQuotaForInteractiveFetch(), Eq(false));
}

TEST_F(RemoteSuggestionsSchedulerImplTest,
       ShouldThrottleNonInteractiveRequests) {
  // Change the quota for interactive requests ("active NTP user" is the default
  // class in tests).
  SetVariationParameter("quota_SuggestionFetcherActiveNTPUser", "5");
  ResetProvider();

  // One scheduling on start, 5 times after successful fetches.
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _)).Times(6);

  // First enable the scheduler -- this will trigger the persistent scheduling.
  ActivateProviderAndEula();

  // As long as the quota suffices, the call gets through.
  RemoteSuggestionsProvider::FetchStatusCallback signal_fetch_done;
  EXPECT_CALL(*provider(), RefetchInTheBackground(_))
      .Times(5)
      .WillRepeatedly(SaveArgByMove<0>(&signal_fetch_done));
  for (int x = 0; x < 5; ++x) {
    scheduler()->OnPersistentSchedulerWakeUp();
    std::move(signal_fetch_done).Run(Status::Success());
  }

  // For the 6th time, it is blocked by the scheduling provider.
  scheduler()->OnPersistentSchedulerWakeUp();
}

TEST_F(RemoteSuggestionsSchedulerImplTest,
       ShouldIgnoreSubsequentStartupSignalsForM58) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      kRemoteSuggestionsEmulateM58FetchingSchedule);
  RemoteSuggestionsProvider::FetchStatusCallback signal_fetch_done;

  // First enable the scheduler -- this will trigger the persistent scheduling.
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _));
  ActivateProviderAndEula();

  // The startup triggers are ignored.
  EXPECT_CALL(*provider(), RefetchInTheBackground(_)).Times(0);
  scheduler()->OnBrowserForegrounded();
  scheduler()->OnBrowserColdStart();

  // Foreground the browser again after a very long delay. Again, no fetch is
  // executed for neither Foregrounded, nor ColdStart.
  test_clock()->Advance(base::TimeDelta::FromHours(100000));
  scheduler()->OnBrowserForegrounded();
  scheduler()->OnBrowserColdStart();
}

TEST_F(RemoteSuggestionsSchedulerImplTest, ShouldIgnoreSignalsWhenOffline) {
  // Simulate being offline. NetworkChangeNotifier is a singleton, thus, this
  // instance is actually globally accessible (from the static function
  // NetworkChangeNotifier::IsOffline() that is called from the scheduler).
  FakeOfflineNetworkChangeNotifier fake;

  // Activating the provider should schedule the persistent background fetches.
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _));
  ActivateProviderAndEula();

  // All signals are ignored because of being offline.
  EXPECT_CALL(*provider(), RefetchInTheBackground(_)).Times(0);
  scheduler()->OnPersistentSchedulerWakeUp();
  scheduler()->OnSuggestionsSurfaceOpened();
  scheduler()->OnBrowserForegrounded();
  scheduler()->OnBrowserColdStart();
}

TEST_F(RemoteSuggestionsSchedulerImplTest,
       ShouldNotRefetchWhileDisplayingBeforeDefaultDelay) {
  // Activating the provider should schedule the persistent background fetches.
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _)).Times(2);
  // First enable the scheduler -- this will trigger the persistent scheduling.
  ActivateProviderAndEula();

  // The first refetch is never considered due to staleness.
  RemoteSuggestionsProvider::FetchStatusCallback signal_fetch_done;
  EXPECT_CALL(*provider(), RefetchInTheBackground(_))
      .WillOnce(SaveArgByMove<0>(&signal_fetch_done));
  scheduler()->OnBrowserForegrounded();
  std::move(signal_fetch_done).Run(Status::Success());

  // The staleness threshold by default equals to the startup interval.
  test_clock()->Advance(
      base::TimeDelta::FromHours(kDefaultStartupIntervalHours) -
      base::TimeDelta::FromMinutes(1));

  // Not long enough: non-stale.
  EXPECT_CALL(*provider(), RefetchWhileDisplaying(_)).Times(0);
  EXPECT_CALL(*provider(), RefetchInTheBackground(_));
  // Use the SurfaceOpened trigger as this has the shortest intervals and thus
  // allows to test both short and long delay in the same way.
  scheduler()->OnSuggestionsSurfaceOpened();
}

TEST_F(RemoteSuggestionsSchedulerImplTest,
       ShouldRefetchWhileDisplayingAfterDefaultDelay) {
  // Activating the provider should schedule the persistent background fetches.
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _)).Times(2);
  // First enable the scheduler -- this will trigger the persistent scheduling.
  ActivateProviderAndEula();

  // The first refetch is never considered due to staleness.
  RemoteSuggestionsProvider::FetchStatusCallback signal_fetch_done;
  EXPECT_CALL(*provider(), RefetchInTheBackground(_))
      .WillOnce(SaveArgByMove<0>(&signal_fetch_done));
  scheduler()->OnBrowserForegrounded();
  std::move(signal_fetch_done).Run(Status::Success());

  // The staleness threshold by default equals to the startup interval.
  test_clock()->Advance(
      base::TimeDelta::FromHours(kDefaultStartupIntervalHours) +
      base::TimeDelta::FromMinutes(1));

  // Long enough: stale.
  EXPECT_CALL(*provider(), RefetchWhileDisplaying(_));
  EXPECT_CALL(*provider(), RefetchInTheBackground(_)).Times(0);
  // Use the SurfaceOpened trigger as this has the shortest intervals and thus
  // allows to test both short and long delay in the same way.
  scheduler()->OnSuggestionsSurfaceOpened();
}

TEST_F(RemoteSuggestionsSchedulerImplTest,
       ShouldNotRefetchWhileDisplayingBeforeConfigurableDelay) {
  constexpr int kStaleHours = 18;
  SetVariationParameter("min_age_for_stale_fetch_hours",
                        base::NumberToString(kStaleHours));
  // Activating the provider should schedule the persistent background fetches.
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _)).Times(2);
  // First enable the scheduler -- this will trigger the persistent scheduling.
  ActivateProviderAndEula();

  // The first refetch is never considered due to staleness.
  RemoteSuggestionsProvider::FetchStatusCallback signal_fetch_done;
  EXPECT_CALL(*provider(), RefetchInTheBackground(_))
      .WillOnce(SaveArgByMove<0>(&signal_fetch_done));
  scheduler()->OnBrowserForegrounded();
  std::move(signal_fetch_done).Run(Status::Success());

  // The staleness threshold by default equals to the startup interval.
  test_clock()->Advance(base::TimeDelta::FromHours(kStaleHours) -
                        base::TimeDelta::FromMinutes(1));

  // Not long enough: non-stale.
  EXPECT_CALL(*provider(), RefetchWhileDisplaying(_)).Times(0);
  EXPECT_CALL(*provider(), RefetchInTheBackground(_));
  // Use the SurfaceOpened trigger as this has the shortest intervals and thus
  // allows to test both short and long delay in the same way.
  scheduler()->OnSuggestionsSurfaceOpened();
}

TEST_F(RemoteSuggestionsSchedulerImplTest,
       ShouldRefetchWhileDisplayingAfterConfigurableDelay) {
  constexpr int kStaleHours = 18;
  SetVariationParameter("min_age_for_stale_fetch_hours",
                        base::NumberToString(kStaleHours));
  // Activating the provider should schedule the persistent background fetches.
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _)).Times(2);
  // First enable the scheduler -- this will trigger the persistent scheduling.
  ActivateProviderAndEula();

  // The first refetch is never considered due to staleness.
  RemoteSuggestionsProvider::FetchStatusCallback signal_fetch_done;
  EXPECT_CALL(*provider(), RefetchInTheBackground(_))
      .WillOnce(SaveArgByMove<0>(&signal_fetch_done));
  scheduler()->OnBrowserForegrounded();
  std::move(signal_fetch_done).Run(Status::Success());

  test_clock()->Advance(base::TimeDelta::FromHours(kStaleHours) +
                        base::TimeDelta::FromMinutes(1));

  // Long enough: stale.
  EXPECT_CALL(*provider(), RefetchWhileDisplaying(_));
  EXPECT_CALL(*provider(), RefetchInTheBackground(_)).Times(0);
  // Use the SurfaceOpened trigger as this has the shortest intervals and thus
  // allows to test both short and long delay in the same way.
  scheduler()->OnSuggestionsSurfaceOpened();
}

TEST_F(RemoteSuggestionsSchedulerImplTest,
       ShouldNotRefetchWhileDisplayingBeforeFallbackConfigurableDelay) {
  constexpr int kStartupHours = 12;
  SetVariationParameter("startup_fetching_interval_hours-wifi-active_ntp_user",
                        base::NumberToString(kStartupHours));
  // Activating the provider should schedule the persistent background fetches.
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _)).Times(2);
  // First enable the scheduler -- this will trigger the persistent scheduling.
  ActivateProviderAndEula();

  // The first refetch is never considered due to staleness.
  RemoteSuggestionsProvider::FetchStatusCallback signal_fetch_done;
  EXPECT_CALL(*provider(), RefetchInTheBackground(_))
      .WillOnce(SaveArgByMove<0>(&signal_fetch_done));
  scheduler()->OnBrowserForegrounded();
  std::move(signal_fetch_done).Run(Status::Success());

  // The staleness threshold by default equals to the startup interval.
  test_clock()->Advance(base::TimeDelta::FromHours(kStartupHours) -
                        base::TimeDelta::FromMinutes(1));

  // Not long enough: non-stale.
  EXPECT_CALL(*provider(), RefetchWhileDisplaying(_)).Times(0);
  EXPECT_CALL(*provider(), RefetchInTheBackground(_));
  // Use the SurfaceOpened trigger as this has the shortest intervals and thus
  // allows to test both short and long delay in the same way.
  scheduler()->OnSuggestionsSurfaceOpened();
}

TEST_F(RemoteSuggestionsSchedulerImplTest,
       ShouldRefetchWhileDisplayingAfterFallbackConfigurableDelay) {
  constexpr int kStartupHours = 12;
  SetVariationParameter("startup_fetching_interval_hours-wifi-active_ntp_user",
                        base::NumberToString(kStartupHours));
  // Activating the provider should schedule the persistent background fetches.
  EXPECT_CALL(*persistent_scheduler(), Schedule(_, _)).Times(2);
  // First enable the scheduler -- this will trigger the persistent scheduling.
  ActivateProviderAndEula();

  // The first refetch is never considered due to staleness.
  RemoteSuggestionsProvider::FetchStatusCallback signal_fetch_done;
  EXPECT_CALL(*provider(), RefetchInTheBackground(_))
      .WillOnce(SaveArgByMove<0>(&signal_fetch_done));
  scheduler()->OnBrowserForegrounded();
  std::move(signal_fetch_done).Run(Status::Success());

  test_clock()->Advance(base::TimeDelta::FromHours(kStartupHours) +
                        base::TimeDelta::FromMinutes(1));

  // Long enough: stale.
  EXPECT_CALL(*provider(), RefetchWhileDisplaying(_));
  EXPECT_CALL(*provider(), RefetchInTheBackground(_)).Times(0);
  // Use the SurfaceOpened trigger as this has the shortest intervals and thus
  // allows to test both short and long delay in the same way.
  scheduler()->OnSuggestionsSurfaceOpened();
}

}  // namespace ntp_snippets
