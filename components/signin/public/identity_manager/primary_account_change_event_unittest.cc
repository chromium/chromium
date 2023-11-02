// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/identity_manager/primary_account_change_event.h"
#include "components/signin/public/base/consent_level.h"
#include "google_apis/gaia/core_account_id.h"
#include "testing/gtest/include/gtest/gtest.h"

using signin::ConsentLevel;
using signin::PrimaryAccountChangeEvent;
using Type = signin::PrimaryAccountChangeEvent::Type;
using State = signin::PrimaryAccountChangeEvent::State;

class PrimaryAccountChangeEventTest : public testing::Test {
 public:
  PrimaryAccountChangeEventTest() {
    CoreAccountInfo account_info1 = GetCoreAccountInfoFrom("account1@test.com");
    CoreAccountInfo account_info2 = GetCoreAccountInfoFrom("account2@test.com");

    empty_not_required_ = State(CoreAccountInfo(), ConsentLevel::kSignin);
    account1_not_required_ = State(account_info1, ConsentLevel::kSignin);
    account2_not_required_ = State(account_info2, ConsentLevel::kSignin);
    account1_sync_ = State(account_info1, ConsentLevel::kSync);
    account2_sync_ = State(account_info2, ConsentLevel::kSync);
  }

  State empty_not_required_;
  State account1_not_required_;
  State account2_not_required_;
  State account1_sync_;
  State account2_sync_;

 private:
  CoreAccountInfo GetCoreAccountInfoFrom(const char* account_name) {
    CoreAccountInfo account_info;
    account_info.account_id = CoreAccountId(account_name);
    account_info.gaia = account_info.email = account_name;

    return account_info;
  }
};

TEST_F(PrimaryAccountChangeEventTest, NoStateChange) {
  PrimaryAccountChangeEvent event(empty_not_required_, empty_not_required_);
  EXPECT_EQ(Type::kNone, event.GetEventTypeFor(ConsentLevel::kSignin));
  EXPECT_EQ(Type::kNone, event.GetEventTypeFor(ConsentLevel::kSync));

  event =
      PrimaryAccountChangeEvent(account1_not_required_, account1_not_required_);
  EXPECT_EQ(Type::kNone, event.GetEventTypeFor(ConsentLevel::kSignin));
  EXPECT_EQ(Type::kNone, event.GetEventTypeFor(ConsentLevel::kSync));

  event = PrimaryAccountChangeEvent(account1_sync_, account1_sync_);
  EXPECT_EQ(Type::kNone, event.GetEventTypeFor(ConsentLevel::kSignin));
  EXPECT_EQ(Type::kNone, event.GetEventTypeFor(ConsentLevel::kSync));
}

TEST_F(PrimaryAccountChangeEventTest,
       ConsentLevelChangeFromNotRequiredToNotRequired) {
  PrimaryAccountChangeEvent event(empty_not_required_, account1_not_required_);
  EXPECT_EQ(Type::kSet, event.GetEventTypeFor(ConsentLevel::kSignin));
  EXPECT_EQ(Type::kNone, event.GetEventTypeFor(ConsentLevel::kSync));

  event =
      PrimaryAccountChangeEvent(account1_not_required_, account2_not_required_);
  EXPECT_EQ(Type::kSet, event.GetEventTypeFor(ConsentLevel::kSignin));
  EXPECT_EQ(Type::kNone, event.GetEventTypeFor(ConsentLevel::kSync));

  event =
      PrimaryAccountChangeEvent(account1_not_required_, empty_not_required_);
  EXPECT_EQ(Type::kCleared, event.GetEventTypeFor(ConsentLevel::kSignin));
  EXPECT_EQ(Type::kNone, event.GetEventTypeFor(ConsentLevel::kSync));
}

TEST_F(PrimaryAccountChangeEventTest, ConsentLevelChangeFromNotRequiredToSync) {
  PrimaryAccountChangeEvent event(empty_not_required_, account1_sync_);
  EXPECT_EQ(Type::kSet, event.GetEventTypeFor(ConsentLevel::kSignin));
  EXPECT_EQ(Type::kSet, event.GetEventTypeFor(ConsentLevel::kSync));

  event = PrimaryAccountChangeEvent(account1_not_required_, account1_sync_);
  EXPECT_EQ(Type::kNone, event.GetEventTypeFor(ConsentLevel::kSignin));
  EXPECT_EQ(Type::kSet, event.GetEventTypeFor(ConsentLevel::kSync));

  event = PrimaryAccountChangeEvent(account1_not_required_, account2_sync_);
  EXPECT_EQ(Type::kSet, event.GetEventTypeFor(ConsentLevel::kSignin));
  EXPECT_EQ(Type::kSet, event.GetEventTypeFor(ConsentLevel::kSync));
}

TEST_F(PrimaryAccountChangeEventTest, ConsentLevelChangeFromSyncToNotRequired) {
  PrimaryAccountChangeEvent event(account1_sync_, account1_not_required_);
  EXPECT_EQ(Type::kNone, event.GetEventTypeFor(ConsentLevel::kSignin));
  EXPECT_EQ(Type::kCleared, event.GetEventTypeFor(ConsentLevel::kSync));

  event = PrimaryAccountChangeEvent(account1_sync_, empty_not_required_);
  EXPECT_EQ(Type::kCleared, event.GetEventTypeFor(ConsentLevel::kSignin));
  EXPECT_EQ(Type::kCleared, event.GetEventTypeFor(ConsentLevel::kSync));
}
