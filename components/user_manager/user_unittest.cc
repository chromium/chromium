// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_manager/user.h"

#include "base/memory/raw_ptr.h"
#include "components/account_id/account_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace user_manager {

namespace {

const char kEmail[] = "email@example.com";
const char kGaiaId[] = "fake_gaia_id";

}  // namespace

TEST(UserTest, DeviceLocalAccountAffiliation) {
  // This implementation of RAII for User* is to prevent memory leak.
  // Smart pointers are not friends of User and can't call protected destructor.
  class ScopedUser {
   public:
    ScopedUser(const User* const user) : user_(user) {}

    ScopedUser(const ScopedUser&) = delete;
    ScopedUser& operator=(const ScopedUser&) = delete;

    ~ScopedUser() { delete user_; }

    bool IsAffiliated() const { return user_ && user_->IsAffiliated(); }

   private:
    const raw_ptr<const User, DanglingUntriaged> user_;
  };

  const AccountId account_id = AccountId::FromUserEmailGaiaId(kEmail, kGaiaId);

  ScopedUser kiosk_user(User::CreateKioskAppUser(account_id));
  EXPECT_TRUE(kiosk_user.IsAffiliated());

  ScopedUser public_session_user(User::CreatePublicAccountUser(account_id));
  EXPECT_TRUE(public_session_user.IsAffiliated());

  ScopedUser web_kiosk_user(User::CreateWebKioskAppUser(account_id));
  EXPECT_TRUE(web_kiosk_user.IsAffiliated());
}

}  // namespace user_manager
