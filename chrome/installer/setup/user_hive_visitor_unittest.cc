// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/setup/user_hive_visitor.h"

#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/win/registry.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace installer {

namespace {

class UserHiveVisitor {
 public:
  UserHiveVisitor() = default;

  UserHiveVisitor(const UserHiveVisitor&) = delete;
  UserHiveVisitor& operator=(const UserHiveVisitor&) = delete;

  bool OnUserHive(const wchar_t* sid, base::win::RegKey* key) {
    EXPECT_NE(nullptr, sid);
    EXPECT_STRNE(L"", sid);
    EXPECT_NE(nullptr, key);
    EXPECT_TRUE(key->Valid());
    sids_visited_.push_back(sid);
    return !early_exit_;
  }

  void set_early_exit(bool early_exit) { early_exit_ = early_exit; }

  const std::vector<std::wstring> sids_visited() const { return sids_visited_; }

 private:
  std::vector<std::wstring> sids_visited_;
  bool early_exit_ = false;
};

}  // namespace

// Tests that the visitor visits at least one user hive. This will succeed even
// when run as an ordinary user, as the current user's hive is always available.
TEST(UserHiveVisitorTest, VisitAllUserHives) {
  UserHiveVisitor visitor;

  VisitUserHives(base::BindRepeating(&UserHiveVisitor::OnUserHive,
                                     base::Unretained(&visitor)));

  EXPECT_GT(visitor.sids_visited().size(), 0U);
}

// Tests that only one user hive is visited when the visitor returns false to
// stop the iteration.
TEST(UserHiveVisitor, VisitOneHive) {
  UserHiveVisitor visitor;

  visitor.set_early_exit(true);
  VisitUserHives(base::BindRepeating(&UserHiveVisitor::OnUserHive,
                                     base::Unretained(&visitor)));

  EXPECT_EQ(1U, visitor.sids_visited().size());
}

}  // namespace installer
