// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/mock_policy_service.h"

namespace policy {

MockPolicyServiceObserver::MockPolicyServiceObserver() = default;

MockPolicyServiceObserver::~MockPolicyServiceObserver() = default;

MockPolicyServiceProviderUpdateObserver::
    MockPolicyServiceProviderUpdateObserver() = default;

MockPolicyServiceProviderUpdateObserver::
    ~MockPolicyServiceProviderUpdateObserver() = default;

MockPolicyService::MockPolicyService() = default;

MockPolicyService::~MockPolicyService() = default;

}  // namespace policy
