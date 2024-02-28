// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/identity_manager/primary_account_change_event.h"

#include <sstream>

#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_metrics.h"
#include "google_apis/gaia/core_account_id.h"
#include "testing/gtest/include/gtest/gtest.h"

using signin::ConsentLevel;
using signin::PrimaryAccountChangeEvent;
using Type = signin::PrimaryAccountChangeEvent::Type;
using State = signin::PrimaryAccountChangeEvent::State;

// TODO(crbug.com/40067058): Revise this test suite when ConsentLevel::kSync is
//     deleted. See ConsentLevel::kSync documentation for details.
class PrimaryAccountChangeEventTest : public testing::Test {
 public:
  PrimaryAccountChangeEventTest() {
    CoreAccountInfo account_info1 = GetCoreAccountInfoFrom("account1");
    CoreAccountInfo account_info2 = GetCoreAccountInfoFrom("account2");

    empty_sync_ = State(account_info2, signin::ConsentLevel::kSync);
    empty_signin_ = State(CoreAccountInfo(), ConsentLevel::kSignin);
    account1_signin_ = State(account_info1, ConsentLevel::kSignin);
    account2_signin_ = State(account_info2, ConsentLevel::kSignin);
    account1_sync_ = State(account_info1, ConsentLevel::kSync);
    account2_sync_ = State(account_info2, ConsentLevel::kSync);
    access_point_ = signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN;
    profile_signout_ = signin_metrics::ProfileSignout::kTest;
  }

  State empty_sync_;
  State empty_signin_;
  State account1_signin_;
  State account2_signin_;
  State account1_sync_;
  State account2_sync_;
  signin_metrics::AccessPoint access_point_;
  signin_metrics::ProfileSignout profile_signout_;

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
  PrimaryAccountChangeEvent event(empty_signin_, empty_signin_, access_point_);
  EXPECT_EQ(Type::kNone, event.GetEventTypeFor(ConsentLevel::kSignin));
  EXPECT_EQ(Type::kNone, event.GetEventTypeFor(ConsentLevel::kSync));

  event = PrimaryAccountChangeEvent(account1_signin_, account1_signin_,
                                    access_point_);
  EXPECT_EQ(Type::kNone, event.GetEventTypeFor(ConsentLevel::kSignin));
  EXPECT_EQ(Type::kNone, event.GetEventTypeFor(ConsentLevel::kSync));

  event =
      PrimaryAccountChangeEvent(account1_sync_, account1_sync_, access_point_);
  EXPECT_EQ(Type::kNone, event.GetEventTypeFor(ConsentLevel::kSignin));
  EXPECT_EQ(Type::kNone, event.GetEventTypeFor(ConsentLevel::kSync));
}

TEST_F(PrimaryAccountChangeEventTest,
       ConsentLevelChangeFromNotRequiredToNotRequired) {
  PrimaryAccountChangeEvent event(empty_signin_, account1_signin_,
                                  access_point_);
  EXPECT_EQ(Type::kSet, event.GetEventTypeFor(ConsentLevel::kSignin));
  EXPECT_EQ(Type::kNone, event.GetEventTypeFor(ConsentLevel::kSync));

  event = PrimaryAccountChangeEvent(account1_signin_, account2_signin_,
                                    access_point_);
  EXPECT_EQ(Type::kSet, event.GetEventTypeFor(ConsentLevel::kSignin));
  EXPECT_EQ(Type::kNone, event.GetEventTypeFor(ConsentLevel::kSync));

  event = PrimaryAccountChangeEvent(account1_signin_, empty_signin_,
                                    profile_signout_);
  EXPECT_EQ(Type::kCleared, event.GetEventTypeFor(ConsentLevel::kSignin));
  EXPECT_EQ(Type::kNone, event.GetEventTypeFor(ConsentLevel::kSync));
}

// TODO(crbug.com/40067058): Delete this test when ConsentLevel::kSync is
//     deleted. See ConsentLevel::kSync documentation for details.
TEST_F(PrimaryAccountChangeEventTest, ConsentLevelChangeFromNotRequiredToSync) {
  PrimaryAccountChangeEvent event(empty_signin_, account1_sync_, access_point_);
  EXPECT_EQ(Type::kSet, event.GetEventTypeFor(ConsentLevel::kSignin));
  EXPECT_EQ(Type::kSet, event.GetEventTypeFor(ConsentLevel::kSync));

  event = PrimaryAccountChangeEvent(account1_signin_, account1_sync_,
                                    access_point_);
  EXPECT_EQ(Type::kNone, event.GetEventTypeFor(ConsentLevel::kSignin));
  EXPECT_EQ(Type::kSet, event.GetEventTypeFor(ConsentLevel::kSync));

  event = PrimaryAccountChangeEvent(account1_signin_, account2_sync_,
                                    access_point_);
  EXPECT_EQ(Type::kSet, event.GetEventTypeFor(ConsentLevel::kSignin));
  EXPECT_EQ(Type::kSet, event.GetEventTypeFor(ConsentLevel::kSync));
}

// TODO(crbug.com/40067058): Delete this test when ConsentLevel::kSync is
//     deleted. See ConsentLevel::kSync documentation for details.
TEST_F(PrimaryAccountChangeEventTest, ConsentLevelChangeFromSyncToNotRequired) {
  PrimaryAccountChangeEvent event(account1_sync_, account1_signin_,
                                  profile_signout_);
  EXPECT_EQ(Type::kNone, event.GetEventTypeFor(ConsentLevel::kSignin));
  EXPECT_EQ(Type::kCleared, event.GetEventTypeFor(ConsentLevel::kSync));

  event = PrimaryAccountChangeEvent(account1_sync_, empty_signin_,
                                    profile_signout_);
  EXPECT_EQ(Type::kCleared, event.GetEventTypeFor(ConsentLevel::kSignin));
  EXPECT_EQ(Type::kCleared, event.GetEventTypeFor(ConsentLevel::kSync));
}

TEST_F(PrimaryAccountChangeEventTest, StatesAndEventSourceAreValid) {
  // The states cannot have an empty primary account and the consent level
  // kSync.
  EXPECT_FALSE(signin::PrimaryAccountChangeEvent::StatesAndEventSourceAreValid(
      empty_sync_, account1_signin_, access_point_));
  EXPECT_FALSE(signin::PrimaryAccountChangeEvent::StatesAndEventSourceAreValid(
      account1_signin_, empty_sync_, profile_signout_));

  // If the previous state's primary account is empty and the current state's is
  // not, the event source should be an access point.
  EXPECT_TRUE(signin::PrimaryAccountChangeEvent::StatesAndEventSourceAreValid(
      empty_signin_, account1_signin_, access_point_));
  EXPECT_FALSE(signin::PrimaryAccountChangeEvent::StatesAndEventSourceAreValid(
      empty_signin_, account1_signin_, profile_signout_));

  // If the previous state's primary account is not empty and the current
  // state's is empty, the event source should be a profile sign out.
  EXPECT_TRUE(signin::PrimaryAccountChangeEvent::StatesAndEventSourceAreValid(
      account1_signin_, empty_signin_, profile_signout_));
  EXPECT_FALSE(signin::PrimaryAccountChangeEvent::StatesAndEventSourceAreValid(
      account1_signin_, empty_signin_, access_point_));

  // If the previous state's consent level is kSignin and the current state's is
  // kSync, the event source should be an access point.
  EXPECT_TRUE(signin::PrimaryAccountChangeEvent::StatesAndEventSourceAreValid(
      account1_signin_, account1_sync_, access_point_));
  EXPECT_FALSE(signin::PrimaryAccountChangeEvent::StatesAndEventSourceAreValid(
      account1_signin_, account1_sync_, profile_signout_));

  // If the previous state's consent level is kSync and the current state's is
  // kSignin, the event source should be a profile sign out.
  EXPECT_TRUE(signin::PrimaryAccountChangeEvent::StatesAndEventSourceAreValid(
      account1_sync_, account1_signin_, profile_signout_));
  EXPECT_FALSE(signin::PrimaryAccountChangeEvent::StatesAndEventSourceAreValid(
      account1_sync_, account1_signin_, access_point_));

  // If the primary account changes and the states' consent level stay the same,
  // the event source should be an access point.
  EXPECT_TRUE(signin::PrimaryAccountChangeEvent::StatesAndEventSourceAreValid(
      account1_sync_, account2_sync_, access_point_));
  EXPECT_FALSE(signin::PrimaryAccountChangeEvent::StatesAndEventSourceAreValid(
      account1_sync_, account2_sync_, profile_signout_));
  EXPECT_TRUE(signin::PrimaryAccountChangeEvent::StatesAndEventSourceAreValid(
      account1_signin_, account2_signin_, access_point_));
  EXPECT_FALSE(signin::PrimaryAccountChangeEvent::StatesAndEventSourceAreValid(
      account1_signin_, account2_signin_, profile_signout_));
}

TEST_F(PrimaryAccountChangeEventTest, ToStringSupported) {
  PrimaryAccountChangeEvent event(empty_signin_, account1_signin_,
                                  access_point_);

  std::stringstream sstream;
  sstream << event;

  EXPECT_EQ(
      sstream.str(),
      "{ previous_state: { primary_account: , consent_level: }, "
      "current_state: { primary_account: account1, consent_level: Signin } }");
}
