// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_MANAGEMENT_SCOPED_MANAGEMENT_SERVICE_OVERRIDE_FOR_TESTING_H_
#define COMPONENTS_POLICY_CORE_COMMON_MANAGEMENT_SCOPED_MANAGEMENT_SERVICE_OVERRIDE_FOR_TESTING_H_

#include <optional>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "components/policy/core/common/management/management_service.h"

namespace policy {
// Sets the management authorities override for |target| on construction, and
// removes it when the object goes out of scope. This class is intended to be
// used by tests that need to override management authorities to ensure their
// overrides are properly handled and reverted when the scope of the test is
// left.
// |authorities| here are the management authorities we want to fake as active
// for the testing purposes.
// Use case example:
//   ScopedManagementServiceOverrideForTesting
//     scoped_management_service_override(
//       service, EnterpriseManagementAuthority::DOMAIN_LOCAL);

class ScopedManagementServiceOverrideForTesting {
 public:
  ScopedManagementServiceOverrideForTesting(ManagementService* service,
                                            uint64_t authorities);
  ~ScopedManagementServiceOverrideForTesting();

 private:
  raw_ptr<ManagementService> service_;
  std::optional<uint64_t> previous_authorities_;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_MANAGEMENT_SCOPED_MANAGEMENT_SERVICE_OVERRIDE_FOR_TESTING_H_
