// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/throttle/throttle_service.h"

#include <utility>

#include "content/public/test/test_browser_context.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

constexpr const char kFirstObserverName[] = "o1";
constexpr const char kSecondObserverName[] = "o2";

class TestObserver : public ThrottleService::ServiceObserver {
 public:
  TestObserver() = default;
  ~TestObserver() override = default;

  // ThrottleService::Observer:
  void OnThrottle(bool was_throttled) override {
    last_was_throttled_ = was_throttled;
    ++update_count_;
  }

  int GetUpdateCountAndReset() {
    const int update_count = update_count_;
    update_count_ = 0;
    return update_count;
  }

  bool last_was_throttled() const { return last_was_throttled_; }

 private:
  int update_count_ = 0;
  bool last_was_throttled_ = false;

  TestObserver(TestObserver const&) = delete;
  TestObserver& operator=(TestObserver const&) = delete;
};

}  // namespace

class TestThrottleService : public ThrottleService {
 public:
  using ThrottleService::ThrottleService;

  size_t throttle_instance_count() const { return throttle_instance_count_; }

  size_t uma_count() { return record_uma_counter_; }

  bool last_should_throttle() const { return last_should_throttle_; }

  const std::string& last_recorded_observer_name() {
    return last_recorded_observer_name_;
  }

 private:
  void ThrottleInstance(bool should_throttle) override {
    ++throttle_instance_count_;
    last_should_throttle_ = should_throttle;
  }

  void RecordCpuRestrictionDisabledUMA(const std::string& observer_name,
                                       base::TimeDelta delta) override {
    ++record_uma_counter_;
    last_recorded_observer_name_ = observer_name;
  }

  size_t throttle_instance_count_{0};
  size_t record_uma_counter_{0};
  std::string last_recorded_observer_name_;
  bool last_should_throttle_ = false;
};

class ThrottleServiceTest : public testing::Test {
 public:
  ThrottleServiceTest() {
    std::vector<std::unique_ptr<ThrottleObserver>> observers;
    observers.push_back(std::make_unique<ThrottleObserver>(kFirstObserverName));
    observers.push_back(
        std::make_unique<ThrottleObserver>(kSecondObserverName));
    service_.SetObserversForTesting(std::move(observers));
  }

  ThrottleServiceTest(const ThrottleServiceTest&) = delete;
  ThrottleServiceTest& operator=(const ThrottleServiceTest&) = delete;

 protected:
  TestThrottleService* service() { return &service_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::TestBrowserContext browser_context_;
  TestThrottleService service_{&browser_context_};
};

TEST_F(ThrottleServiceTest, TestConstructDestruct) {}

// Tests that the ThrottleService calls ThrottleInstance with the correct
// throttling when there is a change in observers, but skips the call if new
// throttling is same as before.
TEST_F(ThrottleServiceTest, TestOnObserverStateChanged) {
  EXPECT_EQ(0U, service()->throttle_instance_count());

  // Initially, it is throttled.
  service()->NotifyObserverStateChangedForTesting();
  EXPECT_EQ(1U, service()->throttle_instance_count());
  EXPECT_TRUE(service()->last_should_throttle());

  // Activate one of two observers. Verify it is unthrottled.
  service()->observers_for_testing()[0]->SetActive(true);
  EXPECT_EQ(2U, service()->throttle_instance_count());
  EXPECT_FALSE(service()->last_should_throttle());

  // Activate the other observer too. Verify ThrottleInstance() is not called.
  service()->observers_for_testing()[1]->SetActive(true);
  EXPECT_EQ(2U, service()->throttle_instance_count());
  EXPECT_FALSE(service()->last_should_throttle());

  // Deactivate one observer. Verify ThrottleInstance() is not called.
  service()->observers_for_testing()[1]->SetActive(false);
  EXPECT_EQ(2U, service()->throttle_instance_count());
  EXPECT_FALSE(service()->last_should_throttle());

  // Deactivate the other observer too. Verify ThrottleInstance() is called.
  service()->observers_for_testing()[0]->SetActive(false);
  EXPECT_EQ(3U, service()->throttle_instance_count());
  EXPECT_TRUE(service()->last_should_throttle());
}

// Tests that ArcInstanceThrottle records the duration that the effective
// observer is active.
TEST_F(ThrottleServiceTest, RecordCpuRestrictionDisabledUMA) {
  EXPECT_EQ(0U, service()->uma_count());

  // The effective observer transitions from null to the first one; no UMA
  // is recorded yet.
  service()->observers_for_testing()[0]->SetActive(true);
  EXPECT_EQ(0U, service()->uma_count());

  // The effective observer is still the first one.
  service()->observers_for_testing()[1]->SetActive(true);
  EXPECT_EQ(0U, service()->uma_count());

  // The effective observer transitions from the first one to the second one.
  // UMA should be recorded for the first one.
  service()->observers_for_testing()[0]->SetActive(false);
  EXPECT_EQ(1U, service()->uma_count());
  EXPECT_EQ(service()->observers_for_testing()[0]->name(),
            service()->last_recorded_observer_name());

  // Effective observer transitions from the second one to the first one. UMA
  // should be recorded for the second one.
  service()->observers_for_testing()[0]->SetActive(true);
  EXPECT_EQ(2U, service()->uma_count());
  EXPECT_EQ(service()->observers_for_testing()[1]->name(),
            service()->last_recorded_observer_name());

  // Effective observer transitions from the first one to null; UMA should
  // be recorded for critical_observer.
  service()->observers_for_testing()[1]->SetActive(false);
  service()->observers_for_testing()[0]->SetActive(false);
  EXPECT_EQ(3U, service()->uma_count());
  EXPECT_EQ(service()->observers_for_testing()[0]->name(),
            service()->last_recorded_observer_name());
}

// Tests that verifies enforcement mode.
TEST_F(ThrottleServiceTest, TestEnforced) {
  service()->observers_for_testing()[0]->SetActive(false);
  service()->observers_for_testing()[1]->SetActive(true);
  EXPECT_FALSE(service()->should_throttle());

  // Enforce the first observer which is not active. Verify the service is
  // throttled.
  service()->observers_for_testing()[0]->SetEnforced(true);
  EXPECT_TRUE(service()->should_throttle());

  // Stop enforcing it and verify the service is the service is unthrottled.
  service()->observers_for_testing()[0]->SetEnforced(false);
  EXPECT_FALSE(service()->should_throttle());
}

// Tests that verifies observer notifications.
TEST_F(ThrottleServiceTest, TestObservers) {
  TestObserver test_observer;
  service()->AddServiceObserver(&test_observer);

  // Activate the second observer. Verify that OnThrottle() is called.
  EXPECT_EQ(0, test_observer.GetUpdateCountAndReset());
  service()->observers_for_testing()[1]->SetActive(true);
  EXPECT_FALSE(service()->should_throttle());
  EXPECT_FALSE(test_observer.last_was_throttled());
  EXPECT_EQ(1, test_observer.GetUpdateCountAndReset());

  // Activate the first observer too. Verify that OnThrottle() is NOT called
  // because the throttling is not changed.
  service()->observers_for_testing()[0]->SetActive(true);
  EXPECT_FALSE(service()->should_throttle());
  EXPECT_FALSE(test_observer.last_was_throttled());
  EXPECT_EQ(0, test_observer.GetUpdateCountAndReset());

  // Deactivate both. Verify that OnThrottle() is called.
  service()->observers_for_testing()[0]->SetActive(false);
  EXPECT_EQ(0, test_observer.GetUpdateCountAndReset());  // not yet called
  service()->observers_for_testing()[1]->SetActive(false);
  EXPECT_TRUE(service()->should_throttle());
  EXPECT_TRUE(test_observer.last_was_throttled());
  EXPECT_EQ(1, test_observer.GetUpdateCountAndReset());

  // Remove the observer. Verify that OnThrottle() is no longer called.
  service()->RemoveServiceObserver(&test_observer);
  service()->observers_for_testing()[1]->SetActive(true);
  EXPECT_FALSE(service()->should_throttle());
  EXPECT_EQ(0, test_observer.GetUpdateCountAndReset());
}

// Tests that getting an observer by its name works.
TEST_F(ThrottleServiceTest, TestGetObserverByName) {
  auto* first_observer = service()->GetObserverByName(kFirstObserverName);
  auto* second_observer = service()->GetObserverByName(kSecondObserverName);
  EXPECT_NE(nullptr, first_observer);
  EXPECT_NE(nullptr, second_observer);
  EXPECT_NE(first_observer, second_observer);
  EXPECT_EQ(nullptr, service()->GetObserverByName("NonExistentObserverName"));
}

}  // namespace ash
