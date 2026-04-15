// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/at_memory_promo_tracker.h"

#include "base/test/task_environment.h"
#include "components/sessions/core/session_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

class AtMemoryPromoTrackerTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  AtMemoryPromoTracker tracker_;
  SessionID tab1_ = SessionID::FromSerializedValue(1);
  SessionID tab2_ = SessionID::FromSerializedValue(2);
};

TEST_F(AtMemoryPromoTrackerTest, ValidSequenceTriggers) {
  tracker_.OnCopy(tab1_);
  EXPECT_TRUE(tracker_.OnPaste(tab2_));
}

TEST_F(AtMemoryPromoTrackerTest, SameTabDoesNotTrigger) {
  tracker_.OnCopy(tab1_);
  EXPECT_FALSE(tracker_.OnPaste(tab1_));
}

TEST_F(AtMemoryPromoTrackerTest, SequenceTooSlow) {
  tracker_.OnCopy(tab1_);
  task_environment_.FastForwardBy(base::Seconds(61));
  EXPECT_FALSE(tracker_.OnPaste(tab2_));
}

TEST_F(AtMemoryPromoTrackerTest, NewCopyResetsTimer) {
  tracker_.OnCopy(tab1_);
  task_environment_.FastForwardBy(base::Seconds(61));
  tracker_.OnCopy(tab1_);
  EXPECT_TRUE(tracker_.OnPaste(tab2_));
}

TEST_F(AtMemoryPromoTrackerTest, PasteWithoutCopyFails) {
  EXPECT_FALSE(tracker_.OnPaste(tab2_));
}

}  // namespace autofill
