// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/core/session_id_generator.h"

#include <limits>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sessions {
namespace {

using testing::Return;

const int kExpectedIdPadding = 50;

class SessionIdGeneratorTest : public testing::Test {
 public:
  SessionIdGeneratorTest(const SessionIdGeneratorTest&) = delete;
  SessionIdGeneratorTest& operator=(const SessionIdGeneratorTest&) = delete;

 protected:
  SessionIdGeneratorTest() {
    // Call Shutdown() in case other tests outside this file have forgotten to
    // do so, which would leak undesired state across tests.
    SessionIdGenerator::GetInstance()->Shutdown();
    SessionIdGenerator::RegisterPrefs(prefs_.registry());
  }

  ~SessionIdGeneratorTest() override {
    SessionIdGenerator::GetInstance()->Shutdown();
  }

  void WriteLastValueToPrefs(int64_t value) {
    prefs_.SetInt64(SessionIdGenerator::GetLastValuePrefNameForTest(), value);
  }

  int64_t ReadLastValueFromPrefs() {
    return prefs_.GetInt64(SessionIdGenerator::GetLastValuePrefNameForTest());
  }

  TestingPrefServiceSimple prefs_;
};

TEST_F(SessionIdGeneratorTest, ShouldGenerateContiguousIds) {
  SessionIdGenerator* generator = SessionIdGenerator::GetInstance();
  generator->Init(&prefs_);
  SessionID session_id1 = generator->NewUnique();
  SessionID session_id2 = generator->NewUnique();
  SessionID session_id3 = generator->NewUnique();
  EXPECT_EQ(session_id2.id(), session_id1.id() + 1);
  EXPECT_EQ(session_id3.id(), session_id2.id() + 1);
  EXPECT_EQ(session_id3.id(), ReadLastValueFromPrefs());
}

TEST_F(SessionIdGeneratorTest, ShouldInitializeWithRandomValue) {
  base::MockCallback<SessionIdGenerator::RandomGenerator> random_generator;
  SessionIdGenerator* generator = SessionIdGenerator::GetInstance();
  generator->SetRandomGeneratorForTest(random_generator.Get());

  EXPECT_CALL(random_generator, Run()).WillOnce(Return(123));
  generator->Init(&prefs_);
  EXPECT_EQ(123 + 1 + kExpectedIdPadding, generator->NewUnique().id());

  // Mimic a browser restart with cleared prefs.
  generator->Shutdown();
  WriteLastValueToPrefs(-1);
  EXPECT_CALL(random_generator, Run()).WillOnce(Return(19));
  generator->Init(&prefs_);

  EXPECT_EQ(19 + 1 + kExpectedIdPadding, generator->NewUnique().id());
}

TEST_F(SessionIdGeneratorTest, ShouldCornerCasesInRandomFunc) {
  base::MockCallback<SessionIdGenerator::RandomGenerator> random_generator;
  SessionIdGenerator* generator = SessionIdGenerator::GetInstance();
  generator->SetRandomGeneratorForTest(random_generator.Get());

  // Exercise smallest value in range.
  EXPECT_CALL(random_generator, Run()).WillOnce(Return(0));
  generator->Init(&prefs_);
  EXPECT_EQ(0 + 1 + kExpectedIdPadding, generator->NewUnique().id());

  // Exercise maximum value in range.
  generator->Shutdown();
  WriteLastValueToPrefs(-1);
  EXPECT_CALL(random_generator, Run())
      .WillOnce(Return(std::numeric_limits<SessionID::id_type>::max()));
  generator->Init(&prefs_);
  EXPECT_EQ(1 + kExpectedIdPadding, generator->NewUnique().id());

  // Exercise maximum value minus one in range.
  generator->Shutdown();
  WriteLastValueToPrefs(-1);
  EXPECT_CALL(random_generator, Run())
      .WillOnce(Return(std::numeric_limits<SessionID::id_type>::max() - 1));
  generator->Init(&prefs_);
  EXPECT_EQ(1 + kExpectedIdPadding, generator->NewUnique().id());

  // Exercise a random value which is exactly |kExpectedIdPadding| less than
  // the maximum value in the range.
  generator->Shutdown();
  WriteLastValueToPrefs(-1);
  EXPECT_CALL(random_generator, Run())
      .WillOnce(Return(std::numeric_limits<SessionID::id_type>::max() -
                       kExpectedIdPadding));
  generator->Init(&prefs_);
  EXPECT_EQ(1, generator->NewUnique().id());

  // Exercise a random value which is |kExpectedIdPadding-1| less than the
  // maximum value in the range (no overflow expected).
  generator->Shutdown();
  WriteLastValueToPrefs(-1);
  EXPECT_CALL(random_generator, Run())
      .WillOnce(Return(std::numeric_limits<SessionID::id_type>::max() -
                       kExpectedIdPadding - 1));
  generator->Init(&prefs_);
  EXPECT_EQ(std::numeric_limits<SessionID::id_type>::max(),
            generator->NewUnique().id());
}

TEST_F(SessionIdGeneratorTest, ShouldRestoreAndPadLastValueFromPrefs) {
  WriteLastValueToPrefs(7);
  SessionIdGenerator* generator = SessionIdGenerator::GetInstance();
  generator->Init(&prefs_);
  SessionID session_id = generator->NewUnique();
  EXPECT_EQ(8 + kExpectedIdPadding, session_id.id());
  EXPECT_EQ(8 + kExpectedIdPadding, ReadLastValueFromPrefs());
}

TEST_F(SessionIdGeneratorTest, ShouldHandleOverflowDuringGeneration) {
  WriteLastValueToPrefs(std::numeric_limits<SessionID::id_type>::max() -
                        kExpectedIdPadding - 3);
  SessionIdGenerator* generator = SessionIdGenerator::GetInstance();
  generator->Init(&prefs_);
  ASSERT_EQ(std::numeric_limits<SessionID::id_type>::max() - 2,
            generator->NewUnique().id());
  EXPECT_EQ(std::numeric_limits<SessionID::id_type>::max() - 1,
            generator->NewUnique().id());
  EXPECT_EQ(std::numeric_limits<SessionID::id_type>::max(),
            generator->NewUnique().id());
  EXPECT_EQ(1, generator->NewUnique().id());
}

TEST_F(SessionIdGeneratorTest, ShouldHandleOverflowDuringPadding) {
  WriteLastValueToPrefs(std::numeric_limits<SessionID::id_type>::max() - 2);
  SessionIdGenerator* generator = SessionIdGenerator::GetInstance();
  generator->Init(&prefs_);
  EXPECT_EQ(kExpectedIdPadding + 1, generator->NewUnique().id());
}

TEST_F(SessionIdGeneratorTest, HighestRestoredID) {
  std::string histogram("Session.ID.RestoredDifference");
  base::HistogramTester tester;

  base::MockCallback<SessionIdGenerator::RandomGenerator> random_generator;
  SessionIdGenerator* generator = SessionIdGenerator::GetInstance();
  generator->SetRandomGeneratorForTest(random_generator.Get());

  // Highest restored ID is lower than the next value.
  EXPECT_CALL(random_generator, Run()).WillOnce(Return(123));
  generator->Init(&prefs_);

  SessionID highest = SessionID::FromSerializedValue(123 + kExpectedIdPadding);
  generator->SetHighestRestoredID(highest);

  EXPECT_EQ(123 + 1 + kExpectedIdPadding, generator->NewUnique().id());
  tester.ExpectBucketCount(histogram, 0, 1);
  tester.ExpectTotalCount(histogram, 1);

  // Highest restored ID is higher than the next value.
  generator->Shutdown();
  WriteLastValueToPrefs(-1);
  EXPECT_CALL(random_generator, Run()).WillOnce(Return(123));
  generator->Init(&prefs_);

  highest = SessionID::FromSerializedValue(200);
  generator->SetHighestRestoredID(highest);

  EXPECT_EQ(201, generator->NewUnique().id());
  tester.ExpectBucketCount(histogram, 27, 1);
  tester.ExpectTotalCount(histogram, 2);

  // Highest restored ID is higher than the next value because it overflown.
  generator->Shutdown();
  WriteLastValueToPrefs(-1);
  EXPECT_CALL(random_generator, Run()).WillOnce(Return(123));
  generator->Init(&prefs_);

  SessionID::id_type very_high_restore =
      std::numeric_limits<SessionID::id_type>::max() / 2 + 123 +
      kExpectedIdPadding + 2;
  highest = SessionID::FromSerializedValue(very_high_restore);
  generator->SetHighestRestoredID(highest);

  EXPECT_EQ(123 + 1 + kExpectedIdPadding, generator->NewUnique().id());
  tester.ExpectBucketCount(histogram, 0, 2);
  tester.ExpectTotalCount(histogram, 3);
}

// Verifies correctness of the test-only codepath.
TEST(SessionIdGeneratorWithoutInitTest, ShouldStartFromOneAndIncrement) {
  SessionIdGenerator* generator = SessionIdGenerator::GetInstance();
  EXPECT_EQ(1, generator->NewUnique().id());
  EXPECT_EQ(2, generator->NewUnique().id());
  generator->Shutdown();
}

}  // namespace
}  // namespace sessions
