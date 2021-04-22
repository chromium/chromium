// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_MANAGEMENT_SCOPED_MANAGEMENT_SERVICE_OVERRIDE_FOR_TESTING_H_
#define COMPONENTS_POLICY_CORE_COMMON_MANAGEMENT_SCOPED_MANAGEMENT_SERVICE_OVERRIDE_FOR_TESTING_H_

#include "base/containers/flat_set.h"
#include "components/policy/core/common/management/management_service.h"

namespace policy {
// Sets the management authorities override for |target| on construction, and
// removes it when the object goes out of scope. This class is intended to be
// used by tests that need to override management authorities to ensure their
// overrides are properly handled and reverted when the scope of the test is
// left. This class does not support nested scopes. There can only be one
// override for the same |target| in a scope.
// |target| here can be either the platform or the browser.
// |authorities| here are the management authorities we want to fake as active
// for the testing purposes.
// Use case example:
//   ScopedManagementServiceOverrideForTesting
//     scoped_management_service_override(ManagementTarget::PLATFORM,
//       base::flat_set<EnterpriseManagementAuthority{
//         EnterpriseManagementAuthority::DOMAIN_LOCAL
//       }));

class ScopedManagementServiceOverrideForTesting {
 public:
  ScopedManagementServiceOverrideForTesting(
      ManagementTarget target,
      base::flat_set<EnterpriseManagementAuthority> authorities);
  ~ScopedManagementServiceOverrideForTesting();

 private:
  ManagementTarget target_;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_MANAGEMENT_SCOPED_MANAGEMENT_SERVICE_OVERRIDE_FOR_TESTING_H_
