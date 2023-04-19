// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_MOCK_AUTH_PERFORMER_H_
#define CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_MOCK_AUTH_PERFORMER_H_

#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/login/auth/auth_performer.h"
#include "chromeos/ash/components/login/auth/public/auth_session_intent.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

class MockAuthPerformer : public AuthPerformer {
 public:
  explicit MockAuthPerformer(UserDataAuthClient* client);
  MockAuthPerformer(const MockAuthPerformer&) = delete;
  MockAuthPerformer& operator=(const MockAuthPerformer&) = delete;
  ~MockAuthPerformer() override;

  // AuthPerformer
  MOCK_METHOD(void,
              StartAuthSession,
              (std::unique_ptr<UserContext> context,
               bool ephemeral,
               AuthSessionIntent intent,
               StartSessionCallback callback),
              (override));

  MOCK_METHOD(void,
              AuthenticateWithPassword,
              (const std::string& key_label,
               const std::string& password,
               std::unique_ptr<UserContext> context,
               AuthOperationCallback callback),
              (override));
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_MOCK_AUTH_PERFORMER_H_
