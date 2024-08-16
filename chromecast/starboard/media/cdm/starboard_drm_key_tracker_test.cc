// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/starboard/media/cdm/starboard_drm_key_tracker.h"

#include "base/test/bind.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace media {
namespace {

using ::testing::MockFunction;

constexpr char kSessionId1[] = "session1";
constexpr char kSessionId2[] = "session2";

// A test fixture is used to ensure that the StarboardDrmKeyTracker's state is
// cleared before each test.
class StarboardDrmKeyTrackerTest : public ::testing::Test {
 protected:
  StarboardDrmKeyTrackerTest() {
    StarboardDrmKeyTracker::GetInstance().ClearStateForTesting();
  }

  ~StarboardDrmKeyTrackerTest() override = default;
};

TEST_F(StarboardDrmKeyTrackerTest, HasKeyFindsAddedKey) {
  constexpr char kKey[] = "key";

  StarboardDrmKeyTracker& cdm_manager = StarboardDrmKeyTracker::GetInstance();

  cdm_manager.AddKey(kKey, kSessionId1);
  EXPECT_TRUE(cdm_manager.HasKey(kKey));
}

TEST_F(StarboardDrmKeyTrackerTest, HasKeyDoesNotFindKeyThatWasNotAdded) {
  constexpr char kKey[] = "key";

  StarboardDrmKeyTracker& cdm_manager = StarboardDrmKeyTracker::GetInstance();

  EXPECT_FALSE(cdm_manager.HasKey(kKey));
}

TEST_F(StarboardDrmKeyTrackerTest, HasKeyDoesNotFindKeyThatWasRemoved) {
  constexpr char kKey[] = "key";

  StarboardDrmKeyTracker& cdm_manager = StarboardDrmKeyTracker::GetInstance();

  cdm_manager.AddKey(kKey, kSessionId1);
  EXPECT_TRUE(cdm_manager.HasKey(kKey));
  cdm_manager.RemoveKey(kKey, kSessionId1);
  EXPECT_FALSE(cdm_manager.HasKey(kKey));
}

TEST_F(StarboardDrmKeyTrackerTest, OnlyRemovesKeyForRelevantSession) {
  constexpr char kKey[] = "key";

  StarboardDrmKeyTracker& cdm_manager = StarboardDrmKeyTracker::GetInstance();

  cdm_manager.AddKey(kKey, kSessionId1);
  cdm_manager.AddKey(kKey, kSessionId2);

  EXPECT_TRUE(cdm_manager.HasKey(kKey));
  cdm_manager.RemoveKey(kKey, kSessionId1);
  // The key should still be present in kSessionId2.
  EXPECT_TRUE(cdm_manager.HasKey(kKey));
}

TEST_F(StarboardDrmKeyTrackerTest, RemoveKeysForSessionRemovesAllRelevantKeys) {
  constexpr char kKey1[] = "key1";
  constexpr char kKey2[] = "key2";

  StarboardDrmKeyTracker& cdm_manager = StarboardDrmKeyTracker::GetInstance();

  cdm_manager.AddKey(kKey1, kSessionId1);
  cdm_manager.AddKey(kKey2, kSessionId1);

  cdm_manager.AddKey(kKey1, kSessionId2);

  EXPECT_TRUE(cdm_manager.HasKey(kKey1));
  EXPECT_TRUE(cdm_manager.HasKey(kKey2));

  cdm_manager.RemoveKeysForSession(kSessionId1);
  // The key should still be present in kSessionId2.
  EXPECT_TRUE(cdm_manager.HasKey(kKey1));
  EXPECT_FALSE(cdm_manager.HasKey(kKey2));
}

TEST_F(StarboardDrmKeyTrackerTest, RunsCallbacksWhenKeyIsAvailable) {
  constexpr char kKey[] = "key";

  StarboardDrmKeyTracker& cdm_manager = StarboardDrmKeyTracker::GetInstance();

  MockFunction<void(int64_t)> cb1;
  MockFunction<void(int64_t)> cb2;

  EXPECT_CALL(cb1, Call).Times(1);
  EXPECT_CALL(cb2, Call).Times(1);

  const int64_t ticket1 = cdm_manager.WaitForKey(
      kKey, base::BindLambdaForTesting(cb1.AsStdFunction()));
  const int64_t ticket2 = cdm_manager.WaitForKey(
      kKey, base::BindLambdaForTesting(cb2.AsStdFunction()));

  EXPECT_NE(ticket1, ticket2);

  cdm_manager.AddKey(kKey, kSessionId1);
}

TEST_F(StarboardDrmKeyTrackerTest,
       RunsCallbackImmediatelyIfKeyIsAlreadyAvailable) {
  constexpr char kKey[] = "key";

  StarboardDrmKeyTracker& cdm_manager = StarboardDrmKeyTracker::GetInstance();
  cdm_manager.AddKey(kKey, kSessionId1);

  MockFunction<void(int64_t)> cb;
  EXPECT_CALL(cb, Call).Times(1);

  cdm_manager.WaitForKey(kKey, base::BindLambdaForTesting(cb.AsStdFunction()));
}

TEST_F(StarboardDrmKeyTrackerTest, DoesNotRunUnregisteredCallback) {
  constexpr char kKey[] = "key";

  StarboardDrmKeyTracker& cdm_manager = StarboardDrmKeyTracker::GetInstance();

  MockFunction<void(int64_t)> cb1;
  MockFunction<void(int64_t)> cb2;

  // cb1 will be unregistered, so it should not be run.
  EXPECT_CALL(cb1, Call).Times(0);
  EXPECT_CALL(cb2, Call).Times(1);

  const int64_t ticket1 = cdm_manager.WaitForKey(
      kKey, base::BindLambdaForTesting(cb1.AsStdFunction()));
  const int64_t ticket2 = cdm_manager.WaitForKey(
      kKey, base::BindLambdaForTesting(cb2.AsStdFunction()));

  EXPECT_NE(ticket1, ticket2);
  cdm_manager.UnregisterCallback(ticket1);
  cdm_manager.AddKey(kKey, kSessionId1);
}

}  // namespace
}  // namespace media
}  // namespace chromecast
