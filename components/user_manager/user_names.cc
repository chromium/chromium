// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/user_manager/user_names.h"

#include "base/memory/singleton.h"
#include "components/account_id/account_id.h"
#include "google_apis/gaia/gaia_auth_util.h"

class AccountId;

namespace {

// Username for Demo session user.
constexpr const char kDemoUserName[] = "demouser@demo.app.local";

// Username for Signin account id.
constexpr const char kSignInUser[] = "sign-in-user-id";

// This is a singleton object that is used to store several
// global AccountIds for special accounts.
class FixedAccountManager {
 public:
  static FixedAccountManager* GetInstance() {
    return base::Singleton<FixedAccountManager>::get();
  }

  FixedAccountManager(const FixedAccountManager&) = delete;
  FixedAccountManager& operator=(const FixedAccountManager&) = delete;

  const AccountId& stub_account_id() const { return stub_account_id_; }
  const AccountId& signin_account_id() const { return signin_account_id_; }
  const AccountId& guest_account_id() const { return guest_account_id_; }
  const AccountId& demo_account_id() const { return demo_account_id_; }

 private:
  friend struct base::DefaultSingletonTraits<FixedAccountManager>;

  FixedAccountManager() = default;

  const AccountId stub_account_id_ =
      AccountId::FromUserEmailGaiaId(user_manager::kStubUserEmail,
                                     user_manager::kStubUserId);
  const AccountId signin_account_id_ = AccountId::FromUserEmail(kSignInUser);
  const AccountId guest_account_id_ =
      AccountId::FromUserEmail(user_manager::kGuestUserName);
  const AccountId demo_account_id_ = AccountId::FromUserEmail(kDemoUserName);
};

}  // namespace

namespace user_manager {

const char kStubUserEmail[] = "stub-user@example.com";
const char kStubUserId[] = "1234567890123456789012";

// Should match cros constant in platform/libchromeos/chromeos/cryptohome.h
const char kGuestUserName[] = "$guest";

const char kSupervisedUserDomain[] = "locally-managed.localhost";

const char kArcKioskDomain[] = "arc-kiosk-apps.device-local.localhost";

std::string CanonicalizeUserID(const std::string& user_id) {
  if (user_id == kGuestUserName)
    return user_id;
  return gaia::CanonicalizeEmail(user_id);
}

// Note: StubAccountId is used for all tests, not only ChromeOS tests.
const AccountId& StubAccountId() {
  return FixedAccountManager::GetInstance()->stub_account_id();
}

const AccountId& SignInAccountId() {
  return FixedAccountManager::GetInstance()->signin_account_id();
}

const AccountId& GuestAccountId() {
  return FixedAccountManager::GetInstance()->guest_account_id();
}

const AccountId& DemoAccountId() {
  return FixedAccountManager::GetInstance()->demo_account_id();
}

}  // namespace user_manager
