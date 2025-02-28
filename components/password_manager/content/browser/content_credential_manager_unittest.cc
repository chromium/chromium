// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/content/browser/content_credential_manager.h"

#include "base/test/mock_callback.h"
#include "components/password_manager/core/browser/credential_manager_interface.h"
#include "components/password_manager/core/common/credential_manager_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

class MockCredentialManagerImpl : public CredentialManagerInterface {
 public:
  MOCK_METHOD(void,
              Store,
              (const CredentialInfo& credential, StoreCallback callback),
              (override));
  MOCK_METHOD(void,
              PreventSilentAccess,
              (PreventSilentAccessCallback callback),
              (override));
  MOCK_METHOD(void,
              Get,
              (CredentialMediationRequirement mediation,
               int requested_credential_type_flags,
               const std::vector<GURL>& federations,
               GetCallback callback),
              (override));
  MOCK_METHOD(void, ResetPendingRequest, (), (override));
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
  content_credential_manager.Store(CredentialInfo(), StoreCallback());
}

}  // namespace password_manager
