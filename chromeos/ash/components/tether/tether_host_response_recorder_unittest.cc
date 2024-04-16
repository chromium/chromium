// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/tether_host_response_recorder.h"

#include <memory>
#include <stack>

#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::tether {

namespace {

class TestObserver final : public TetherHostResponseRecorder::Observer {
 public:
  TestObserver() = default;
  ~TestObserver() = default;

  uint32_t num_callbacks() { return num_callbacks_; }

  // TetherHostResponseRecorder::Observer:
  void OnPreviouslyConnectedHostIdsChanged() override { num_callbacks_++; }

 private:
  uint32_t num_callbacks_ = 0;
};

}  // namespace

class TetherHostResponseRecorderTest : public testing::Test {
 public:
  TetherHostResponseRecorderTest(const TetherHostResponseRecorderTest&) =
      delete;
  TetherHostResponseRecorderTest& operator=(
      const TetherHostResponseRecorderTest&) = delete;

 protected:
  TetherHostResponseRecorderTest() = default;

  void SetUp() override {
    pref_service_ =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    TetherHostResponseRecorder::RegisterPrefs(pref_service_->registry());

    recorder_ =
        std::make_unique<TetherHostResponseRecorder>(pref_service_.get());

    test_observer_ = base::WrapUnique(new TestObserver());
    recorder_->AddObserver(test_observer_.get());
  }

  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable> pref_service_;
  std::unique_ptr<TestObserver> test_observer_;

  std::unique_ptr<TetherHostResponseRecorder> recorder_;
};

TEST_F(TetherHostResponseRecorderTest, TestTetherAvailabilityResponses) {
  uint number_of_responses = 10;
  std::stack<std::string> expected_responses;

  for (uint i = 0; i < number_of_responses; i++) {
    recorder_->RecordSuccessfulTetherAvailabilityResponse(
        base::NumberToString(i));
    expected_responses.push(base::NumberToString(i));
  }

  EXPECT_EQ(expected_responses.size(),
            recorder_->GetPreviouslyAvailableHostIds().size());

  for (uint i = 0; i < number_of_responses; i++) {
    EXPECT_EQ(expected_responses.top(),
              recorder_->GetPreviouslyAvailableHostIds()[i]);
    expected_responses.pop();
  }

  EXPECT_EQ(0u, test_observer_->num_callbacks());
}

TEST_F(TetherHostResponseRecorderTest, TestConnectTetheringResponses) {
  uint number_of_responses = 10;
  std::stack<std::string> expected_responses;

  for (uint i = 0; i < number_of_responses; i++) {
    recorder_->RecordSuccessfulConnectTetheringResponse(
        base::NumberToString(i));
    expected_responses.push(base::NumberToString(i));
  }

  EXPECT_EQ(expected_responses.size(),
            recorder_->GetPreviouslyConnectedHostIds().size());

  for (uint i = 0; i < number_of_responses; i++) {
    EXPECT_EQ(expected_responses.top(),
              recorder_->GetPreviouslyConnectedHostIds()[i]);
    expected_responses.pop();
  }

  EXPECT_EQ(10u, test_observer_->num_callbacks());
}

}  // namespace ash::tether
