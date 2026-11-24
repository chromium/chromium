// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_CHANGE_MOCK_PASSWORD_CHANGE_CREDENTIALS_PROVIDER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_CHANGE_MOCK_PASSWORD_CHANGE_CREDENTIALS_PROVIDER_H_

#include "base/types/expected.h"
#include "components/password_manager/core/browser/password_change/password_change_credentials_provider.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_string.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace password_manager {

class MockPasswordChangeCredentialsProvider
    : public PasswordChangeCredentialsProvider {
 public:
  MockPasswordChangeCredentialsProvider();
  ~MockPasswordChangeCredentialsProvider() override;

  MOCK_METHOD((base::expected<PasswordString, Error>),
              GetOldPassword,
              (),
              (const, override));
  MOCK_METHOD((base::expected<PasswordString, Error>),
              GetNewPassword,
              (PasswordFormManager * form_manager),
              (override));
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_CHANGE_MOCK_PASSWORD_CHANGE_CREDENTIALS_PROVIDER_H_
