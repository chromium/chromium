// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/internal/tracker_impl.h"

#include <map>
#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/feature_engagement/internal/availability_model_impl.h"
#include "components/feature_engagement/internal/display_lock_controller.h"
#include "components/feature_engagement/internal/editable_configuration.h"
#include "components/feature_engagement/internal/event_model_impl.h"
#include "components/feature_engagement/internal/feature_config_condition_validator.h"
#include "components/feature_engagement/internal/in_memory_event_store.h"
#include "components/feature_engagement/internal/never_availability_model.h"
#include "components/feature_engagement/internal/never_event_storage_validator.h"
#include "components/feature_engagement/internal/once_condition_validator.h"
#include "components/feature_engagement/internal/stats.h"
#include "components/feature_engagement/internal/test/test_time_provider.h"
#include "components/feature_engagement/internal/time_provider.h"
#include "components/feature_engagement/public/configuration.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/feature_list.h"
#include "components/feature_engagement/public/session_controller.h"
#include "components/feature_engagement/test/scoped_iph_feature_list.h"
#include "components/feature_engagement/test/test_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feature_engagement {

namespace {
BASE_FEATURE(kTrackerTestFeatureFoo,
             "test_foo",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kTrackerTestFeatureBar,
             "test_bar",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kTrackerTestFeatureBaz,
             "test_baz",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kTrackerTestFeatureQux,
             "test_qux",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kTrackerTestFeatureEvent,
             "test_event",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kTrackerTestFeatureSnooze,
             "test_snooze",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kTrackerTestGroupOne,
             "test_group_one",
             base::FEATURE_DISABLED_BY_DEFAULT);

void RegisterFeatureConfig(EditableConfiguration* configuration,
                           const base::Feature& feature,
                           bool valid,
                           bool tracking_only,
                           bool snooze_params,
                           const char* additional_event_name = nullptr) {
  FeatureConfig config;
  config.valid = valid;
  config.used.name = feature.name + std::string("_used");
  config.trigger.name = feature.name + std::string("_trigger");
  config.trigger.storage = 1u;
  config.groups = {"test_group_one"};
  config.tracking_only = tracking_only;
  if (snooze_params) {
    config.snooze_params.snooze_interval = 7u;
    config.snooze_params.max_limit = 3u;
  }
  if (additional_event_name) {
    EventConfig event_config;
    event_config.name = additional_event_name;
    event_config.comparator.type = GREATER_THAN_OR_EQUAL;
    event_config.comparator.value = 2U;
    event_config.window = 7U;
    event_config.storage = 7U;
    config.event_configs.emplace(std::move(event_config));
  }
  configuration->SetConfiguration(&feature, config);
}

void RegisterGroupConfig(EditableConfiguration* configuration,
                         const base::Feature& group,
                         bool valid) {
  GroupConfig config;
  config.valid = valid;
  config.trigger.name = group.name + std::string("_trigger");
  config.trigger.storage = 1u;
  configuration->SetConfiguration(&group, config);
}

// An OnInitializedCallback that stores whether it has been invoked and what
// the result was.
class StoringInitializedCallback {
 public:
  StoringInitializedCallback() : invoked_(false), success_(false) {}

  StoringInitializedCallback(const StoringInitializedCallback&) = delete;
  StoringInitializedCallback& operator=(const StoringInitializedCallback&) =
      delete;

  void OnInitialized(bool success) {
    DCHECK(!invoked_);
    invoked_ = true;
    success_ = success;
  }

  bool invoked() { return invoked_; }

  bool success() { return success_; }

 private:
  bool invoked_;
  bool success_;
};

// An InMemoryEventStore that is able to fake successful and unsuccessful
// loading of state.
class TestTrackerInMemoryEventStore : public InMemoryEventStore {
 public:
  explicit TestTrackerInMemoryEventStore(bool load_should_succeed)
      : load_should_succeed_(load_should_succeed) {}

  TestTrackerInMemoryEventStore(const TestTrackerInMemoryEventStore&) = delete;
  TestTrackerInMemoryEventStore& operator=(
      const TestTrackerInMemoryEventStore&) = delete;

  void Load(OnLoadedCallback callback) override {
    HandleLoadResult(std::move(callback), load_should_succeed_);
  }

  void WriteEvent(const Event& event) override {
    events_[event.name()] = event;
  }

  Event GetEvent(const std::string& event_name) { return events_[event_name]; }

 private:
  // Denotes whether the call to Load(...) should succeed or not. This impacts
  // both the ready-state and the result for the OnLoadedCallback.
  bool load_should_succeed_;

  std::map<std::string, Event> events_;
};

class StoreEverythingEventStorageValidator : public EventStorageValidator {
 public:
  StoreEverythingEventStorageValidator() = default;

  StoreEverythingEventStorageValidator(
      const StoreEverythingEventStorageValidator&) = delete;
  StoreEverythingEventStorageValidator& operator=(
      const StoreEverythingEventStorageValidator&) = delete;

  ~StoreEverythingEventStorageValidator() override = default;

  bool ShouldStore(const std::string& event_name) const override {
    return true;
  }

  bool ShouldKeep(const std::string& event_name,
                  uint32_t event_day,
                  uint32_t current_day) const override {
    return true;
  }
};

class TestTrackerAvailabilityModel : public AvailabilityModel {
 public:
  TestTrackerAvailabilityModel() : ready_(true) {}

  TestTrackerAvailabilityModel(const TestTrackerAvailabilityModel&) = delete;
  TestTrackerAvailabilityModel& operator=(const TestTrackerAvailabilityModel&) =
      delete;

  ~TestTrackerAvailabilityModel() override = default;

  void Initialize(AvailabilityModel::OnInitializedCallback callback,
                  uint32_t current_day) override {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), ready_));
  }

  bool IsReady() const override { return ready_; }

  void SetIsReady(bool ready) { ready_ = ready; }

  std::optional<uint32_t> GetAvailability(
      const base::Feature& feature) const override {
    return std::nullopt;
  }

 private:
  bool ready_;
};

class TestTrackerDisplayLockController : public DisplayLockController {
 public:
  TestTrackerDisplayLockController() = default;

  TestTrackerDisplayLockController(const TestTrackerDisplayLockController&) =
      delete;
  TestTrackerDisplayLockController& operator=(
      const TestTrackerDisplayLockController&) = delete;

  ~TestTrackerDisplayLockController() override = default;

  std::unique_ptr<DisplayLockHandle> AcquireDisplayLock() override {
    return std::move(next_display_lock_handle_);
  }

  bool IsDisplayLocked() const override { return false; }

  void SetNextDisplayLockHandle(
      std::unique_ptr<DisplayLockHandle> display_lock_handle) {
    next_display_lock_handle_ = std::move(display_lock_handle);
  }

 private:
  // The next DisplayLockHandle to return.
  std::unique_ptr<DisplayLockHandle> next_display_lock_handle_;
};

class TestTrackerEventExporter : public TrackerEventExporter {
 public:
  TestTrackerEventExporter() = default;

  ~TestTrackerEventExporter() override = default;

  void ExportEvents(ExportEventsCallback callback) override {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), events_to_export_));
  }

  void SetEventsToExport(std::vector<EventData> events) {
    events_to_export_ = events;
  }

  base::WeakPtr<TestTrackerEventExporter> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  // The events to export
  std::vector<EventData> events_to_export_;

  base::WeakPtrFactory<TestTrackerEventExporter> weak_ptr_factory_{this};
};

class TestSessionController : public SessionController {
 public:
  TestSessionController() : should_reset_for_next_call_(false) {}
  ~TestSessionController() override = default;

  bool ShouldResetSession() override { return should_reset_for_next_call_; }

  void SetShouldResetForNextCall(bool should_reset_for_next_call) {
    should_reset_for_next_call_ = should_reset_for_next_call;
  }

 private:
  bool should_reset_for_next_call_;
};

#if BUILDFLAG(IS_CHROMEOS_ASH)
class TestConfigurationProvider : public ConfigurationProvider {
 public:
  TestConfigurationProvider() = default;
  ~TestConfigurationProvider() override = default;

  // ConfigurationProvider:
  bool MaybeProvideFeatureConfiguration(
      const base::Feature& feature,
      feature_engagement::FeatureConfig& config,
      const feature_engagement::FeatureVector& known_features,
      const feature_engagement::GroupVector& known_groups) const override {
    config = config_;
    return true;
  }

  const char* GetConfigurationSourceDescription() const override {
    return "Test Configuration Provider";
  }

  std::set<std::string> MaybeProvideAllowedEventPrefixes(
      const base::Feature& feature) const override {
    return {};
  }

  void SetConfig(const FeatureConfig& config) { config_ = config; }

 private:
  FeatureConfig config_;
};
#endif

class TrackerImplTest : public ::testing::Test {
 public:
  TrackerImplTest() = default;

  TrackerImplTest(const TrackerImplTest&) = delete;
  TrackerImplTest& operator=(const TrackerImplTest&) = delete;

  void SetUp() override {
    std::unique_ptr<EditableConfiguration> configuration =
        std::make_unique<EditableConfiguration>();
    configuration_ = configuration.get();

    RegisterFeatureConfig(configuration.get(), kTrackerTestFeatureFoo,
                          true /* is_valid */, false /* tracking_only */,
                          false /* snooze_params */);
    RegisterFeatureConfig(configuration.get(), kTrackerTestFeatureBar,
                          true /* is_valid */, false /* tracking_only */,
                          false /* snooze_params */);
    RegisterFeatureConfig(configuration.get(), kTrackerTestFeatureBaz,
                          true /* is_valid */, true /* tracking_only */,
                          false /* snooze_params */);
    RegisterFeatureConfig(configuration.get(), kTrackerTestFeatureQux,
                          false /* is_valid */, false /* tracking_only */,
                          false /* snooze_params */);
    RegisterFeatureConfig(configuration.get(), kTrackerTestFeatureEvent,
                          /*valid=*/true, /*tracking_only=*/false,
                          /*snooze_params=*/false, "test_event_event");
    RegisterFeatureConfig(configuration.get(), kTrackerTestFeatureSnooze,
                          true /* is_valid */, false /* tracking_only */,
                          true /* snooze_params */);
    RegisterGroupConfig(configuration.get(), kTrackerTestGroupOne,
                        true /* is_valid */);

    std::unique_ptr<TestTrackerInMemoryEventStore> event_store =
        CreateEventStore();
    event_store_ = event_store.get();

    auto event_model = std::make_unique<EventModelImpl>(
        std::move(event_store),
        std::make_unique<StoreEverythingEventStorageValidator>());

    auto availability_model = std::make_unique<TestTrackerAvailabilityModel>();
    availability_model_ = availability_model.get();
    availability_model_->SetIsReady(ShouldAvailabilityStoreBeReady());

    auto display_lock_controller =
        std::make_unique<TestTrackerDisplayLockController>();
    display_lock_controller_ = display_lock_controller.get();

    auto condition_validator = CreateConditionValidator();
    condition_validator_ = condition_validator.get();

    auto time_provider = std::make_unique<TestTimeProvider>();
    time_provider_ = time_provider.get();
    time_provider->SetCurrentDay(1u);

    auto event_exporter = std::make_unique<TestTrackerEventExporter>();
    event_exporter_ = event_exporter.get();

    auto session_controller = std::make_unique<TestSessionController>();
    session_controller_ = session_controller.get();

    tracker_ = std::make_unique<TrackerImpl>(
        std::move(event_model), std::move(availability_model),
        std::move(configuration), std::move(display_lock_controller),
        std::move(condition_validator), std::move(time_provider),
        std::move(event_exporter), std::move(session_controller));
  }

  void VerifyEventTrigger(std::string event_name, uint32_t count) {
    Event trigger_event = event_store_->GetEvent(event_name);
    if (count == 0) {
      EXPECT_EQ(0, trigger_event.events_size());
      return;
    }

    EXPECT_EQ(1, trigger_event.events_size());
    EXPECT_EQ(1u, trigger_event.events(0).day());
    EXPECT_EQ(count, trigger_event.events(0).count());
  }

  void VerifyEventTriggerEvents(const base::Feature& feature, uint32_t count) {
    VerifyEventTrigger(configuration_->GetFeatureConfig(feature).trigger.name,
                       count);
  }

  void VerifyGroupEventTriggerEvents(const base::Feature& group,
                                     uint32_t count) {
    VerifyEventTrigger(configuration_->GetGroupConfig(group).trigger.name,
                       count);
  }

  void VerifyHistogramsForFeature(const std::string& histogram_name,
                                  bool check,
                                  int expected_success_count,
                                  int expected_failure_count,
                                  int expected_success_tracking_only_count) {
    if (!check)
      return;

    histogram_tester_.ExpectBucketCount(
        histogram_name, static_cast<int>(stats::TriggerHelpUIResult::SUCCESS),
        expected_success_count);
    histogram_tester_.ExpectBucketCount(
        histogram_name, static_cast<int>(stats::TriggerHelpUIResult::FAILURE),
        expected_failure_count);
    histogram_tester_.ExpectBucketCount(
        histogram_name,
        static_cast<int>(stats::TriggerHelpUIResult::SUCCESS_TRACKING_ONLY),
        expected_success_tracking_only_count);
  }

  // Histogram values are checked only if their respective |check_...| is true,
  // since inspecting a bucket count for a histogram that has not been recorded
  // yet leads to an error.
  void VerifyHistograms(bool check_foo,
                        int expected_foo_success_count,
                        int expected_foo_failure_count,
                        int expected_foo_success_tracking_only_count,
                        bool check_bar,
                        int expected_bar_success_count,
                        int expected_bar_failure_count,
                        int expected_bar_success_tracking_only_count,
                        bool check_baz,
                        int expected_baz_success_count,
                        int expected_baz_failure_count,
                        int expected_baz_success_tracking_only_count,
                        bool check_qux,
                        int expected_qux_success_count,
                        int expected_qux_failure_count,
                        int expected_qux_success_tracking_only_count) {
    VerifyHistogramsForFeature("InProductHelp.ShouldTriggerHelpUI.test_foo",
                               check_foo, expected_foo_success_count,
                               expected_foo_failure_count,
                               expected_foo_success_tracking_only_count);
    VerifyHistogramsForFeature("InProductHelp.ShouldTriggerHelpUI.test_bar",
                               check_bar, expected_bar_success_count,
                               expected_bar_failure_count,
                               expected_bar_success_tracking_only_count);
    VerifyHistogramsForFeature("InProductHelp.ShouldTriggerHelpUI.test_baz",
                               check_baz, expected_baz_success_count,
                               expected_baz_failure_count,
                               expected_baz_success_tracking_only_count);
    VerifyHistogramsForFeature("InProductHelp.ShouldTriggerHelpUI.test_qux",
                               check_qux, expected_qux_success_count,
                               expected_qux_failure_count,
                               expected_qux_success_tracking_only_count);

    int expected_total_successes =
        expected_foo_success_count + expected_bar_success_count +
        expected_baz_success_count + expected_qux_success_count;
    int expected_total_failures =
        expected_foo_failure_count + expected_bar_failure_count +
        expected_baz_failure_count + expected_qux_failure_count;
    int expected_total_success_tracking_onlys =
        expected_foo_success_tracking_only_count +
        expected_bar_success_tracking_only_count +
        expected_baz_success_tracking_only_count +
        expected_qux_success_tracking_only_count;
    bool should_check = check_foo || check_bar || check_baz || check_qux;
    VerifyHistogramsForFeature("InProductHelp.ShouldTriggerHelpUI",
                               should_check, expected_total_successes,
                               expected_total_failures,
                               expected_total_success_tracking_onlys);
  }

  void VerifyUserActionsTriggerChecks(
      const base::UserActionTester& user_action_tester,
      int expected_foo_count,
      int expected_bar_count,
      int expected_baz_count,
      int expected_qux_count) {
    EXPECT_EQ(expected_foo_count,
              user_action_tester.GetActionCount(
                  "InProductHelp.ShouldTriggerHelpUI.test_foo"));
    EXPECT_EQ(expected_bar_count,
              user_action_tester.GetActionCount(
                  "InProductHelp.ShouldTriggerHelpUI.test_bar"));
    EXPECT_EQ(expected_baz_count,
              user_action_tester.GetActionCount(
                  "InProductHelp.ShouldTriggerHelpUI.test_baz"));
    EXPECT_EQ(expected_qux_count,
              user_action_tester.GetActionCount(
                  "InProductHelp.ShouldTriggerHelpUI.test_qux"));
  }

  void VerifyUserActionsTriggered(
      const base::UserActionTester& user_action_tester,
      int expected_foo_count,
      int expected_bar_count,
      int expected_baz_count,
      int expected_qux_count) {
    EXPECT_EQ(
        expected_foo_count,
        user_action_tester.GetActionCount(
            "InProductHelp.ShouldTriggerHelpUIResult.Triggered.test_foo"));
    EXPECT_EQ(
        expected_bar_count,
        user_action_tester.GetActionCount(
            "InProductHelp.ShouldTriggerHelpUIResult.Triggered.test_bar"));
    EXPECT_EQ(
        expected_baz_count,
        user_action_tester.GetActionCount(
            "InProductHelp.ShouldTriggerHelpUIResult.Triggered.test_baz"));
    EXPECT_EQ(
        expected_qux_count,
        user_action_tester.GetActionCount(
            "InProductHelp.ShouldTriggerHelpUIResult.Triggered.test_qux"));
  }

  void VerifyUserActionsNotTriggered(
      const base::UserActionTester& user_action_tester,
      int expected_foo_count,
      int expected_bar_count,
      int expected_baz_count,
      int expected_qux_count) {
    EXPECT_EQ(
        expected_foo_count,
        user_action_tester.GetActionCount(
            "InProductHelp.ShouldTriggerHelpUIResult.NotTriggered.test_foo"));
    EXPECT_EQ(
        expected_bar_count,
        user_action_tester.GetActionCount(
            "InProductHelp.ShouldTriggerHelpUIResult.NotTriggered.test_bar"));
    EXPECT_EQ(
        expected_baz_count,
        user_action_tester.GetActionCount(
            "InProductHelp.ShouldTriggerHelpUIResult.NotTriggered.test_baz"));
    EXPECT_EQ(
        expected_qux_count,
        user_action_tester.GetActionCount(
            "InProductHelp.ShouldTriggerHelpUIResult.NotTriggered.test_qux"));
  }

  void VerifyUserActionsWouldHaveTriggered(
      const base::UserActionTester& user_action_tester,
      int expected_foo_count,
      int expected_bar_count,
      int expected_baz_count,
      int expected_qux_count) {
    EXPECT_EQ(expected_foo_count, user_action_tester.GetActionCount(
                                      "InProductHelp.ShouldTriggerHelpUIResult."
                                      "WouldHaveTriggered.test_foo"));
    EXPECT_EQ(expected_bar_count, user_action_tester.GetActionCount(
                                      "InProductHelp.ShouldTriggerHelpUIResult."
                                      "WouldHaveTriggered.test_bar"));
    EXPECT_EQ(expected_baz_count, user_action_tester.GetActionCount(
                                      "InProductHelp.ShouldTriggerHelpUIResult."
                                      "WouldHaveTriggered.test_baz"));
    EXPECT_EQ(expected_qux_count, user_action_tester.GetActionCount(
                                      "InProductHelp.ShouldTriggerHelpUIResult."
                                      "WouldHaveTriggered.test_qux"));
  }

  void VerifyUserActionsDismissed(
      const base::UserActionTester& user_action_tester,
      int expected_dismissed_count) {
    EXPECT_EQ(expected_dismissed_count,
              user_action_tester.GetActionCount("InProductHelp.Dismissed"));
  }

 protected:
  virtual std::unique_ptr<TestTrackerInMemoryEventStore> CreateEventStore() {
    // Returns a EventStore that will successfully initialize.
    return std::make_unique<TestTrackerInMemoryEventStore>(true);
  }

  virtual std::unique_ptr<ConditionValidator> CreateConditionValidator() {
    return std::make_unique<OnceConditionValidator>();
  }

  virtual bool ShouldAvailabilityStoreBeReady() { return true; }

  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<TrackerImpl> tracker_;
  raw_ptr<TestTrackerInMemoryEventStore> event_store_;
  raw_ptr<TestTrackerAvailabilityModel> availability_model_;
  raw_ptr<TestTrackerDisplayLockController> display_lock_controller_;
  raw_ptr<EditableConfiguration> configuration_;
  raw_ptr<TestTrackerEventExporter> event_exporter_;
  raw_ptr<TestSessionController> session_controller_;
  base::HistogramTester histogram_tester_;
  raw_ptr<ConditionValidator> condition_validator_;
  raw_ptr<TestTimeProvider> time_provider_;
};

// A top-level test class where the store fails to initialize.
class FailingStoreInitTrackerImplTest : public TrackerImplTest {
 public:
  FailingStoreInitTrackerImplTest() = default;

  FailingStoreInitTrackerImplTest(const FailingStoreInitTrackerImplTest&) =
      delete;
  FailingStoreInitTrackerImplTest& operator=(
      const FailingStoreInitTrackerImplTest&) = delete;

 protected:
  std::unique_ptr<TestTrackerInMemoryEventStore> CreateEventStore() override {
    // Returns a EventStore that will fail to initialize.
    return std::make_unique<TestTrackerInMemoryEventStore>(false);
  }
};

// A top-level test class where the AvailabilityModel fails to initialize.
class FailingAvailabilityModelInitTrackerImplTest : public TrackerImplTest {
 public:
  FailingAvailabilityModelInitTrackerImplTest() = default;

  FailingAvailabilityModelInitTrackerImplTest(
      const FailingAvailabilityModelInitTrackerImplTest&) = delete;
  FailingAvailabilityModelInitTrackerImplTest& operator=(
      const FailingAvailabilityModelInitTrackerImplTest&) = delete;

 protected:
  bool ShouldAvailabilityStoreBeReady() override { return false; }
};

}  // namespace

TEST_F(TrackerImplTest, TestCreateTestTracker) {
  EXPECT_NE(feature_engagement::CreateTestTracker(), nullptr);
}

TEST_F(TrackerImplTest, TestInitialization) {
  EXPECT_FALSE(tracker_->IsInitialized());

  StoringInitializedCallback callback;
  tracker_->AddOnInitializedCallback(base::BindOnce(
      &StoringInitializedCallback::OnInitialized, base::Unretained(&callback)));
  EXPECT_FALSE(callback.invoked());

  // Ensure all initialization is finished.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(tracker_->IsInitialized());
  EXPECT_TRUE(callback.invoked());
  EXPECT_TRUE(callback.success());
}

TEST_F(TrackerImplTest, TestInitializationMultipleCallbacks) {
  EXPECT_FALSE(tracker_->IsInitialized());

  StoringInitializedCallback callback1;
  StoringInitializedCallback callback2;

  tracker_->AddOnInitializedCallback(
      base::BindOnce(&StoringInitializedCallback::OnInitialized,
                     base::Unretained(&callback1)));
  tracker_->AddOnInitializedCallback(
      base::BindOnce(&StoringInitializedCallback::OnInitialized,
                     base::Unretained(&callback2)));
  EXPECT_FALSE(callback1.invoked());
  EXPECT_FALSE(callback2.invoked());

  // Ensure all initialization is finished.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(tracker_->IsInitialized());
  EXPECT_TRUE(callback1.invoked());
  EXPECT_TRUE(callback2.invoked());
  EXPECT_TRUE(callback1.success());
  EXPECT_TRUE(callback2.success());
}

TEST_F(TrackerImplTest, TestAddingCallbackAfterInitFinished) {
  EXPECT_FALSE(tracker_->IsInitialized());

  // Ensure all initialization is finished.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(tracker_->IsInitialized());

  StoringInitializedCallback callback;
  tracker_->AddOnInitializedCallback(base::BindOnce(
      &StoringInitializedCallback::OnInitialized, base::Unretained(&callback)));
  EXPECT_FALSE(callback.invoked());

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(callback.invoked());
}

TEST_F(TrackerImplTest, TestAddingCallbackBeforeAndAfterInitFinished) {
  EXPECT_FALSE(tracker_->IsInitialized());

  // Ensure all initialization is finished.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(tracker_->IsInitialized());

  StoringInitializedCallback callback_before;
  tracker_->AddOnInitializedCallback(
      base::BindOnce(&StoringInitializedCallback::OnInitialized,
                     base::Unretained(&callback_before)));
  EXPECT_FALSE(callback_before.invoked());

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(callback_before.invoked());

  StoringInitializedCallback callback_after;
  tracker_->AddOnInitializedCallback(
      base::BindOnce(&StoringInitializedCallback::OnInitialized,
                     base::Unretained(&callback_after)));
  EXPECT_FALSE(callback_after.invoked());

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(callback_after.invoked());
}

TEST_F(FailingStoreInitTrackerImplTest, TestFailingInitialization) {
  EXPECT_FALSE(tracker_->IsInitialized());

  StoringInitializedCallback callback;
  tracker_->AddOnInitializedCallback(base::BindOnce(
      &StoringInitializedCallback::OnInitialized, base::Unretained(&callback)));
  EXPECT_FALSE(callback.invoked());

  // Ensure all initialization is finished.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(tracker_->IsInitialized());
  EXPECT_TRUE(callback.invoked());
  EXPECT_FALSE(callback.success());
}

TEST_F(FailingStoreInitTrackerImplTest,
       TestFailingInitializationMultipleCallbacks) {
  EXPECT_FALSE(tracker_->IsInitialized());

  StoringInitializedCallback callback1;
  StoringInitializedCallback callback2;
  tracker_->AddOnInitializedCallback(
      base::BindOnce(&StoringInitializedCallback::OnInitialized,
                     base::Unretained(&callback1)));
  tracker_->AddOnInitializedCallback(
      base::BindOnce(&StoringInitializedCallback::OnInitialized,
                     base::Unretained(&callback2)));
  EXPECT_FALSE(callback1.invoked());
  EXPECT_FALSE(callback2.invoked());

  // Ensure all initialization is finished.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(tracker_->IsInitialized());
  EXPECT_TRUE(callback1.invoked());
  EXPECT_TRUE(callback2.invoked());
  EXPECT_FALSE(callback1.success());
  EXPECT_FALSE(callback2.success());
}

TEST_F(FailingAvailabilityModelInitTrackerImplTest, AvailabilityModelNotReady) {
  EXPECT_FALSE(tracker_->IsInitialized());

  StoringInitializedCallback callback;
  tracker_->AddOnInitializedCallback(base::BindOnce(
      &StoringInitializedCallback::OnInitialized, base::Unretained(&callback)));
  EXPECT_FALSE(callback.invoked());

  // Ensure all initialization is finished.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(tracker_->IsInitialized());
  EXPECT_TRUE(callback.invoked());
  EXPECT_FALSE(callback.success());
}

TEST_F(TrackerImplTest, TestMigrateEvents) {
  EXPECT_FALSE(tracker_->IsInitialized());
  TestTrackerEventExporter::EventData event1("test", 1);
  event_exporter_->SetEventsToExport({event1});

  StoringInitializedCallback callback;
  tracker_->AddOnInitializedCallback(base::BindOnce(
      &StoringInitializedCallback::OnInitialized, base::Unretained(&callback)));
  EXPECT_FALSE(callback.invoked());

  // Ensure all initialization is finished.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(tracker_->IsInitialized());
  EXPECT_TRUE(callback.invoked());
  EXPECT_TRUE(callback.success());

  // Check that event made it into the store.
  Event stored_event1 = event_store_->GetEvent("test");
  EXPECT_EQ("test", stored_event1.name());
  ASSERT_EQ(1, stored_event1.events_size());
  EXPECT_EQ(1u, stored_event1.events(0).day());
  EXPECT_EQ(1u, stored_event1.events(0).count());
}

TEST_F(TrackerImplTest, TestMigrateMultipleEvents) {
  EXPECT_FALSE(tracker_->IsInitialized());
  TestTrackerEventExporter::EventData event1("test", 1);
  TestTrackerEventExporter::EventData event2("test2", 1);
  event_exporter_->SetEventsToExport({event1, event2});

  StoringInitializedCallback callback;
  tracker_->AddOnInitializedCallback(base::BindOnce(
      &StoringInitializedCallback::OnInitialized, base::Unretained(&callback)));
  EXPECT_FALSE(callback.invoked());

  // Ensure all initialization is finished.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(tracker_->IsInitialized());
  EXPECT_TRUE(callback.invoked());
  EXPECT_TRUE(callback.success());

  // Check that events made it into the store.
  Event stored_event1 = event_store_->GetEvent("test");
  EXPECT_EQ("test", stored_event1.name());
  ASSERT_EQ(1, stored_event1.events_size());
  EXPECT_EQ(1u, stored_event1.events(0).day());
  EXPECT_EQ(1u, stored_event1.events(0).count());

  Event stored_event2 = event_store_->GetEvent("test2");
  EXPECT_EQ("test2", stored_event2.name());
  ASSERT_EQ(1, stored_event2.events_size());
  EXPECT_EQ(1u, stored_event2.events(0).day());
  EXPECT_EQ(1u, stored_event2.events(0).count());
}

TEST_F(TrackerImplTest, TestMigrateSameEventMultipleTimes) {
  EXPECT_FALSE(tracker_->IsInitialized());
  TestTrackerEventExporter::EventData event1("test", 1);
  TestTrackerEventExporter::EventData event2("test", 1);
  TestTrackerEventExporter::EventData event3("test", 2);
  event_exporter_->SetEventsToExport({event1, event2, event3});

  StoringInitializedCallback callback;
  tracker_->AddOnInitializedCallback(base::BindOnce(
      &StoringInitializedCallback::OnInitialized, base::Unretained(&callback)));
  EXPECT_FALSE(callback.invoked());

  // Ensure all initialization is finished.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(tracker_->IsInitialized());
  EXPECT_TRUE(callback.invoked());
  EXPECT_TRUE(callback.success());

  // Check that events made it into the store.
  Event stored_event = event_store_->GetEvent("test");
  EXPECT_EQ("test", stored_event.name());
  ASSERT_EQ(2, stored_event.events_size());
  EXPECT_EQ(1u, stored_event.events(0).day());
  EXPECT_EQ(2u, stored_event.events(0).count());

  EXPECT_EQ(2u, stored_event.events(1).day());
  EXPECT_EQ(1u, stored_event.events(1).count());
}

TEST_F(TrackerImplTest, TestNoMigration) {
  std::unique_ptr<Tracker> tracker = feature_engagement::CreateTestTracker();
  EXPECT_FALSE(tracker->IsInitialized());

  StoringInitializedCallback callback;
  tracker->AddOnInitializedCallback(base::BindOnce(
      &StoringInitializedCallback::OnInitialized, base::Unretained(&callback)));
  EXPECT_FALSE(callback.invoked());

  // Ensure all initialization is finished and no crash or NPE happens.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(tracker->IsInitialized());
  EXPECT_TRUE(callback.invoked());
  EXPECT_TRUE(callback.success());
}

TEST_F(TrackerImplTest, TestSetPriorityNotificationBeforeRegistration) {
  // Ensure all initialization is finished.
  StoringInitializedCallback callback;
  tracker_->AddOnInitializedCallback(base::BindOnce(
      &StoringInitializedCallback::OnInitialized, base::Unretained(&callback)));
  base::RunLoop().RunUntilIdle();

  bool invoked = false;

  // Set priority notification, and then register handler. IPH will show up
  // immediately after registration.
  tracker_->SetPriorityNotification(kTrackerTestFeatureFoo);
  tracker_->RegisterPriorityNotificationHandler(
      kTrackerTestFeatureFoo,
      base::BindLambdaForTesting([&]() { invoked = true; }));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(invoked);

  // Try registering handler once again. The IPH won't show up again since the
  // notification has been consumed.
  invoked = false;
  tracker_->RegisterPriorityNotificationHandler(
      kTrackerTestFeatureFoo,
      base::BindLambdaForTesting([&]() { invoked = true; }));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(invoked);

  // Set priority notification one more time. Now the IPH will show up.
  tracker_->SetPriorityNotification(kTrackerTestFeatureFoo);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(invoked);
}

TEST_F(TrackerImplTest, TestSetPriorityNotificationAfterRegistration) {
  // Ensure all initialization is finished.
  StoringInitializedCallback callback;
  tracker_->AddOnInitializedCallback(base::BindOnce(
      &StoringInitializedCallback::OnInitialized, base::Unretained(&callback)));
  base::RunLoop().RunUntilIdle();

  bool invoked = false;

  // Register the handler first, and then set priority notification.
  tracker_->RegisterPriorityNotificationHandler(
      kTrackerTestFeatureFoo,
      base::BindLambdaForTesting([&]() { invoked = true; }));
  tracker_->SetPriorityNotification(kTrackerTestFeatureFoo);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(invoked);

  // Set priority notification again. The IPH won't show up again, since the
  // handler is good for only one use.
  invoked = false;
  tracker_->SetPriorityNotification(kTrackerTestFeatureFoo);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(invoked);
}

TEST_F(TrackerImplTest, TestUnregisterPriorityNotification) {
  // Ensure all initialization is finished.
  StoringInitializedCallback callback;
  tracker_->AddOnInitializedCallback(base::BindOnce(
      &StoringInitializedCallback::OnInitialized, base::Unretained(&callback)));
  base::RunLoop().RunUntilIdle();

  bool invoked = false;

  // Register the handler, and unregister before setting the notification. The
  // IPH won't show up.
  invoked = false;
  tracker_->RegisterPriorityNotificationHandler(
      kTrackerTestFeatureFoo,
      base::BindLambdaForTesting([&]() { invoked = true; }));
  tracker_->UnregisterPriorityNotificationHandler(kTrackerTestFeatureFoo);
  tracker_->SetPriorityNotification(kTrackerTestFeatureFoo);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(invoked);
}

TEST_F(TrackerImplTest, TestTriggering) {
  // Ensure all initialization is finished.
  StoringInitializedCallback callback;
  tracker_->AddOnInitializedCallback(base::BindOnce(
      &StoringInitializedCallback::OnInitialized, base::Unretained(&callback)));
  base::RunLoop().RunUntilIdle();
  base::UserActionTester user_action_tester;

  // The first time a feature triggers it should be shown.
  EXPECT_TRUE(tracker_->ShouldTriggerHelpUI(kTrackerTestFeatureFoo));
  VerifyEventTriggerEvents(kTrackerTestFeatureFoo, 1u);
  VerifyGroupEventTriggerEvents(kTrackerTestGroupOne, 1u);
  EXPECT_FALSE(tracker_->ShouldTriggerHelpUI(kTrackerTestFeatureFoo));
  VerifyEventTriggerEvents(kTrackerTestFeatureFoo, 1u);
  VerifyGroupEventTriggerEvents(kTrackerTestGroupOne, 1u);
  EXPECT_FALSE(tracker_->ShouldTriggerHelpUI(kTrackerTestFeatureQux));
  VerifyEventTriggerEvents(kTrackerTestFeatureQux, 0);
  VerifyGroupEventTriggerEvents(kTrackerTestGroupOne, 1u);
  VerifyUserActionsTriggerChecks(user_action_tester, 2, 0, 0, 1);
  VerifyUserActionsTriggered(user_action_tester, 1, 0, 0, 0);
  VerifyUserActionsNotTriggered(user_action_tester, 1, 0, 0, 1);
  VerifyUserActionsWouldHaveTriggered(user_action_tester, 0, 0, 0, 0);
  VerifyUserActionsDismissed(user_action_tester, 0);
  VerifyHistograms(true, 1, 1, 0, false, 0, 0, 0, false, 0, 0, 0, true, 0, 1,
                   0);

  // While in-product help is currently showing, no other features should be
  // shown.
  EXPECT_FALSE(tracker_->ShouldTriggerHelpUI(kTrackerTestFeatureBar));
  VerifyEventTriggerEvents(kTrackerTestFeatureBar, 0);
  VerifyGroupEventTriggerEvents(kTrackerTestGroupOne, 1u);
  EXPECT_FALSE(tracker_->ShouldTriggerHelpUI(kTrackerTestFeatureQux));
  VerifyEventTriggerEvents(kTrackerTestFeatureQux, 0);
  VerifyGroupEventTriggerEvents(kTrackerTestGroupOne, 1u);
  VerifyUserActionsTriggerChecks(user_action_tester, 2, 1, 0, 2);
  VerifyUserActionsTriggered(user_action_tester, 1, 0, 0, 0);
  VerifyUserActionsNotTriggered(user_action_tester, 1, 1, 0, 2);
  VerifyUserActionsWouldHaveTriggered(user_action_tester, 0, 0, 0, 0);
  VerifyUserActionsDismissed(user_action_tester, 0);
  VerifyHistograms(true, 1, 1, 0, true, 0, 1, 0, false, 0, 0, 0, true, 0, 2, 0);

  // After dismissing the current in-product help, that feature can not be shown
  // again, but a different feature should.
  tracker_->Dismissed(kTrackerTestFeatureFoo);
  EXPECT_FALSE(tracker_->ShouldTriggerHelpUI(kTrackerTestFeatureFoo));
  VerifyEventTriggerEvents(kTrackerTestFeatureFoo, 1u);
  VerifyGroupEventTriggerEvents(kTrackerTestGroupOne, 1u);
  EXPECT_TRUE(tracker_->ShouldTriggerHelpUI(kTrackerTestFeatureBar));
  VerifyEventTriggerEvents(kTrackerTestFeatureBar, 1u);
  VerifyGroupEventTriggerEvents(kTrackerTestGroupOne, 2u);
  EXPECT_FALSE(tracker_->ShouldTriggerHelpUI(kTrackerTestFeatureQux));
  VerifyEventTriggerEvents(kTrackerTestFeatureQux, 0);
  VerifyGroupEventTriggerEvents(kTrackerTestGroupOne, 2u);
  VerifyUserActionsTriggerChecks(user_action_tester, 3, 2, 0, 3);
  VerifyUserActionsTriggered(user_action_tester, 1, 1, 0, 0);
  VerifyUserActionsNotTriggered(user_action_tester, 2, 1, 0, 3);
  VerifyUserActionsWouldHaveTriggered(user_action_tester, 0, 0, 0, 0);
  VerifyUserActionsDismissed(user_action_tester, 1);
  VerifyHistograms(true, 1, 2, 0, true, 1, 1, 0, false, 0, 0, 0, true, 0, 3, 0);

  // After dismissing the second registered feature, no more in-product help
  // should be shown, since kTrackerTestFeatureQux is invalid.
  tracker_->Dismissed(kTrackerTestFeatureBar);
  EXPECT_FALSE(tracker_->ShouldTriggerHelpUI(kTrackerTestFeatureFoo));
  VerifyEventTriggerEvents(kTrackerTestFeatureFoo, 1u);
  VerifyGroupEventTriggerEvents(kTrackerTestGroupOne, 2u);
  EXPECT_FALSE(tracker_->ShouldTriggerHelpUI(kTrackerTestFeatureBar));
  VerifyEventTriggerEvents(kTrackerTestFeatureBar, 1u);
  VerifyGroupEventTriggerEvents(kTrackerTestGroupOne, 2u);
  EXPECT_FALSE(tracker_->ShouldTriggerHelpUI(kTrackerTestFeatureQux));
  VerifyEventTriggerEvents(kTrackerTestFeatureQux, 0);
  VerifyGroupEventTriggerEvents(kTrackerTestGroupOne, 2u);
  VerifyUserActionsTriggerChecks(user_action_tester, 4, 3, 0, 4);
  VerifyUserActionsTriggered(user_action_tester, 1, 1, 0, 0);
  VerifyUserActionsNotTriggered(user_action_tester, 3, 2, 0, 4);
  VerifyUserActionsWouldHaveTriggered(user_action_tester, 0, 0, 0, 0);
  VerifyUserActionsDismissed(user_action_tester, 2);
  VerifyHistograms(true, 1, 3, 0, true, 1, 2, 0, false, 0, 0, 0, true, 0, 4, 0);
}

TEST_F(TrackerImplTest, TestTriggeringWithSessionController) {
  // Ensure all initialization is finished.
  StoringInitializedCallback callback;
  tracker_->AddOnInitializedCallback(base::BindOnce(
      &StoringInitializedCallback::OnInitialized, base::Unretained(&callback)));
  base::RunLoop().RunUntilIdle();
  base::UserActionTester user_action_tester;

  // The first time a feature triggers it should be shown.
  EXPECT_TRUE(tracker_->ShouldTriggerHelpUI(kTrackerTestFeatureFoo));
  VerifyEventTriggerEvents(kTrackerTestFeatureFoo, 1u);
  VerifyGroupEventTriggerEvents(kTrackerTestGroupOne, 1u);
  EXPECT_FALSE(tracker_->ShouldTriggerHelpUI(kTrackerTestFeatureFoo));
  VerifyEventTriggerEvents(kTrackerTestFeatureFoo, 1u);
  VerifyGroupEventTriggerEvents(kTrackerTestGroupOne, 1u);

  // Dismiss the feature.
  tracker_->Dismissed(kTrackerTestFeatureFoo);

  // Make the next `ShouldTriggerHelpUI` call trigger the session reset.
  session_controller_->SetShouldResetForNextCall(true);

  // The same feature can be shown again, and blocks a different feature.
  EXPECT_TRUE(tracker_->ShouldTriggerHelpUI(kTrackerTestFeatureFoo));
  VerifyEventTriggerEvents(kTrackerTestFeatureFoo, 2u);
  VerifyGroupEventTriggerEvents(kTrackerTestGroupOne, 2u);
  EXPECT_FALSE(tracker_->ShouldTriggerHelpUI(kTrackerTestFeatureBar));
  VerifyEventTriggerEvents(kTrackerTestFeatureBar, 0);
  VerifyGroupEventTriggerEvents(kTrackerTestGroupOne, 2u);
}

TEST_F(TrackerImplTest, TestTrackingOnlyTriggering) {
  // Ensure all initialization is finished.
  StoringInitializedCallback callback;
  tracker_->AddOnInitializedCallback(base::BindOnce(
      &StoringInitializedCallback::OnInitialized, base::Unretained(&callback)));
  base::RunLoop().RunUntilIdle();
  base::UserActionTester user_action_tester;

  // When another feature is showing, tracking only features should not trigger.
  EXPECT_TRUE(tracker_->ShouldTriggerHelpUI(kTrackerTestFeatureFoo));
  VerifyEventTriggerEvents(kTrackerTestFeatureFoo, 1u);
  EXPECT_FALSE(tracker_->ShouldTriggerHelpUI(kTrackerTestFeatureBaz));
  VerifyEventTriggerEvents(kTrackerTestFeatureBaz, 0u);
  VerifyUserActionsTriggerChecks(user_action_tester, 1, 0, 1, 0);
  VerifyUserActionsTriggered(user_action_tester, 1, 0, 0, 0);
  VerifyUserActionsNotTriggered(user_action_tester, 0, 0, 1, 0);
  VerifyUserActionsWouldHaveTriggered(user_action_tester, 0, 0, 0, 0);
  VerifyUserActionsDismissed(user_action_tester, 0);
  VerifyHistograms(true, 1, 0, 0, false, 0, 0, 0, true, 0, 1, 0, false, 0, 0,
                   0);

  // Now verify tracking only kTrackerTestFeatureBaz would have triggered and is
  // immediately be dismissed.
  tracker_->Dismissed(kTrackerTestFeatureFoo);
  VerifyUserActionsDismissed(user_action_tester, 1);
  EXPECT_FALSE(tracker_->ShouldTriggerHelpUI(kTrackerTestFeatureBaz));
  VerifyEventTriggerEvents(kTrackerTestFeatureBaz, 1u);
  VerifyUserActionsTriggerChecks(user_action_tester, 1, 0, 2, 0);
  VerifyUserActionsTriggered(user_action_tester, 1, 0, 0, 0);
  VerifyUserActionsNotTriggered(user_action_tester, 0, 0, 1, 0);
  VerifyUserActionsWouldHaveTriggered(user_action_tester, 0, 0, 1, 0);
  VerifyUserActionsDismissed(user_action_tester, 2);
  VerifyHistograms(true, 1, 0, 0, false, 0, 0, 0, true, 0, 1, 1, false, 0, 0,
                   0);

  // Other in-product help is should be showable after a tracking only feature
  // would have been triggered, because nothing is currently showing.
  EXPECT_TRUE(tracker_->ShouldTriggerHelpUI(kTrackerTestFeatureBar));
  VerifyEventTriggerEvents(kTrackerTestFeatureBar, 1u);
  VerifyUserActionsTriggerChecks(user_action_tester, 1, 1, 2, 0);
  VerifyUserActionsTriggered(user_action_tester, 1, 1, 0, 0);
  VerifyUserActionsNotTriggered(user_action_tester, 0, 0, 1, 0);
  VerifyUserActionsWouldHaveTriggered(user_action_tester, 0, 0, 1, 0);
  VerifyUserActionsDismissed(user_action_tester, 2);
  VerifyHistograms(true, 1, 0, 0, true, 1, 0, 0, true, 0, 1, 1, false, 0, 0, 0);
}

TEST_F(TrackerImplTest, TestHasEverTriggered) {
  // Ensure all initialization is finished.
  StoringInitializedCallback callback;
  tracker_->AddOnInitializedCallback(base::BindOnce(
      &StoringInitializedCallback::OnInitialized, base::Unretained(&callback)));
  base::RunLoop().RunUntilIdle();
  base::UserActionTester user_action_tester;

  // All the features should not be triggered yet.
  EXPECT_FALSE(tracker_->HasEverTriggered(kTrackerTestFeatureFoo, false));
  EXPECT_FALSE(tracker_->HasEverTriggered(kTrackerTestFeatureBar, false));
  EXPECT_FALSE(tracker_->HasEverTriggered(kTrackerTestFeatureBaz, false));
  EXPECT_FALSE(tracker_->HasEverTriggered(kTrackerTestFeatureQux, false));

  // For triggered features, has ever triggered from storage should returns
  // true, as the storage is set to 1.
  EXPECT_TRUE(tracker_->ShouldTriggerHelpUI(kTrackerTestFeatureFoo));
  EXPECT_FALSE(tracker_->ShouldTriggerHelpUI(kTrackerTestFeatureBar));
  VerifyEventTriggerEvents(kTrackerTestFeatureFoo, 1u);
  VerifyEventTriggerEvents(kTrackerTestFeatureBar, 0u);
  EXPECT_TRUE(tracker_->HasEverTriggered(kTrackerTestFeatureFoo, false));
  EXPECT_FALSE(tracker_->HasEverTriggered(kTrackerTestFeatureBar, false));
  tracker_->Dismissed(kTrackerTestFeatureFoo);

  // For tracking only feature, the event will still get recorded even
  // ShouldTriggerHelpUI returns false.
  EXPECT_FALSE(tracker_->ShouldTriggerHelpUI(kTrackerTestFeatureBaz));
  VerifyEventTriggerEvents(kTrackerTestFeatureBaz, 1u);
  EXPECT_TRUE(tracker_->HasEverTriggered(kTrackerTestFeatureBaz, false));

  // If |from_window| = true, HasEverTriggered will always returns false as
  // window size is 0 in test configurations.
  EXPECT_FALSE(tracker_->HasEverTriggered(kTrackerTestFeatureFoo, true));
  EXPECT_FALSE(tracker_->HasEverTriggered(kTrackerTestFeatureBaz, true));
}

TEST_F(TrackerImplTest, TestWouldTriggerInspection) {
  // Ensure all initialization is finished.
  StoringInitializedCallback callback;
  tracker_->AddOnInitializedCallback(base::BindOnce(
      &StoringInitializedCallback::OnInitialized, base::Unretained(&callback)));
  base::RunLoop().RunUntilIdle();
  base::UserActionTester user_action_tester;

  // Initially, both foo and bar would have been shown.
  EXPECT_TRUE(tracker_->WouldTriggerHelpUI(kTrackerTestFeatureFoo));
  EXPECT_TRUE(tracker_->WouldTriggerHelpUI(kTrackerTestFeatureBar));
  EXPECT_FALSE(tracker_->WouldTriggerHelpUI(kTrackerTestFeatureQux));
  VerifyEventTriggerEvents(kTrackerTestFeatureFoo, 0u);
  VerifyEventTriggerEvents(kTrackerTestFeatureBar, 0u);
  VerifyEventTriggerEvents(kTrackerTestFeatureQux, 0u);
  VerifyUserActionsTriggerChecks(user_action_tester, 0, 0, 0, 0);
  VerifyUserActionsTriggered(user_action_tester, 0, 0, 0, 0);
  VerifyUserActionsNotTriggered(user_action_tester, 0, 0, 0, 0);
  VerifyUserActionsWouldHaveTriggered(user_action_tester, 0, 0, 0, 0);
  VerifyUserActionsDismissed(user_action_tester, 0);
  VerifyHistograms(false, 0, 0, 0, false, 0, 0, 0, false, 0, 0, 0, false, 0, 0,
                   0);

  // While foo shows, nothing else would have been shown.
  EXPECT_TRUE(tracker_->ShouldTriggerHelpUI(kTrackerTestFeatureFoo));
  EXPECT_FALSE(tracker_->WouldTriggerHelpUI(kTrackerTestFeatureFoo));
  EXPECT_FALSE(tracker_->WouldTriggerHelpUI(kTrackerTestFeatureBar));
  EXPECT_FALSE(tracker_->WouldTriggerHelpUI(kTrackerTestFeatureQux));
  VerifyEventTriggerEvents(kTrackerTestFeatureFoo, 1);
  VerifyUserActionsTriggerChecks(user_action_tester, 1, 0, 0, 0);
  VerifyUserActionsTriggered(user_action_tester, 1, 0, 0, 0);
  VerifyUserActionsNotTriggered(user_action_tester, 0, 0, 0, 0);
  VerifyUserActionsWouldHaveTriggered(user_action_tester, 0, 0, 0, 0);
  VerifyUserActionsDismissed(user_action_tester, 0);
  VerifyHistograms(true, 1, 0, 0, false, 0, 0, 0, false, 0, 0, 0, false, 0, 0,
                   0);

  // After foo has been dismissed, it would not have triggered again, but bar
  // would have.
  tracker_->Dismissed(kTrackerTestFeatureFoo);
  EXPECT_FALSE(tracker_->WouldTriggerHelpUI(kTrackerTestFeatureFoo));
  EXPECT_FALSE(tracker_->ShouldTriggerHelpUI(kTrackerTestFeatureFoo));
  EXPECT_TRUE(tracker_->WouldTriggerHelpUI(kTrackerTestFeatureBar));
  EXPECT_TRUE(tracker_->ShouldTriggerHelpUI(kTrackerTestFeatureBar));
  EXPECT_FALSE(tracker_->WouldTriggerHelpUI(kTrackerTestFeatureQux));
  VerifyEventTriggerEvents(kTrackerTestFeatureFoo, 1);
  VerifyEventTriggerEvents(kTrackerTestFeatureBar, 1);
  VerifyUserActionsTriggerChecks(user_action_tester, 2, 1, 0, 0);
  VerifyUserActionsTriggered(user_action_tester, 1, 1, 0, 0);
  VerifyUserActionsNotTriggered(user_action_tester, 1, 0, 0, 0);
  VerifyUserActionsWouldHaveTriggered(user_action_tester, 0, 0, 0, 0);
  VerifyUserActionsDismissed(user_action_tester, 1);
  VerifyHistograms(true, 1, 1, 0, true, 1, 0, 0, false, 0, 0, 0, false, 0, 0,
                   0);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(TrackerImplTest, TestWouldTriggerWithUpdatedConfig) {
  // Ensure all initialization is finished.
  StoringInitializedCallback callback;
  tracker_->AddOnInitializedCallback(base::BindOnce(
      &StoringInitializedCallback::OnInitialized, base::Unretained(&callback)));
  base::RunLoop().RunUntilIdle();
  base::UserActionTester user_action_tester;

  // Initially, foo would have been shown.
  EXPECT_TRUE(tracker_->WouldTriggerHelpUI(kTrackerTestFeatureFoo));

  FeatureConfig config;
  config.valid = false;
  config.used.name = kTrackerTestFeatureFoo.name + std::string("_used");
  config.trigger.name = kTrackerTestFeatureFoo.name + std::string("_trigger");

  auto provider = std::make_unique<TestConfigurationProvider>();
  provider->SetConfig(config);
  tracker_->UpdateConfig(kTrackerTestFeatureFoo, provider.get());
  EXPECT_FALSE(tracker_->WouldTriggerHelpUI(kTrackerTestFeatureFoo));

  config.valid = true;
  provider->SetConfig(config);
  tracker_->UpdateConfig(kTrackerTestFeatureFoo, provider.get());
  EXPECT_TRUE(tracker_->WouldTriggerHelpUI(kTrackerTestFeatureFoo));
}
#endif

TEST_F(TrackerImplTest, TestTriggerStateInspection) {
  // Before initialization has finished, NOT_READY should always be returned.
  EXPECT_EQ(Tracker::TriggerState::NOT_READY,
            tracker_->GetTriggerState(kTrackerTestFeatureFoo));
  EXPECT_EQ(Tracker::TriggerState::NOT_READY,
            tracker_->GetTriggerState(kTrackerTestFeatureQux));

  // Ensure all initialization is finished.
  StoringInitializedCallback callback;
  tracker_->AddOnInitializedCallback(base::BindOnce(
      &StoringInitializedCallback::OnInitialized, base::Unretained(&callback)));
  base::RunLoop().RunUntilIdle();
  base::UserActionTester user_action_tester;

  EXPECT_EQ(Tracker::TriggerState::HAS_NOT_BEEN_DISPLAYED,
            tracker_->GetTriggerState(kTrackerTestFeatureFoo));
  EXPECT_EQ(Tracker::TriggerState::HAS_NOT_BEEN_DISPLAYED,
            tracker_->GetTriggerState(kTrackerTestFeatureBar));

  // The first time a feature triggers it should be shown.
  EXPECT_TRUE(tracker_->ShouldTriggerHelpUI(kTrackerTestFeatureFoo));
  VerifyEventTriggerEvents(kTrackerTestFeatureFoo, 1u);
  EXPECT_EQ(Tracker::TriggerState::HAS_BEEN_DISPLAYED,
            tracker_->GetTriggerState(kTrackerTestFeatureFoo));

  // Trying to show again should keep state as displayed.
  EXPECT_FALSE(tracker_->ShouldTriggerHelpUI(kTrackerTestFeatureFoo));
  VerifyEventTriggerEvents(kTrackerTestFeatureFoo, 1u);
  EXPECT_EQ(Tracker::TriggerState::HAS_BEEN_DISPLAYED,
            tracker_->GetTriggerState(kTrackerTestFeatureFoo));

  // Other features should also be kept at not having been displayed.
  EXPECT_FALSE(tracker_->ShouldTriggerHelpUI(kTrackerTestFeatureBar));
  VerifyEventTriggerEvents(kTrackerTestFeatureBar, 0);
  EXPECT_EQ(Tracker::TriggerState::HAS_NOT_BEEN_DISPLAYED,
            tracker_->GetTriggerState(kTrackerTestFeatureBar));

  // Dismiss foo and show qux, which should update TriggerState of bar, and keep
  // TriggerState for foo.
  tracker_->Dismissed(kTrackerTestFeatureFoo);
  EXPECT_TRUE(tracker_->ShouldTriggerHelpUI(kTrackerTestFeatureBar));
  VerifyEventTriggerEvents(kTrackerTestFeatureBar, 1);
  EXPECT_EQ(Tracker::TriggerState::HAS_BEEN_DISPLAYED,
            tracker_->GetTriggerState(kTrackerTestFeatureFoo));
  EXPECT_EQ(Tracker::TriggerState::HAS_BEEN_DISPLAYED,
            tracker_->GetTriggerState(kTrackerTestFeatureBar));
}

TEST_F(TrackerImplTest, TestNotifyEvent) {
  StoringInitializedCallback callback;
  tracker_->AddOnInitializedCallback(base::BindOnce(
      &StoringInitializedCallback::OnInitialized, base::Unretained(&callback)));
  base::RunLoop().RunUntilIdle();
  base::UserActionTester user_action_tester;

  tracker_->NotifyEvent("foo");
  tracker_->NotifyEvent("foo");
  tracker_->NotifyEvent("bar");
  tracker_->NotifyEvent(kTrackerTestFeatureFoo.name + std::string("_used"));
  tracker_->NotifyEvent(kTrackerTestFeatureFoo.name + std::string("_trigger"));

  // Used event will record both NotifyEvent and NotifyUsedEvent. Explicitly
  // specify the whole user action string here.
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "InProductHelp.NotifyUsedEvent.test_foo"));
  EXPECT_EQ(2, user_action_tester.GetActionCount(
                   "InProductHelp.NotifyEvent.test_foo"));
  EXPECT_EQ(0, user_action_tester.GetActionCount(
                   "InProductHelp.NotifyUsedEvent.test_bar"));
  EXPECT_EQ(0, user_action_tester.GetActionCount(
                   "InProductHelp.NotifyEvent.test_bar"));

  Event foo_event = event_store_->GetEvent("foo");
  ASSERT_EQ(1, foo_event.events_size());
  EXPECT_EQ(1u, foo_event.events(0).day());
  EXPECT_EQ(2u, foo_event.events(0).count());

  Event bar_event = event_store_->GetEvent("bar");
  ASSERT_EQ(1, bar_event.events_size());
  EXPECT_EQ(1u, bar_event.events(0).day());
  EXPECT_EQ(1u, bar_event.events(0).count());
}

#if !BUILDFLAG(IS_ANDROID)

TEST_F(TrackerImplTest, TestNotifyUsedEvent) {
  StoringInitializedCallback callback;
  tracker_->AddOnInitializedCallback(base::BindOnce(
      &StoringInitializedCallback::OnInitialized, base::Unretained(&callback)));
  base::RunLoop().RunUntilIdle();
  base::UserActionTester user_action_tester;

  tracker_->NotifyUsedEvent(kTrackerTestFeatureFoo);

  // Used event will record both NotifyEvent and NotifyUsedEvent. Explicitly
  // specify the whole user action string here.
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "InProductHelp.NotifyUsedEvent.test_foo"));
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "InProductHelp.NotifyEvent.test_foo"));
  EXPECT_EQ(0, user_action_tester.GetActionCount(
                   "InProductHelp.NotifyUsedEvent.test_bar"));
  EXPECT_EQ(0, user_action_tester.GetActionCount(
                   "InProductHelp.NotifyEvent.test_bar"));

  Event event = event_store_->GetEvent("test_foo_used");
  EXPECT_EQ(1, event.events_size());
}

TEST_F(TrackerImplTest, TestClearEventData) {
  StoringInitializedCallback callback;
  tracker_->AddOnInitializedCallback(base::BindOnce(
      &StoringInitializedCallback::OnInitialized, base::Unretained(&callback)));
  base::RunLoop().RunUntilIdle();
  base::UserActionTester user_action_tester;

  tracker_->NotifyUsedEvent(kTrackerTestFeatureFoo);
  tracker_->NotifyUsedEvent(kTrackerTestFeatureBaz);
  tracker_->ClearEventData(kTrackerTestFeatureFoo);

  // Test clearing used events.
  Event event = event_store_->GetEvent("test_foo_used");
  EXPECT_EQ(0, event.events_size());
  event = event_store_->GetEvent("test_baz_used");
  EXPECT_EQ(1, event.events_size());

  // Test clearing other events.
  tracker_->NotifyEvent("test_event_event");
  EXPECT_EQ(1, event_store_->GetEvent("test_event_event").events_size());
  tracker_->ClearEventData(kTrackerTestFeatureEvent);
  EXPECT_EQ(0, event_store_->GetEvent("test_event_event").events_size());
}

#endif  // !BUILDFLAG(IS_ANDROID)

TEST_F(TrackerImplTest, ShouldPassThroughAcquireDisplayLock) {
  auto lock_handle = std::make_unique<DisplayLockHandle>(base::DoNothing());
  DisplayLockHandle* lock_handle_ptr = lock_handle.get();
  display_lock_controller_->SetNextDisplayLockHandle(std::move(lock_handle));
  EXPECT_EQ(lock_handle_ptr, tracker_->AcquireDisplayLock().get());
}

// Checks that the time is correctly logged when an IPH is presented.
TEST_F(TrackerImplTest, ShownTimeLogged) {
  // Ensure all initialization is finished.
  StoringInitializedCallback callback;
  tracker_->AddOnInitializedCallback(base::BindOnce(
      &StoringInitializedCallback::OnInitialized, base::Unretained(&callback)));
  base::RunLoop().RunUntilIdle();
  const char histogram_name[] = "InProductHelp.ShownTime.test_foo";

  base::Time now = base::Time::Now();
  time_provider_->SetCurrentTime(now);

  // Start the timer by allowing the IPH.
  EXPECT_TRUE(tracker_->ShouldTriggerHelpUI(kTrackerTestFeatureFoo));
  histogram_tester_.ExpectTotalCount(histogram_name, 0);

  // Fake running the clock, where the IPH is displayed.
  time_provider_->SetCurrentTime(now + base::Seconds(3));

  // Dismiss the IPH and assert that the ShownTime is correctly logged.
  tracker_->Dismissed(kTrackerTestFeatureFoo);
  histogram_tester_.ExpectTotalCount(histogram_name, 1);
  histogram_tester_.ExpectUniqueTimeSample(histogram_name, base::Seconds(3), 1);
}

// Checks that the time is not logged when the feature is `tracking_only`.
TEST_F(TrackerImplTest, TrackingOnly_ShownTimeNotLogged) {
  // Ensure all initialization is finished.
  StoringInitializedCallback callback;
  tracker_->AddOnInitializedCallback(base::BindOnce(
      &StoringInitializedCallback::OnInitialized, base::Unretained(&callback)));
  base::RunLoop().RunUntilIdle();
  const char histogram_name[] = "InProductHelp.ShownTime.test_baz";

  base::Time now = base::Time::Now();
  time_provider_->SetCurrentTime(now);

  // Start the timer by allowing the IPH.
  EXPECT_TRUE(tracker_->ShouldTriggerHelpUI(kTrackerTestFeatureFoo));
  histogram_tester_.ExpectTotalCount(histogram_name, 0);

  // Fake running the clock, where the IPH is displayed.
  time_provider_->SetCurrentTime(now + base::Seconds(3));

  // Dismiss the IPH and assert that the ShownTime is not logged.
  tracker_->Dismissed(kTrackerTestFeatureFoo);
  histogram_tester_.ExpectTotalCount(histogram_name, 0);
}

// Base class for any tests that specifically require a
// |OnceConditionValidator|.
class OnceConditionTrackerImplTest : public TrackerImplTest {
 public:
  OnceConditionTrackerImplTest() = default;
  ~OnceConditionTrackerImplTest() override = default;

 protected:
  std::unique_ptr<ConditionValidator> CreateConditionValidator() override {
    auto once_condition_validator = std::make_unique<OnceConditionValidator>();
    once_condition_validator_ = once_condition_validator.get();
    return once_condition_validator;
  }
  raw_ptr<OnceConditionValidator> once_condition_validator_;
};

// Checks that the times are logged even when multiple IPH are presented.
TEST_F(OnceConditionTrackerImplTest, MultipleShownTimeLogged) {
  // Ensure all initialization is finished.
  StoringInitializedCallback callback;
  tracker_->AddOnInitializedCallback(base::BindOnce(
      &StoringInitializedCallback::OnInitialized, base::Unretained(&callback)));
  base::RunLoop().RunUntilIdle();
  const char histogram_name_foo[] = "InProductHelp.ShownTime.test_foo";
  const char histogram_name_bar[] = "InProductHelp.ShownTime.test_bar";

  once_condition_validator_->AllowMultipleFeaturesForTesting(true);

  base::Time start = base::Time::Now();
  time_provider_->SetCurrentTime(start);

  // Start the timer by allowing a first IPH.
  EXPECT_TRUE(tracker_->ShouldTriggerHelpUI(kTrackerTestFeatureFoo));
  histogram_tester_.ExpectTotalCount(histogram_name_foo, 0);
  histogram_tester_.ExpectTotalCount(histogram_name_bar, 0);

  // Fake running the clock, where the first IPH is displayed.
  time_provider_->SetCurrentTime(start + base::Seconds(1));

  // Start a second timer by allowing a second IPH.
  EXPECT_TRUE(tracker_->ShouldTriggerHelpUI(kTrackerTestFeatureBar));
  histogram_tester_.ExpectTotalCount(histogram_name_foo, 0);
  histogram_tester_.ExpectTotalCount(histogram_name_bar, 0);

  // Fake running the clock while both are presented.
  time_provider_->SetCurrentTime(start + base::Seconds(2));

  // Dismiss the first IPH and assert that the ShownTime is correctly logged.
  tracker_->Dismissed(kTrackerTestFeatureFoo);
  histogram_tester_.ExpectTotalCount(histogram_name_foo, 1);
  histogram_tester_.ExpectTotalCount(histogram_name_bar, 0);
  histogram_tester_.ExpectUniqueTimeSample(histogram_name_foo, base::Seconds(2),
                                           1);

  // Fake running the clock, where the first IPH is displayed.
  time_provider_->SetCurrentTime(start + base::Seconds(4));

  // Dismiss the second IPH and assert that the ShownTime is correctly logged.
  tracker_->Dismissed(kTrackerTestFeatureBar);
  histogram_tester_.ExpectTotalCount(histogram_name_foo, 1);
  histogram_tester_.ExpectTotalCount(histogram_name_bar, 1);
  histogram_tester_.ExpectUniqueTimeSample(histogram_name_foo, base::Seconds(2),
                                           1);
  histogram_tester_.ExpectUniqueTimeSample(histogram_name_bar, base::Seconds(3),
                                           1);
}

namespace test {

class ScopedIphFeatureListTest : public TrackerImplTest {
 public:
  ScopedIphFeatureListTest() = default;
  ~ScopedIphFeatureListTest() override = default;

  void SetUp() override {
    TrackerImplTest::SetUp();

    // Ensure all initialization is finished.
    StoringInitializedCallback callback;
    tracker_->AddOnInitializedCallback(
        base::BindOnce(&StoringInitializedCallback::OnInitialized,
                       base::Unretained(&callback)));
    base::RunLoop().RunUntilIdle();
  }
};

TEST_F(ScopedIphFeatureListTest, InitWithNoFeaturesAllowed) {
  ScopedIphFeatureList list;
  list.InitWithNoFeaturesAllowed();
  // Init should not have enabled any features.
  EXPECT_FALSE(base::FeatureList::IsEnabled(kTrackerTestFeatureFoo));
  EXPECT_FALSE(tracker_->ShouldTriggerHelpUI(kTrackerTestFeatureFoo));
}

TEST_F(ScopedIphFeatureListTest, InitWithNoFeaturesAllowed_AllowedAfterReset) {
  ScopedIphFeatureList list;
  list.InitWithNoFeaturesAllowed();
  list.Reset();
  EXPECT_TRUE(tracker_->ShouldTriggerHelpUI(kTrackerTestFeatureFoo));
  tracker_->Dismissed(kTrackerTestFeatureFoo);
}

TEST_F(ScopedIphFeatureListTest,
       InitWithNoFeaturesAllowed_AllowedAfterDestruct) {
  {
    ScopedIphFeatureList list;
    list.InitWithNoFeaturesAllowed();
  }
  EXPECT_TRUE(tracker_->ShouldTriggerHelpUI(kTrackerTestFeatureFoo));
  tracker_->Dismissed(kTrackerTestFeatureFoo);
}

TEST_F(ScopedIphFeatureListTest, InitWithExistingFeatures) {
  ScopedIphFeatureList list;
  list.InitWithExistingFeatures({kTrackerTestFeatureFoo});
  // Init should not have enabled any features.
  EXPECT_FALSE(base::FeatureList::IsEnabled(kTrackerTestFeatureFoo));

  EXPECT_FALSE(tracker_->ShouldTriggerHelpUI(kTrackerTestFeatureBar));
  EXPECT_TRUE(tracker_->ShouldTriggerHelpUI(kTrackerTestFeatureFoo));
  tracker_->Dismissed(kTrackerTestFeatureFoo);
}

TEST_F(ScopedIphFeatureListTest, InitWithExistingFeatures_AllowedAfterReset) {
  ScopedIphFeatureList list;
  list.InitWithExistingFeatures({kTrackerTestFeatureFoo});
  list.Reset();

  EXPECT_TRUE(tracker_->ShouldTriggerHelpUI(kTrackerTestFeatureBar));
  tracker_->Dismissed(kTrackerTestFeatureBar);
}

TEST_F(ScopedIphFeatureListTest,
       InitWithExistingFeatures_AllowedAfterDestruct) {
  {
    ScopedIphFeatureList list;
    list.InitWithExistingFeatures({kTrackerTestFeatureFoo});
  }

  EXPECT_TRUE(tracker_->ShouldTriggerHelpUI(kTrackerTestFeatureBar));
  tracker_->Dismissed(kTrackerTestFeatureBar);
}

TEST_F(ScopedIphFeatureListTest, InitForDemo) {
  ScopedIphFeatureList list;
  list.InitForDemo(kTrackerTestFeatureFoo);

  EXPECT_TRUE(base::FeatureList::IsEnabled(kIPHDemoMode));
  EXPECT_EQ(kTrackerTestFeatureFoo.name,
            base::GetFieldTrialParamValueByFeature(
                kIPHDemoMode, kIPHDemoModeFeatureChoiceParam));
  EXPECT_TRUE(base::FeatureList::IsEnabled(kTrackerTestFeatureFoo));

  EXPECT_FALSE(tracker_->ShouldTriggerHelpUI(kTrackerTestFeatureBar));
  EXPECT_TRUE(tracker_->ShouldTriggerHelpUI(kTrackerTestFeatureFoo));
  tracker_->Dismissed(kTrackerTestFeatureFoo);
}

TEST_F(ScopedIphFeatureListTest, InitForDemo_Reset) {
  ScopedIphFeatureList list;
  list.InitForDemo(kTrackerTestFeatureFoo);
  list.Reset();

  EXPECT_FALSE(base::FeatureList::IsEnabled(kIPHDemoMode));
  EXPECT_FALSE(base::FeatureList::IsEnabled(kTrackerTestFeatureFoo));

  EXPECT_TRUE(tracker_->ShouldTriggerHelpUI(kTrackerTestFeatureBar));
  tracker_->Dismissed(kTrackerTestFeatureBar);
}

TEST_F(ScopedIphFeatureListTest, InitForDemo_Destruct) {
  {
    ScopedIphFeatureList list;
    list.InitForDemo(kTrackerTestFeatureFoo);
  }

  EXPECT_FALSE(base::FeatureList::IsEnabled(kIPHDemoMode));
  EXPECT_FALSE(base::FeatureList::IsEnabled(kTrackerTestFeatureFoo));

  EXPECT_TRUE(tracker_->ShouldTriggerHelpUI(kTrackerTestFeatureBar));
  tracker_->Dismissed(kTrackerTestFeatureBar);
}

TEST_F(ScopedIphFeatureListTest, InitAndEnableFeatures) {
  ScopedIphFeatureList list;
  list.InitAndEnableFeatures({kTrackerTestFeatureFoo, kTrackerTestFeatureBaz});

  EXPECT_TRUE(base::FeatureList::IsEnabled(kTrackerTestFeatureFoo));
  EXPECT_TRUE(base::FeatureList::IsEnabled(kTrackerTestFeatureBaz));

  EXPECT_FALSE(tracker_->ShouldTriggerHelpUI(kTrackerTestFeatureBar));
  EXPECT_TRUE(tracker_->ShouldTriggerHelpUI(kTrackerTestFeatureFoo));
  tracker_->Dismissed(kTrackerTestFeatureFoo);
}

TEST_F(ScopedIphFeatureListTest, InitAndEnableFeatures_Reset) {
  ScopedIphFeatureList list;
  list.InitAndEnableFeatures({kTrackerTestFeatureFoo, kTrackerTestFeatureBaz});
  list.Reset();

  EXPECT_FALSE(base::FeatureList::IsEnabled(kTrackerTestFeatureFoo));
  EXPECT_FALSE(base::FeatureList::IsEnabled(kTrackerTestFeatureBaz));

  EXPECT_TRUE(tracker_->ShouldTriggerHelpUI(kTrackerTestFeatureBar));
  tracker_->Dismissed(kTrackerTestFeatureBar);
}

TEST_F(ScopedIphFeatureListTest, InitAndEnableFeatures_Destruct) {
  {
    ScopedIphFeatureList list;
    list.InitAndEnableFeatures(
        {kTrackerTestFeatureFoo, kTrackerTestFeatureBaz});
  }

  EXPECT_FALSE(base::FeatureList::IsEnabled(kTrackerTestFeatureFoo));
  EXPECT_FALSE(base::FeatureList::IsEnabled(kTrackerTestFeatureBaz));

  EXPECT_TRUE(tracker_->ShouldTriggerHelpUI(kTrackerTestFeatureBar));
  tracker_->Dismissed(kTrackerTestFeatureBar);
}

TEST_F(ScopedIphFeatureListTest, InitAndEnableFeaturesWithParameters) {
  ScopedIphFeatureList list;
  list.InitAndEnableFeaturesWithParameters(
      {{kTrackerTestFeatureFoo, {{"x_foo", "1"}}},
       {kTrackerTestFeatureBaz, {{"x_bar", "2"}, {"x_baz", "3"}}}});

  EXPECT_TRUE(base::FeatureList::IsEnabled(kTrackerTestFeatureFoo));
  EXPECT_EQ("1", base::GetFieldTrialParamValueByFeature(kTrackerTestFeatureFoo,
                                                        "x_foo"));
  EXPECT_TRUE(base::FeatureList::IsEnabled(kTrackerTestFeatureBaz));
  EXPECT_EQ("2", base::GetFieldTrialParamValueByFeature(kTrackerTestFeatureBaz,
                                                        "x_bar"));
  EXPECT_EQ("3", base::GetFieldTrialParamValueByFeature(kTrackerTestFeatureBaz,
                                                        "x_baz"));

  EXPECT_FALSE(tracker_->ShouldTriggerHelpUI(kTrackerTestFeatureBar));
  EXPECT_TRUE(tracker_->ShouldTriggerHelpUI(kTrackerTestFeatureFoo));
  tracker_->Dismissed(kTrackerTestFeatureFoo);
}

TEST_F(ScopedIphFeatureListTest, InitAndEnableFeaturesWithParameters_Reset) {
  ScopedIphFeatureList list;
  list.InitAndEnableFeaturesWithParameters(
      {{kTrackerTestFeatureFoo, {{"x_foo", "1"}}},
       {kTrackerTestFeatureBaz, {{"x_bar", "2"}, {"x_baz", "3"}}}});
  list.Reset();

  EXPECT_FALSE(base::FeatureList::IsEnabled(kTrackerTestFeatureFoo));
  EXPECT_FALSE(base::FeatureList::IsEnabled(kTrackerTestFeatureBaz));

  EXPECT_TRUE(tracker_->ShouldTriggerHelpUI(kTrackerTestFeatureBar));
  tracker_->Dismissed(kTrackerTestFeatureBar);
}

TEST_F(ScopedIphFeatureListTest, InitAndEnableFeaturesWithParameters_Destruct) {
  {
    ScopedIphFeatureList list;
    list.InitAndEnableFeaturesWithParameters(
        {{kTrackerTestFeatureFoo, {{"x_foo", "1"}}},
         {kTrackerTestFeatureBaz, {{"x_bar", "2"}, {"x_baz", "3"}}}});
  }

  EXPECT_FALSE(base::FeatureList::IsEnabled(kTrackerTestFeatureFoo));
  EXPECT_FALSE(base::FeatureList::IsEnabled(kTrackerTestFeatureBaz));

  EXPECT_TRUE(tracker_->ShouldTriggerHelpUI(kTrackerTestFeatureBar));
  tracker_->Dismissed(kTrackerTestFeatureBar);
}

TEST_F(ScopedIphFeatureListTest, NestedScopes) {
  ScopedIphFeatureList list1;
  ScopedIphFeatureList list2;
  ScopedIphFeatureList list3;
  list1.InitWithNoFeaturesAllowed();
  list2.InitWithExistingFeatures({kTrackerTestFeatureFoo});
  EXPECT_FALSE(tracker_->ShouldTriggerHelpUI(kTrackerTestFeatureBar));
  list3.InitWithExistingFeatures({kTrackerTestFeatureBar});
  EXPECT_TRUE(tracker_->ShouldTriggerHelpUI(kTrackerTestFeatureBar));
  tracker_->Dismissed(kTrackerTestFeatureBar);
  EXPECT_TRUE(tracker_->ShouldTriggerHelpUI(kTrackerTestFeatureFoo));
  tracker_->Dismissed(kTrackerTestFeatureFoo);
}

TEST_F(ScopedIphFeatureListTest, NestedScopes_DestructInWrongOrder) {
  ScopedIphFeatureList list1;
  ScopedIphFeatureList list2;
  ScopedIphFeatureList list3;
  list1.InitWithNoFeaturesAllowed();
  list2.InitWithExistingFeatures({kTrackerTestFeatureFoo});
  list3.InitWithExistingFeatures({kTrackerTestFeatureBar});
  list1.Reset();
  list2.Reset();
  // Destroyed the scope that allowed Foo, but not the one that allowed Bar.
  EXPECT_FALSE(tracker_->ShouldTriggerHelpUI(kTrackerTestFeatureFoo));
  EXPECT_TRUE(tracker_->ShouldTriggerHelpUI(kTrackerTestFeatureBar));
  tracker_->Dismissed(kTrackerTestFeatureBar);
  list3.Reset();

  // Now there are no more scopes, so all IPH are allowed.
  EXPECT_TRUE(tracker_->ShouldTriggerHelpUI(kTrackerTestFeatureFoo));
  tracker_->Dismissed(kTrackerTestFeatureFoo);
}

TEST_F(ScopedIphFeatureListTest, NestedScopes_SameFeature) {
  ScopedIphFeatureList list1;
  list1.InitWithExistingFeatures({kTrackerTestFeatureBar});
  {
    ScopedIphFeatureList list2;
    list2.InitWithExistingFeatures(
        {kTrackerTestFeatureFoo, kTrackerTestFeatureBar});
    EXPECT_TRUE(tracker_->ShouldTriggerHelpUI(kTrackerTestFeatureFoo));
    tracker_->Dismissed(kTrackerTestFeatureFoo);
  }
  EXPECT_TRUE(tracker_->ShouldTriggerHelpUI(kTrackerTestFeatureBar));
  tracker_->Dismissed(kTrackerTestFeatureBar);
}

// Test class for tests that require an actual
// |FeatureConfigConditionValidator|.
class FeatureConfigConditionValidatorTrackerTest : public TrackerImplTest {
 public:
  FeatureConfigConditionValidatorTrackerTest() = default;
  ~FeatureConfigConditionValidatorTrackerTest() override = default;

 protected:
  std::unique_ptr<ConditionValidator> CreateConditionValidator() override {
    return std::make_unique<FeatureConfigConditionValidator>();
  }
};

TEST_F(FeatureConfigConditionValidatorTrackerTest, GroupRulesApplied) {
  // Set up the group config to only allow 1 feature from its group to be
  // displayed.
  GroupConfig custom_group_config;
  custom_group_config.valid = true;
  custom_group_config.trigger.name = "custom_group_trigger";
  custom_group_config.trigger.comparator = Comparator(EQUAL, 0);
  custom_group_config.trigger.window = 1u;
  custom_group_config.trigger.storage = 1u;
  configuration_->SetConfiguration(&kTrackerTestGroupOne, custom_group_config);

  ScopedIphFeatureList list;
  list.InitAndEnableFeatures(
      {kTrackerTestFeatureFoo, kTrackerTestFeatureBar, kTrackerTestGroupOne});

  // Ensure all initialization is finished.
  StoringInitializedCallback callback;
  tracker_->AddOnInitializedCallback(base::BindOnce(
      &StoringInitializedCallback::OnInitialized, base::Unretained(&callback)));
  base::RunLoop().RunUntilIdle();
  base::UserActionTester user_action_tester;

  // The first feature should display, but the second one should not, as they
  // share a group that only allows its features to be displayed once.
  EXPECT_TRUE(tracker_->ShouldTriggerHelpUI(kTrackerTestFeatureFoo));
  tracker_->Dismissed(kTrackerTestFeatureFoo);
  EXPECT_FALSE(tracker_->ShouldTriggerHelpUI(kTrackerTestFeatureBar));
}

TEST_F(FeatureConfigConditionValidatorTrackerTest, TestTriggeringWithSnooze) {
  ScopedIphFeatureList list;
  list.InitAndEnableFeatures(
      {kTrackerTestFeatureSnooze, kTrackerTestFeatureFoo});

  // Ensure all initialization is finished.
  StoringInitializedCallback callback;
  tracker_->AddOnInitializedCallback(base::BindOnce(
      &StoringInitializedCallback::OnInitialized, base::Unretained(&callback)));
  base::RunLoop().RunUntilIdle();

  base::Time now = base::Time::Now();
  time_provider_->SetCurrentTime(now);
  time_provider_->SetCurrentDay(1u);

  // The first time a feature with snooze params triggers, it should be shown
  // with snooze.
  Tracker::TriggerDetails trigger_details =
      tracker_->ShouldTriggerHelpUIWithSnooze(kTrackerTestFeatureSnooze);
  EXPECT_TRUE(trigger_details.ShouldShowIph());
  EXPECT_TRUE(trigger_details.ShouldShowSnooze());

  Event snooze_event = event_store_->GetEvent(
      configuration_->GetFeatureConfig(kTrackerTestFeatureSnooze).trigger.name);
  ASSERT_EQ(1, snooze_event.events_size());
  ASSERT_EQ(1u, snooze_event.events(0).day());
  ASSERT_EQ(1u, snooze_event.events(0).count());
  ASSERT_EQ(0u, snooze_event.events(0).snooze_count());

  tracker_->DismissedWithSnooze(kTrackerTestFeatureSnooze,
                                Tracker::SnoozeAction::SNOOZED);
  snooze_event = event_store_->GetEvent(
      configuration_->GetFeatureConfig(kTrackerTestFeatureSnooze).trigger.name);
  ASSERT_EQ(1u, snooze_event.events(0).snooze_count());
  ASSERT_EQ(now.ToDeltaSinceWindowsEpoch().InMicroseconds(),
            snooze_event.last_snooze_time_us());
  ASSERT_EQ(false, snooze_event.snooze_dismissed());

  // Now, the feature should be on snooze.
  trigger_details =
      tracker_->ShouldTriggerHelpUIWithSnooze(kTrackerTestFeatureSnooze);
  EXPECT_FALSE(trigger_details.ShouldShowIph());
  EXPECT_FALSE(trigger_details.ShouldShowSnooze());

  // Advance the clock so the snooze expires. Then the feature should be ready
  // to show again.
  time_provider_->SetCurrentTime(now + base::Days(8));
  trigger_details =
      tracker_->ShouldTriggerHelpUIWithSnooze(kTrackerTestFeatureSnooze);
  EXPECT_TRUE(trigger_details.ShouldShowIph());
  EXPECT_TRUE(trigger_details.ShouldShowSnooze());

  tracker_->DismissedWithSnooze(kTrackerTestFeatureSnooze,
                                Tracker::SnoozeAction::DISMISSED);
  snooze_event = event_store_->GetEvent(
      configuration_->GetFeatureConfig(kTrackerTestFeatureSnooze).trigger.name);
  ASSERT_EQ(1u, snooze_event.events(0).snooze_count());
  ASSERT_EQ(true, snooze_event.snooze_dismissed());

  EXPECT_TRUE(tracker_->ShouldTriggerHelpUI(kTrackerTestFeatureFoo));
}

}  // namespace test

}  // namespace feature_engagement
