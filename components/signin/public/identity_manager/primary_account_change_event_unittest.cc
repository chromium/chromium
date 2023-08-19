// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/identity_manager/primary_account_change_event.h"

#include <sstream>

#include "components/signin/public/base/consent_level.h"
#include "google_apis/gaia/core_account_id.h"
#include "testing/gtest/include/gtest/gtest.h"

using signin::ConsentLevel;
using signin::PrimaryAccountChangeEvent;
using Type = signin::PrimaryAccountChangeEvent::Type;
using State = signin::PrimaryAccountChangeEvent::State;

// TODO(crbug.com/1462978): Revise this test suite when ConsentLevel::kSync is
//     deleted. See ConsentLevel::kSync documentation for details.
class PrimaryAccountChangeEventTest : public testing::Test {
 public:
  PrimaryAccountChangeEventTest() {
    CoreAccountInfo account_info1 = GetCoreAccountInfoFrom("account1");
    CoreAccountInfo account_info2 = GetCoreAccountInfoFrom("account2");

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
  CoreAccountInfo GetCoreAccountInfoFrom(const char* gaia_id) {
    CoreAccountInfo account_info;
    account_info.account_id = CoreAccountId::FromGaiaId(gaia_id);
    account_info.gaia = gaia_id;
    account_info.email = std::string(gaia_id) + "@gmail.com";
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

// TODO(crbug.com/1462978): Delete this test when ConsentLevel::kSync is
//     deleted. See ConsentLevel::kSync documentation for details.
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

// TODO(crbug.com/1462978): Delete this test when ConsentLevel::kSync is
//     deleted. See ConsentLevel::kSync documentation for details.
TEST_F(PrimaryAccountChangeEventTest, ConsentLevelChangeFromSyncToNotRequired) {
  PrimaryAccountChangeEvent event(account1_sync_, account1_not_required_);
  EXPECT_EQ(Type::kNone, event.GetEventTypeFor(ConsentLevel::kSignin));
  EXPECT_EQ(Type::kCleared, event.GetEventTypeFor(ConsentLevel::kSync));

  event = PrimaryAccountChangeEvent(account1_sync_, empty_not_required_);
  EXPECT_EQ(Type::kCleared, event.GetEventTypeFor(ConsentLevel::kSignin));
  EXPECT_EQ(Type::kCleared, event.GetEventTypeFor(ConsentLevel::kSync));
}

TEST_F(PrimaryAccountChangeEventTest, ToStringSupported) {
  PrimaryAccountChangeEvent event(empty_not_required_, account1_not_required_);

  std::stringstream sstream;
  sstream << event;

  EXPECT_EQ(
      sstream.str(),
      "{ previous_state: { primary_account: , consent_level: }, "
      "current_state: { primary_account: account1, consent_level: Signin } }");
}
