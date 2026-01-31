// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "components/download/internal/background_service/navigation_monitor_impl.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/test_mock_time_task_runner.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace download {
namespace {

class TestNavigationMonitorObserver : public NavigationMonitor::Observer {
 public:
  TestNavigationMonitorObserver(
      scoped_refptr<base::TestMockTimeTaskRunner> task_runner,
      NavigationMonitor* monitor)
      : task_runner_(task_runner),
        monitor_(monitor),
        navigation_in_progress_(false) {}

  TestNavigationMonitorObserver(const TestNavigationMonitorObserver&) = delete;
  TestNavigationMonitorObserver& operator=(
      const TestNavigationMonitorObserver&) = delete;

  ~TestNavigationMonitorObserver() override = default;

  void OnNavigationEvent() override {
    navigation_in_progress_ = monitor_->IsNavigationInProgress();
  }

  void VerifyNavigationState(bool expected) {
    EXPECT_EQ(expected, navigation_in_progress_);
  }

  void VerifyNavigationStateAt(bool expected, int millis) {
    task_runner_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&TestNavigationMonitorObserver::VerifyNavigationState,
                       weak_ptr_factory_.GetWeakPtr(), expected),
        base::Milliseconds(millis));
  }

 private:
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  raw_ptr<NavigationMonitor> monitor_;
  bool navigation_in_progress_;
  base::WeakPtrFactory<TestNavigationMonitorObserver> weak_ptr_factory_{this};
};

class NavigationMonitorImplTest : public testing::Test {
 public:
  NavigationMonitorImplTest()
      : task_runner_(new base::TestMockTimeTaskRunner),
        current_default_handle_(task_runner_) {}

  NavigationMonitorImplTest(const NavigationMonitorImplTest&) = delete;
  NavigationMonitorImplTest& operator=(const NavigationMonitorImplTest&) =
      delete;

  ~NavigationMonitorImplTest() override = default;

  void SetUp() override {
    navigation_monitor_ = std::make_unique<NavigationMonitorImpl>();
    observer_ = std::make_unique<TestNavigationMonitorObserver>(
        task_runner_, navigation_monitor_.get());
    navigation_monitor_->Configure(base::Milliseconds(20),
                                   base::Milliseconds(200));
  }

  void TearDown() override {
    navigation_monitor_->SetObserver(nullptr);
    observer_.reset();
    navigation_monitor_.reset();
  }

  void WaitUntilDone() { task_runner_->FastForwardUntilNoTasksRemain(); }

  void SendNavigationEvent(NavigationEvent event) {
    navigation_monitor_->OnNavigationEvent(event);
  }

  void SendNavigationEventAt(NavigationEvent event, int millis) {
    task_runner_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&NavigationMonitorImplTest::SendNavigationEvent,
                       weak_ptr_factory_.GetWeakPtr(), event),
        base::Milliseconds(millis));
  }

 protected:
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  base::SingleThreadTaskRunner::CurrentDefaultHandle current_default_handle_;
  std::unique_ptr<NavigationMonitorImpl> navigation_monitor_;
  std::unique_ptr<TestNavigationMonitorObserver> observer_;
  base::WeakPtrFactory<NavigationMonitorImplTest> weak_ptr_factory_{this};
};

TEST_F(NavigationMonitorImplTest, NoObserver) {
  SendNavigationEventAt(NavigationEvent::START_NAVIGATION, 5);

  observer_->VerifyNavigationStateAt(false, 0);
  observer_->VerifyNavigationStateAt(false, 10);
  observer_->VerifyNavigationStateAt(false, 100);
  WaitUntilDone();
}

TEST_F(NavigationMonitorImplTest, NavigationTimeout) {
  navigation_monitor_->SetObserver(observer_.get());
  SendNavigationEventAt(NavigationEvent::START_NAVIGATION, 5);

  observer_->VerifyNavigationStateAt(false, 0);
  observer_->VerifyNavigationStateAt(true, 10);
  observer_->VerifyNavigationStateAt(true, 50);
  observer_->VerifyNavigationStateAt(true, 190);
  observer_->VerifyNavigationStateAt(false, 210);
  observer_->VerifyNavigationStateAt(false, 300);
  WaitUntilDone();
}

TEST_F(NavigationMonitorImplTest, UnexpectedNavigationEndCalls) {
  navigation_monitor_->SetObserver(observer_.get());
  SendNavigationEventAt(NavigationEvent::NAVIGATION_COMPLETE, 5);
  SendNavigationEventAt(NavigationEvent::NAVIGATION_COMPLETE, 10);

  observer_->VerifyNavigationStateAt(false, 0);
  observer_->VerifyNavigationStateAt(false, 7);
  observer_->VerifyNavigationStateAt(false, 15);
  observer_->VerifyNavigationStateAt(false, 50);
  observer_->VerifyNavigationStateAt(false, 300);
  WaitUntilDone();
}

TEST_F(NavigationMonitorImplTest, OverlappingNavigations) {
  navigation_monitor_->SetObserver(observer_.get());
  SendNavigationEventAt(NavigationEvent::START_NAVIGATION, 5);
  SendNavigationEventAt(NavigationEvent::START_NAVIGATION, 27);
  SendNavigationEventAt(NavigationEvent::NAVIGATION_COMPLETE, 50);
  SendNavigationEventAt(NavigationEvent::NAVIGATION_COMPLETE, 55);

  observer_->VerifyNavigationStateAt(false, 0);
  observer_->VerifyNavigationStateAt(true, 20);
  observer_->VerifyNavigationStateAt(true, 40);
  observer_->VerifyNavigationStateAt(true, 60);
  observer_->VerifyNavigationStateAt(false, 80);
  observer_->VerifyNavigationStateAt(false, 300);
  WaitUntilDone();
}

TEST_F(NavigationMonitorImplTest, TwoNavigationsShortlyOneAfterAnother) {
  navigation_monitor_->SetObserver(observer_.get());
  SendNavigationEventAt(NavigationEvent::START_NAVIGATION, 5);
  SendNavigationEventAt(NavigationEvent::NAVIGATION_COMPLETE, 10);
  SendNavigationEventAt(NavigationEvent::START_NAVIGATION, 27);
  SendNavigationEventAt(NavigationEvent::NAVIGATION_COMPLETE, 50);

  observer_->VerifyNavigationStateAt(false, 0);
  observer_->VerifyNavigationStateAt(true, 7);
  observer_->VerifyNavigationStateAt(true, 20);
  observer_->VerifyNavigationStateAt(true, 40);
  observer_->VerifyNavigationStateAt(true, 60);
  observer_->VerifyNavigationStateAt(false, 80);
  observer_->VerifyNavigationStateAt(false, 300);
  WaitUntilDone();
}

TEST_F(NavigationMonitorImplTest, NavigationSpacedApartLongTime) {
  navigation_monitor_->SetObserver(observer_.get());
  SendNavigationEventAt(NavigationEvent::START_NAVIGATION, 5);
  SendNavigationEventAt(NavigationEvent::NAVIGATION_COMPLETE, 10);
  SendNavigationEventAt(NavigationEvent::START_NAVIGATION, 60);
  SendNavigationEventAt(NavigationEvent::NAVIGATION_COMPLETE, 70);

  observer_->VerifyNavigationStateAt(false, 0);
  observer_->VerifyNavigationStateAt(true, 7);
  observer_->VerifyNavigationStateAt(true, 15);
  observer_->VerifyNavigationStateAt(false, 40);
  observer_->VerifyNavigationStateAt(true, 65);
  observer_->VerifyNavigationStateAt(true, 80);
  observer_->VerifyNavigationStateAt(false, 100);
  WaitUntilDone();
}

}  // namespace
}  // namespace download
