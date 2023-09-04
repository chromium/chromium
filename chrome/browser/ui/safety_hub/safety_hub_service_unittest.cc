// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/safety_hub_service.h"

#include <memory>
#include <utility>

#include "base/observer_list_types.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "chrome/browser/ui/safety_hub/safety_hub_test_util.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
constexpr base::TimeDelta kUpdateIntervalForTest = base::Days(7);

class MockSafetyHubResult : public SafetyHubService::Result {
 public:
  ~MockSafetyHubResult() override = default;

  int GetVal() { return val_; }

  void SetVal(int val) { val_ = val; }

  void IncreaseVal() { ++val_; }

 private:
  int val_ = 0;
};

class MockSafetyHubService : public SafetyHubService {
 public:
  // Returns the number of times that the UpdateOnBackgroundThread function was
  // called.
  int GetNumBackgroundUpdates() const { return num_updates_background_; }

  // Returns the number of times that the UpdateOnBackgroundThread function was
  // called.
  int GetNumUIUpdates() const { return num_updates_ui_; }

  // Set the latest result to a specific value - 42 in this case.
  void InitializeLatestResult() override {
    auto init_result = std::make_unique<MockSafetyHubResult>();
    init_result->SetVal(42);
    latest_result_ = std::move(init_result);
  }

 protected:
  // For testing purposes, the UpdateOnBackgroundThread function will be
  // executed every seven days.
  base::TimeDelta GetRepeatedUpdateInterval() override {
    return kUpdateIntervalForTest;
  }

  base::OnceCallback<std::unique_ptr<Result>()> GetBackgroundTask() override {
    auto init_result = std::make_unique<MockSafetyHubResult>();
    return base::BindOnce(&UpdateOnBackgroundThread, std::move(init_result));
  }

  static std::unique_ptr<Result> UpdateOnBackgroundThread(
      std::unique_ptr<Result> result) {
    auto background_result = std::make_unique<MockSafetyHubResult>();
    background_result->IncreaseVal();
    return background_result;
  }

  std::unique_ptr<SafetyHubService::Result> UpdateOnUIThread(
      std::unique_ptr<Result> result) override {
    num_updates_background_ +=
        static_cast<MockSafetyHubResult*>(result.get())->GetVal();
    ++num_updates_ui_;
    return std::make_unique<MockSafetyHubResult>();
  }

  base::WeakPtr<SafetyHubService> GetAsWeakRef() override {
    return weak_factory_.GetWeakPtr();
  }

 private:
  int num_updates_background_ = 0;
  int num_updates_ui_ = 0;

  base::WeakPtrFactory<MockSafetyHubService> weak_factory_{this};
};

class MockObserver : public SafetyHubService::Observer {
 public:
  void OnResultAvailable(const SafetyHubService::Result* result) override {
    ++num_calls_;
    if (callback_) {
      callback_.Run();
    }
  }

  void SetCallback(const base::RepeatingClosure& callback) {
    callback_ = callback;
  }

  // Returns the number of times that the observer was notified.
  int GetNumCalls() const { return num_calls_; }

 private:
  int num_calls_ = 0;
  base::RepeatingClosure callback_;
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
  auto result = std::make_unique<MockSafetyHubResult>();
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
  EXPECT_EQ(service()->GetNumUIUpdates(), 0);
  EXPECT_EQ(service()->GetNumBackgroundUpdates(), 0);
  // As long as StartRepeatedUpdates() has not been called, no updates should
  // be made.
  FastForwardBy(kUpdateIntervalForTest);
  EXPECT_EQ(service()->GetNumUIUpdates(), 0);
  EXPECT_EQ(service()->GetNumBackgroundUpdates(), 0);
  // The update will be run asynchronously as soon as StartRepeatedUpdates() is
  // called.
  base::RunLoop loop;
  auto observer = std::make_shared<MockObserver>();
  observer->SetCallback(loop.QuitClosure());
  service()->AddObserver(observer.get());
  service()->StartRepeatedUpdates();
  loop.Run();
  EXPECT_EQ(service()->GetNumUIUpdates(), 1);
  EXPECT_EQ(service()->GetNumBackgroundUpdates(), 1);
  // Move forward a full update interval, which will trigger another update.
  base::RunLoop loop2;
  observer->SetCallback(loop2.QuitClosure());
  FastForwardBy(kUpdateIntervalForTest);
  loop2.Run();
  EXPECT_EQ(service()->GetNumUIUpdates(), 2);
  EXPECT_EQ(service()->GetNumBackgroundUpdates(), 2);
}

TEST_F(SafetyHubServiceTest, GetCachedResult) {
  // The mock service does not initialize the latest result on construction, so
  // the cached result should be null.
  absl::optional<SafetyHubService::Result*> opt_result1 =
      service()->GetCachedResult();
  EXPECT_FALSE(opt_result1.has_value());

  // We have to call InitializeLatestResult here as the service is not created
  // using a factory (which usually calls it).
  service()->InitializeLatestResult();
  absl::optional<SafetyHubService::Result*> opt_result2 =
      service()->GetCachedResult();
  EXPECT_TRUE(opt_result2.has_value());
  auto* result = static_cast<MockSafetyHubResult*>(opt_result2.value());
  EXPECT_EQ(result->GetVal(), 42);
}
