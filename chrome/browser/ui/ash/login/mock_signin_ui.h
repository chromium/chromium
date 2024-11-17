// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LOGIN_MOCK_SIGNIN_UI_H_
#define CHROME_BROWSER_UI_ASH_LOGIN_MOCK_SIGNIN_UI_H_

#include <memory>

#include "chrome/browser/ui/ash/login/signin_ui.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "components/login/base_screen_handler_utils.h"
#include "components/prefs/pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

class MockSigninUI : public SigninUI {
 public:
  MockSigninUI();
  ~MockSigninUI() override;
  MockSigninUI(const MockSigninUI&) = delete;
  MockSigninUI& operator=(const SigninUI&) = delete;

  MOCK_METHOD(void, StartUserOnboarding, (), (override));
  MOCK_METHOD(void,
              ResumeUserOnboarding,
              (const PrefService&, OobeScreenId),
              (override));
  MOCK_METHOD(void, StartManagementTransition, (), (override));
  MOCK_METHOD(void, ShowTosForExistingUser, (), (override));
  MOCK_METHOD(void, ShowNewTermsForFlexUsers, (), (override));
  MOCK_METHOD(void,
              StartEncryptionMigration,
              (std::unique_ptr<UserContext>,
               EncryptionMigrationMode,
               base::OnceCallback<void(std::unique_ptr<UserContext>)>),
              (override));
  MOCK_METHOD(void,
              SetAuthSessionForOnboarding,
              (const UserContext&),
              (override));
  MOCK_METHOD(void, ClearOnboardingAuthSession, (), (override));
  MOCK_METHOD(void,
              UseAlternativeAuthentication,
              (std::unique_ptr<UserContext> user_context, bool),
              (override));
  MOCK_METHOD(void,
              RunLocalAuthentication,
              (std::unique_ptr<UserContext> user_context),
              (override));
  MOCK_METHOD(void,
              ShowSigninError,
              (SigninError, const std::string&),
              (override));
  MOCK_METHOD(void,
              SAMLConfirmPassword,
              (::login::StringList, std::unique_ptr<UserContext>),
              (override));
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_LOGIN_MOCK_SIGNIN_UI_H_
