// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sqlite_vfs/shared_locks.h"

#include "base/memory/unsafe_shared_memory_region.h"
#include "base/test/gmock_expected_support.h"
#include "components/sqlite_vfs/lock_state.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/sqlite/sqlite3.h"

namespace sqlite_vfs {

namespace {

using ::testing::Ne;

TEST(SharedLocksTest, CreateRegion) {
  auto shared_region = SharedLocks::CreateRegion();
  ASSERT_TRUE(shared_region.IsValid());
  // Size should be at least sizeof(uint32_t) for the atomic lock.
  EXPECT_GE(shared_region.GetSize(), sizeof(uint32_t));
}

TEST(SharedLocksTest, Create) {
  auto shared_region = SharedLocks::CreateRegion();
  ASSERT_TRUE(shared_region.IsValid());
  ASSERT_THAT(SharedLocks::Create(shared_region), Ne(std::nullopt));
}

TEST(SharedLocksTest, LockBasics) {
  auto shared_region = SharedLocks::CreateRegion();
  ASSERT_TRUE(shared_region.IsValid());
  ASSERT_OK_AND_ASSIGN(auto locks, SharedLocks::Create(shared_region));

  int current_mode = SQLITE_LOCK_NONE;

  EXPECT_EQ(locks.Lock(SQLITE_LOCK_SHARED, current_mode), SQLITE_OK);
  EXPECT_EQ(current_mode, SQLITE_LOCK_SHARED);

  EXPECT_EQ(locks.Lock(SQLITE_LOCK_RESERVED, current_mode), SQLITE_OK);
  EXPECT_EQ(current_mode, SQLITE_LOCK_RESERVED);

  EXPECT_EQ(locks.Lock(SQLITE_LOCK_EXCLUSIVE, current_mode), SQLITE_OK);
  EXPECT_EQ(current_mode, SQLITE_LOCK_EXCLUSIVE);

  EXPECT_EQ(locks.Unlock(SQLITE_LOCK_SHARED, current_mode), SQLITE_OK);
  EXPECT_EQ(current_mode, SQLITE_LOCK_SHARED);

  EXPECT_EQ(locks.Unlock(SQLITE_LOCK_NONE, current_mode), SQLITE_OK);
  EXPECT_EQ(current_mode, SQLITE_LOCK_NONE);
}

TEST(SharedLocksTest, MultipleLocks) {
  auto shared_region = SharedLocks::CreateRegion();
  ASSERT_TRUE(shared_region.IsValid());

  ASSERT_OK_AND_ASSIGN(auto locks1, SharedLocks::Create(shared_region));
  ASSERT_OK_AND_ASSIGN(auto locks2, SharedLocks::Create(shared_region));
  ASSERT_OK_AND_ASSIGN(auto locks3, SharedLocks::Create(shared_region));

  int mode1 = SQLITE_LOCK_NONE;
  int mode2 = SQLITE_LOCK_NONE;
  int mode3 = SQLITE_LOCK_NONE;

  // Reader 1 takes SHARED lock.
  EXPECT_EQ(locks1.Lock(SQLITE_LOCK_SHARED, mode1), SQLITE_OK);
  EXPECT_EQ(mode1, SQLITE_LOCK_SHARED);

  // Reader 2 takes SHARED lock.
  EXPECT_EQ(locks2.Lock(SQLITE_LOCK_SHARED, mode2), SQLITE_OK);
  EXPECT_EQ(mode2, SQLITE_LOCK_SHARED);

  // Writer 1 tries to take RESERVED lock, should succeed.
  EXPECT_EQ(locks3.Lock(SQLITE_LOCK_SHARED, mode3), SQLITE_OK);
  EXPECT_EQ(mode3, SQLITE_LOCK_SHARED);

  EXPECT_EQ(locks3.Lock(SQLITE_LOCK_RESERVED, mode3), SQLITE_OK);
  EXPECT_EQ(mode3, SQLITE_LOCK_RESERVED);

  // Writer 2 (we reuse locks2 as another connection) tries to take RESERVED,
  // should fail.
  int mode2_temp = mode2;  // Save current mode (SHARED)
  EXPECT_EQ(locks2.Lock(SQLITE_LOCK_RESERVED, mode2), SQLITE_BUSY);
  EXPECT_EQ(mode2, mode2_temp);  // Mode should not change

  // Writer 1 tries to upgrade to EXCLUSIVE, should fail because of other
  // readers.
  EXPECT_EQ(locks3.Lock(SQLITE_LOCK_EXCLUSIVE, mode3), SQLITE_BUSY);
  EXPECT_EQ(mode3, SQLITE_LOCK_PENDING);  // Should be upgraded to PENDING

  // Reader 1 releases SHARED lock.
  EXPECT_EQ(locks1.Unlock(SQLITE_LOCK_NONE, mode1), SQLITE_OK);
  EXPECT_EQ(mode1, SQLITE_LOCK_NONE);

  // Writer 1 tries to upgrade to EXCLUSIVE again, should still fail because
  // reader 2 is still there.
  EXPECT_EQ(locks3.Lock(SQLITE_LOCK_EXCLUSIVE, mode3), SQLITE_BUSY);
  EXPECT_EQ(mode3, SQLITE_LOCK_PENDING);

  // Reader 2 releases SHARED lock.
  EXPECT_EQ(locks2.Unlock(SQLITE_LOCK_NONE, mode2), SQLITE_OK);
  EXPECT_EQ(mode2, SQLITE_LOCK_NONE);

  // Writer 1 should now be able to upgrade to EXCLUSIVE.
  EXPECT_EQ(locks3.Lock(SQLITE_LOCK_EXCLUSIVE, mode3), SQLITE_OK);
  EXPECT_EQ(mode3, SQLITE_LOCK_EXCLUSIVE);

  // Unlock writer 1.
  EXPECT_EQ(locks3.Unlock(SQLITE_LOCK_NONE, mode3), SQLITE_OK);
  EXPECT_EQ(mode3, SQLITE_LOCK_NONE);
}

TEST(SharedLocksTest, Abandon) {
  auto shared_region = SharedLocks::CreateRegion();
  ASSERT_TRUE(shared_region.IsValid());

  ASSERT_OK_AND_ASSIGN(auto locks1, SharedLocks::Create(shared_region));
  ASSERT_OK_AND_ASSIGN(auto locks2, SharedLocks::Create(shared_region));

  int mode1 = SQLITE_LOCK_NONE;
  int mode2 = SQLITE_LOCK_NONE;

  EXPECT_EQ(locks1.Lock(SQLITE_LOCK_SHARED, mode1), SQLITE_OK);
  EXPECT_EQ(mode1, SQLITE_LOCK_SHARED);

  // Abandon locks1.
  LockState state = locks1.Abandon();
  EXPECT_EQ(state, LockState::kReading);

  // locks2 should fail to acquire lock because it's abandoned.
  EXPECT_EQ(locks2.Lock(SQLITE_LOCK_SHARED, mode2), SQLITE_IOERR_LOCK);
}

TEST(SharedLocksTest, IsReserved) {
  auto shared_region = SharedLocks::CreateRegion();
  ASSERT_TRUE(shared_region.IsValid());

  ASSERT_OK_AND_ASSIGN(auto locks1, SharedLocks::Create(shared_region));
  ASSERT_OK_AND_ASSIGN(auto locks2, SharedLocks::Create(shared_region));

  int mode1 = SQLITE_LOCK_NONE;

  EXPECT_FALSE(locks1.IsReserved());

  EXPECT_EQ(locks1.Lock(SQLITE_LOCK_SHARED, mode1), SQLITE_OK);
  EXPECT_EQ(mode1, SQLITE_LOCK_SHARED);

  EXPECT_EQ(locks1.Lock(SQLITE_LOCK_RESERVED, mode1), SQLITE_OK);
  EXPECT_EQ(mode1, SQLITE_LOCK_RESERVED);

  EXPECT_TRUE(locks2.IsReserved());
}

TEST(SharedLocksTest, AbandonWithPending) {
  auto shared_region = SharedLocks::CreateRegion();
  ASSERT_TRUE(shared_region.IsValid());

  ASSERT_OK_AND_ASSIGN(auto locks1, SharedLocks::Create(shared_region));
  ASSERT_OK_AND_ASSIGN(auto locks2, SharedLocks::Create(shared_region));

  int mode1 = SQLITE_LOCK_NONE;
  int mode2 = SQLITE_LOCK_NONE;

  EXPECT_EQ(locks1.Lock(SQLITE_LOCK_SHARED, mode1), SQLITE_OK);
  EXPECT_EQ(locks2.Lock(SQLITE_LOCK_SHARED, mode2), SQLITE_OK);

  // locks1 tries to upgrade to EXCLUSIVE, fails because locks2 has SHARED.
  // But it should become PENDING.
  EXPECT_EQ(locks1.Lock(SQLITE_LOCK_EXCLUSIVE, mode1), SQLITE_BUSY);
  EXPECT_EQ(mode1, SQLITE_LOCK_PENDING);

  // Abandon locks1. Should return kWriting because it was PENDING.
  LockState state = locks1.Abandon();
  EXPECT_EQ(state, LockState::kWriting);
}

TEST(SharedLocksTest, AbandonWithReserved) {
  auto shared_region = SharedLocks::CreateRegion();
  ASSERT_TRUE(shared_region.IsValid());

  ASSERT_OK_AND_ASSIGN(auto locks1, SharedLocks::Create(shared_region));

  int mode1 = SQLITE_LOCK_NONE;

  EXPECT_EQ(locks1.Lock(SQLITE_LOCK_SHARED, mode1), SQLITE_OK);
  EXPECT_EQ(locks1.Lock(SQLITE_LOCK_RESERVED, mode1), SQLITE_OK);

  // Abandon locks1. Should return kWriting because it was RESERVED.
  LockState state = locks1.Abandon();
  EXPECT_EQ(state, LockState::kWriting);
}

TEST(SharedLocksTest, AbandonWithExclusive) {
  auto shared_region = SharedLocks::CreateRegion();
  ASSERT_TRUE(shared_region.IsValid());

  ASSERT_OK_AND_ASSIGN(auto locks1, SharedLocks::Create(shared_region));

  int mode1 = SQLITE_LOCK_NONE;

  EXPECT_EQ(locks1.Lock(SQLITE_LOCK_SHARED, mode1), SQLITE_OK);
  EXPECT_EQ(locks1.Lock(SQLITE_LOCK_RESERVED, mode1), SQLITE_OK);
  EXPECT_EQ(locks1.Lock(SQLITE_LOCK_EXCLUSIVE, mode1), SQLITE_OK);

  // Abandon locks1. Should return kWriting because it was EXCLUSIVE.
  LockState state = locks1.Abandon();
  EXPECT_EQ(state, LockState::kWriting);
}

TEST(SharedLocksTest, LockReservedAfterAbandon) {
  auto shared_region = SharedLocks::CreateRegion();
  ASSERT_TRUE(shared_region.IsValid());

  ASSERT_OK_AND_ASSIGN(auto locks1, SharedLocks::Create(shared_region));
  ASSERT_OK_AND_ASSIGN(auto locks2, SharedLocks::Create(shared_region));

  int mode1 = SQLITE_LOCK_NONE;
  int mode2 = SQLITE_LOCK_NONE;

  EXPECT_EQ(locks1.Lock(SQLITE_LOCK_SHARED, mode1), SQLITE_OK);
  EXPECT_EQ(locks2.Lock(SQLITE_LOCK_SHARED, mode2), SQLITE_OK);

  locks1.Abandon();

  // locks2 has SHARED lock, tries to acquire RESERVED.
  // Should fail with IOERR_LOCK because locks1 abandoned.
  EXPECT_EQ(locks2.Lock(SQLITE_LOCK_RESERVED, mode2), SQLITE_IOERR_LOCK);
}

TEST(SharedLocksTest, LockExclusiveAfterAbandon) {
  auto shared_region = SharedLocks::CreateRegion();
  ASSERT_TRUE(shared_region.IsValid());

  ASSERT_OK_AND_ASSIGN(auto locks1, SharedLocks::Create(shared_region));
  ASSERT_OK_AND_ASSIGN(auto locks2, SharedLocks::Create(shared_region));

  int mode1 = SQLITE_LOCK_NONE;
  int mode2 = SQLITE_LOCK_NONE;

  EXPECT_EQ(locks1.Lock(SQLITE_LOCK_SHARED, mode1), SQLITE_OK);
  EXPECT_EQ(locks2.Lock(SQLITE_LOCK_SHARED, mode2), SQLITE_OK);

  locks1.Abandon();

  // locks2 has SHARED lock, tries to acquire EXCLUSIVE.
  // Should fail with IOERR_LOCK because locks1 abandoned.
  EXPECT_EQ(locks2.Lock(SQLITE_LOCK_EXCLUSIVE, mode2), SQLITE_IOERR_LOCK);
}

}  // namespace

}  // namespace sqlite_vfs
