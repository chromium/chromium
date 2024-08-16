// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SHARING_MOCK_PASSWORD_SENDER_SERVICE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SHARING_MOCK_PASSWORD_SENDER_SERVICE_H_

#include "components/password_manager/core/browser/sharing/password_sender_service.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace password_manager {

class MockPasswordSenderService : public PasswordSenderService {
 public:
  MockPasswordSenderService();
  ~MockPasswordSenderService() override;
  MOCK_METHOD(void,
              SendPasswords,
              (const std::vector<PasswordForm>& passwords,
               const PasswordRecipient& recipient),
              (override));

  MOCK_METHOD(base::WeakPtr<syncer::DataTypeControllerDelegate>,
              GetControllerDelegate,
              (),
              (override));
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SHARING_MOCK_PASSWORD_SENDER_SERVICE_H_
