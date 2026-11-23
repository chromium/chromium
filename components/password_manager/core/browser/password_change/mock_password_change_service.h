// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_CHANGE_MOCK_PASSWORD_CHANGE_SERVICE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_CHANGE_MOCK_PASSWORD_CHANGE_SERVICE_H_

#include "components/password_manager/core/browser/password_change/password_change_service_interface.h"
#include "testing/gmock/include/gmock/gmock.h"

class GURL;

namespace password_manager {

class MockPasswordChangeService : public PasswordChangeServiceInterface {
 public:
  MockPasswordChangeService();
  ~MockPasswordChangeService();

  MOCK_METHOD(bool, IsPasswordChangeAvailable, (), (const, override));
  MOCK_METHOD(bool,
              IsPasswordChangeSupported,
              (const LeakedPasswordDetails&, bool),
              (const, override));
  MOCK_METHOD(void,
              RecordLoginAttemptQuality,
              (password_manager::LogInWithChangedPasswordOutcome, const GURL&),
              (const, override));
  MOCK_METHOD(void, AddChangePasswordUrlOverride, (const GURL&), (override));
  MOCK_METHOD(PasswordChangeCredentialsProvider*,
              GetCredentialsProviderForActorSurface,
              (int32_t),
              (override));
};
}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_CHANGE_MOCK_PASSWORD_CHANGE_SERVICE_H_
