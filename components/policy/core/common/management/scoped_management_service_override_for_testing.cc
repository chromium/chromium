// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"

namespace policy {

ScopedManagementServiceOverrideForTesting::
    ScopedManagementServiceOverrideForTesting(ManagementService* service,
                                              uint64_t authorities)
    : service_(service) {
  if (service_->management_authorities_for_testing().has_value())
    previous_authorities_ =
        service_->management_authorities_for_testing().value();
  service_->SetManagementAuthoritiesForTesting(authorities);
}

ScopedManagementServiceOverrideForTesting::
    ~ScopedManagementServiceOverrideForTesting() {
  if (previous_authorities_.has_value()) {
    service_->SetManagementAuthoritiesForTesting(previous_authorities_.value());
  } else {
    service_->ClearManagementAuthoritiesForTesting();
  }
}

}  // namespace policy
