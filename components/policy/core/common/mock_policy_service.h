// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_MOCK_POLICY_SERVICE_H_
#define COMPONENTS_POLICY_CORE_COMMON_MOCK_POLICY_SERVICE_H_

#include "components/policy/core/common/policy_service.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace policy {

class MockPolicyServiceObserver : public PolicyService::Observer {
 public:
  MockPolicyServiceObserver();
  ~MockPolicyServiceObserver() override;

  MOCK_METHOD3(OnPolicyUpdated, void(const PolicyNamespace&,
                                     const PolicyMap& previous,
                                     const PolicyMap& current));
  MOCK_METHOD1(OnPolicyServiceInitialized, void(PolicyDomain));
};

class MockPolicyServiceProviderUpdateObserver
    : public PolicyService::ProviderUpdateObserver {
 public:
  MockPolicyServiceProviderUpdateObserver();
  ~MockPolicyServiceProviderUpdateObserver() override;

  MOCK_METHOD1(OnProviderUpdatePropagated,
               void(ConfigurationPolicyProvider* provider));
};

class MockPolicyService : public PolicyService {
 public:
  MockPolicyService();
  ~MockPolicyService() override;

  MOCK_METHOD2(AddObserver, void(PolicyDomain, Observer*));
  MOCK_METHOD2(RemoveObserver, void(PolicyDomain, Observer*));
  MOCK_METHOD1(AddProviderUpdateObserver, void(ProviderUpdateObserver*));
  MOCK_METHOD1(RemoveProviderUpdateObserver, void(ProviderUpdateObserver*));
  MOCK_CONST_METHOD1(HasProvider, bool(ConfigurationPolicyProvider*));

  MOCK_CONST_METHOD1(GetPolicies, const PolicyMap&(const PolicyNamespace&));
  MOCK_CONST_METHOD1(IsInitializationComplete, bool(PolicyDomain domain));
  MOCK_METHOD1(RefreshPolicies, void(const base::Closure&));
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_MOCK_POLICY_SERVICE_H_
