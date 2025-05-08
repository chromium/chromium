// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/credential_management/content_credential_manager.h"

#include "base/test/mock_callback.h"
#include "components/credential_management/credential_manager_interface.h"
#include "components/password_manager/core/common/credential_manager_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace credential_management {

class MockCredentialManagerImpl
    : public credential_management::CredentialManagerInterface {
 public:
  MOCK_METHOD(void,
              Store,
              (const password_manager::CredentialInfo& credential,
               credential_management::StoreCallback callback),
              (override));
  MOCK_METHOD(void,
              PreventSilentAccess,
              (PreventSilentAccessCallback callback),
              (override));
  MOCK_METHOD(void,
              Get,
              (password_manager::CredentialMediationRequirement mediation,
               bool include_passwords,
               const std::vector<GURL>& federations,
               GetCallback callback),
              (override));
  MOCK_METHOD(void, ResetAfterDisconnecting, (), (override));
};

// Tests ContentCredentialManager functionality.
class ContentCredentialManagerTest : public testing::Test {
 public:
  ContentCredentialManagerTest() = default;

  ~ContentCredentialManagerTest() override = default;
};

TEST_F(ContentCredentialManagerTest,
       StoreCallIsForwardedToCredentialManagerImpl) {
  auto mock_credential_manager_unique_ptr =
      std::make_unique<testing::StrictMock<MockCredentialManagerImpl>>();
  auto mock_credential_manager = mock_credential_manager_unique_ptr.get();
  auto content_credential_manager =
      ContentCredentialManager(std::move(mock_credential_manager_unique_ptr));

  EXPECT_CALL(*mock_credential_manager, Store);
  content_credential_manager.Store(password_manager::CredentialInfo(),
                                   StoreCallback());
}

}  // namespace credential_management
