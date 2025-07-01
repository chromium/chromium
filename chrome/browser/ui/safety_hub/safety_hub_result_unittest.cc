// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/safety_hub_result.h"

#include <memory>
#include <string>

#include "base/json/values_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockSafetyHubResult : public SafetyHubResult {
 public:
  explicit MockSafetyHubResult(base::Time timestamp = base::Time::Now())
      : SafetyHubResult(timestamp) {}
  ~MockSafetyHubResult() override = default;

  std::unique_ptr<SafetyHubResult> Clone() const override {
    return std::make_unique<MockSafetyHubResult>(*this);
  }

  base::Value::Dict ToDictValue() const override { return BaseToDictValue(); }

  bool IsTriggerForMenuNotification() const override { return true; }

  bool WarrantsNewMenuNotification(
      const base::Value::Dict& previousResult) const override {
    return true;
  }

  std::u16string GetNotificationString() const override {
    return std::u16string();
  }

  int GetNotificationCommandId() const override { return 0; }

  int GetVal() const { return val_; }

  void SetVal(int val) { val_ = val; }

  void IncreaseVal() { ++val_; }

 private:
  int val_ = 0;
};

}  // namespace

class SafetyHubResultTest : public testing::Test {};

TEST_F(SafetyHubResultTest, ResultBaseToDict) {
  base::Time time = base::Time::Now() - base::Days(5);
  auto result = std::make_unique<MockSafetyHubResult>(time);
  EXPECT_EQ(result->timestamp(), time);
  // The timestamp saved in the dict should be the Value of time.
  base::Value::Dict dict = result->ToDictValue();
  EXPECT_EQ(*dict.Find(kSafetyHubTimestampResultKey), base::TimeToValue(time));
}
