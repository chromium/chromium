// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/safety_hub_service.h"

#include <memory>
#include <utility>

#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
constexpr base::TimeDelta kUpdateIntervalForTest = base::Days(7);

class MockSafetyHubService : public SafetyHubService {
 public:
  // Returns the number of times that the UpdateOnBackgroundThread function was
  // called.
  int GetNumUpdates() const { return num_updates_; }

 protected:
  // For testing purposes, the UpdateOnBackgroundThread function will be
  // executed every seven days.
  base::TimeDelta GetRepeatedUpdateInterval() override {
    return kUpdateIntervalForTest;
  }

  std::unique_ptr<Result> UpdateOnBackgroundThread() override {
    ++num_updates_;
    return std::make_unique<Result>();
  }

 private:
  int num_updates_ = 0;
};

class MockObserver : public SafetyHubService::Observer {
 public:
  void OnResultAvailable(const SafetyHubService::Result* result) override {
    ++num_calls_;
  }

  // Returns the number of times that the observer was notified.
  int GetNumCalls() const { return num_calls_; }

 private:
  int num_calls_ = 0;
};

}  // namespace

class SafetyHubServiceTest : public testing::Test {
 public:
  SafetyHubServiceTest() = default;
  ~SafetyHubServiceTest() override = default;

  void SetUp() override { service_ = std::make_unique<MockSafetyHubService>(); }

  void TearDown() override { service_->Shutdown(); }

 protected:
  void FastForwardBy(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  MockSafetyHubService* service() { return service_.get(); }

 private:
  std::unique_ptr<MockSafetyHubService> service_;

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(SafetyHubServiceTest, ManageObservers) {
  auto observer = std::make_unique<MockObserver>();
  // The list of observers should initially be empty.
  EXPECT_FALSE(service()->observers_.HasObserver(observer.get()));
  service()->AddObserver(observer.get());
  // After adding the observer, the observer should be registered in the list of
  // observers, but should not be called yet.
  EXPECT_TRUE(service()->observers_.HasObserver(observer.get()));
  EXPECT_EQ(observer->GetNumCalls(), 0);
  auto result = std::make_unique<SafetyHubService::Result>();
  // Notify all observers.
  service()->NotifyObservers(result.get());
  // Ensure that the observer was called just once.
  EXPECT_EQ(observer->GetNumCalls(), 1);
  service()->NotifyObservers(result.get());
  // Ensure that the observer was called again.
  EXPECT_EQ(observer->GetNumCalls(), 2);
  service()->RemoveObserver(observer.get());
  // The observer should be removed from the list of observers, and not get
  // called again when observers are notified.
  EXPECT_FALSE(service()->observers_.HasObserver(observer.get()));
  service()->NotifyObservers(result.get());
  EXPECT_EQ(observer->GetNumCalls(), 2);
}

TEST_F(SafetyHubServiceTest, UpdateOnBackgroundThread) {
  EXPECT_EQ(service()->GetNumUpdates(), 0);
  // As long as StartRepeatedUpdates() has not been called, no updates should
  // be made.
  FastForwardBy(kUpdateIntervalForTest);
  EXPECT_EQ(service()->GetNumUpdates(), 0);
  // The update will be run asynchronously as soon as StartRepeatedUpdates() is
  // called.
  service()->StartRepeatedUpdates();
  RunUntilIdle();
  EXPECT_EQ(service()->GetNumUpdates(), 1);
  // Move forward a full update interval, which will trigger another update.
  FastForwardBy(kUpdateIntervalForTest);
  RunUntilIdle();
  EXPECT_EQ(service()->GetNumUpdates(), 2);
}
