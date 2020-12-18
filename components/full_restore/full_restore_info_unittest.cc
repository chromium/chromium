// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/full_restore/full_restore_info.h"

#include <map>
#include <set>

#include "components/account_id/account_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace full_restore {

class FakeFullRestoreInfoObserver : public FullRestoreInfo::Observer {
 public:
  void OnRestoreFlagChanged(const AccountId& account_id,
                            bool should_restore) override {
    if (should_restore)
      restore_flags_.insert(account_id);
    else
      restore_flags_.erase(account_id);

    invoked_count_[account_id]++;
  }

  bool ShouldRestore(const AccountId& account_id) {
    return restore_flags_.find(account_id) != restore_flags_.end();
  }

  int InvokedCount(const AccountId& account_id) {
    auto it = invoked_count_.find(account_id);
    return it != invoked_count_.end() ? it->second : 0;
  }

 private:
  std::set<AccountId> restore_flags_;
  std::map<AccountId, int> invoked_count_;
};

using FullRestoreInfoTest = testing::Test;

// Test the RestoreFlagChanged callback when the restore flag is reset.
TEST_F(FullRestoreInfoTest, RestoreFlag) {
  AccountId account_id1 = AccountId::FromUserEmail("aaa@gmail.com");
  AccountId account_id2 = AccountId::FromUserEmail("bbb@gmail.com");

  FullRestoreInfo::GetInstance()->SetRestoreFlag(account_id1, true);

  FakeFullRestoreInfoObserver observer;
  FullRestoreInfo::GetInstance()->AddObserver(&observer);

  // Not change the restore flag
  FullRestoreInfo::GetInstance()->SetRestoreFlag(account_id1, true);
  FullRestoreInfo::GetInstance()->SetRestoreFlag(account_id2, false);
  EXPECT_EQ(0, observer.InvokedCount(account_id1));
  EXPECT_EQ(0, observer.InvokedCount(account_id2));
  EXPECT_TRUE(FullRestoreInfo::GetInstance()->ShouldRestore(account_id1));
  EXPECT_FALSE(FullRestoreInfo::GetInstance()->ShouldRestore(account_id2));

  // Change the restore flag
  FullRestoreInfo::GetInstance()->SetRestoreFlag(account_id1, false);
  FullRestoreInfo::GetInstance()->SetRestoreFlag(account_id2, true);
  EXPECT_EQ(1, observer.InvokedCount(account_id1));
  EXPECT_EQ(1, observer.InvokedCount(account_id2));
  EXPECT_FALSE(observer.ShouldRestore(account_id1));
  EXPECT_TRUE(observer.ShouldRestore(account_id2));
  EXPECT_FALSE(FullRestoreInfo::GetInstance()->ShouldRestore(account_id1));
  EXPECT_TRUE(FullRestoreInfo::GetInstance()->ShouldRestore(account_id2));
}

}  // namespace full_restore
