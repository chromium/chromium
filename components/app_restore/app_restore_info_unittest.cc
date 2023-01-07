// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/app_restore/app_restore_info.h"

#include <map>
#include <set>

#include "components/account_id/account_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_restore {

class FakeAppRestoreInfoObserver : public AppRestoreInfo::Observer {
 public:
  void OnRestorePrefChanged(const AccountId& account_id,
                            bool could_restore) override {
    if (could_restore)
      restore_prefs_.insert(account_id);
    else
      restore_prefs_.erase(account_id);

    restore_pref_changed_count_[account_id]++;
  }

  bool CanPerformRestore(const AccountId& account_id) const {
    return restore_prefs_.find(account_id) != restore_prefs_.end();
  }

  int RestorePrefChangedCount(const AccountId& account_id) const {
    auto it = restore_pref_changed_count_.find(account_id);
    return it != restore_pref_changed_count_.end() ? it->second : 0;
  }

 private:
  std::set<AccountId> restore_prefs_;
  std::map<AccountId, int> restore_pref_changed_count_;
};

using AppRestoreInfoTest = testing::Test;

// Test the OnRestorePrefChanged callback when the restore pref is reset.
TEST_F(AppRestoreInfoTest, RestorePref) {
  AccountId account_id1 = AccountId::FromUserEmail("aaa@gmail.com");
  AccountId account_id2 = AccountId::FromUserEmail("bbb@gmail.com");

  AppRestoreInfo::GetInstance()->SetRestorePref(account_id1, true);

  FakeAppRestoreInfoObserver observer;
  AppRestoreInfo::GetInstance()->AddObserver(&observer);

  // Not change the restore flag.
  AppRestoreInfo::GetInstance()->SetRestorePref(account_id1, true);
  AppRestoreInfo::GetInstance()->SetRestorePref(account_id2, false);
  EXPECT_EQ(0, observer.RestorePrefChangedCount(account_id1));
  EXPECT_EQ(0, observer.RestorePrefChangedCount(account_id2));
  EXPECT_TRUE(AppRestoreInfo::GetInstance()->CanPerformRestore(account_id1));
  EXPECT_FALSE(AppRestoreInfo::GetInstance()->CanPerformRestore(account_id2));

  // Change the restore flag.
  AppRestoreInfo::GetInstance()->SetRestorePref(account_id1, false);
  AppRestoreInfo::GetInstance()->SetRestorePref(account_id2, true);
  EXPECT_EQ(1, observer.RestorePrefChangedCount(account_id1));
  EXPECT_EQ(1, observer.RestorePrefChangedCount(account_id2));
  EXPECT_FALSE(observer.CanPerformRestore(account_id1));
  EXPECT_TRUE(observer.CanPerformRestore(account_id2));
  EXPECT_FALSE(AppRestoreInfo::GetInstance()->CanPerformRestore(account_id1));
  EXPECT_TRUE(AppRestoreInfo::GetInstance()->CanPerformRestore(account_id2));

  AppRestoreInfo::GetInstance()->RemoveObserver(&observer);
}

}  // namespace app_restore
