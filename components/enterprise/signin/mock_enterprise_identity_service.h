// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_SIGNIN_MOCK_ENTERPRISE_IDENTITY_SERVICE_H_
#define COMPONENTS_ENTERPRISE_SIGNIN_MOCK_ENTERPRISE_IDENTITY_SERVICE_H_

#include "base/functional/callback.h"
#include "components/enterprise/signin/enterprise_identity_service.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace enterprise {

class MockEnterpriseIdentityService : public EnterpriseIdentityService {
 public:
  MockEnterpriseIdentityService();
  ~MockEnterpriseIdentityService() override;

  MOCK_METHOD(void,
              GetManagedAccountsWithRefreshTokens,
              (GetManagedAccountsCallback),
              (override));
  MOCK_METHOD(void,
              GetManagedAccountsAccessTokens,
              (base::OnceCallback<void(std::vector<std::string>)>),
              (override));
  MOCK_METHOD(void,
              AddObserver,
              (EnterpriseIdentityService::Observer*),
              (override));
  MOCK_METHOD(void,
              RemoveObserver,
              (EnterpriseIdentityService::Observer*),
              (override));
};

}  // namespace enterprise

#endif  // COMPONENTS_ENTERPRISE_SIGNIN_MOCK_ENTERPRISE_IDENTITY_SERVICE_H_
