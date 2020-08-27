// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/do_not_disturb_controller_impl.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace phonehub {
namespace {

class FakeObserver : public DoNotDisturbController::Observer {
 public:
  FakeObserver() = default;
  ~FakeObserver() override = default;

  size_t num_calls() const { return num_calls_; }

  // DoNotDisturbController::Observer:
  void OnDndStateChanged() override { ++num_calls_; }

 private:
  size_t num_calls_ = 0;
};

}  // namespace

class DoNotDisturbControllerImplTest : public testing::Test {
 protected:
  DoNotDisturbControllerImplTest() = default;
  DoNotDisturbControllerImplTest(const DoNotDisturbControllerImplTest&) =
      delete;
  DoNotDisturbControllerImplTest& operator=(
      const DoNotDisturbControllerImplTest&) = delete;
  ~DoNotDisturbControllerImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    controller_ = std::make_unique<DoNotDisturbControllerImpl>();
    controller_->AddObserver(&fake_observer_);
  }

  void TearDown() override { controller_->RemoveObserver(&fake_observer_); }

  bool IsDndEnabled() const { return controller_->IsDndEnabled(); }

  size_t GetNumObserverCalls() const { return fake_observer_.num_calls(); }

 private:
  FakeObserver fake_observer_;
  std::unique_ptr<DoNotDisturbController> controller_;
};

// TODO(https://crbug.com/1106937): Remove this test once we have real
// functionality to test.
TEST_F(DoNotDisturbControllerImplTest, Initialize) {
  EXPECT_FALSE(IsDndEnabled());
}

}  // namespace phonehub
}  // namespace chromeos
