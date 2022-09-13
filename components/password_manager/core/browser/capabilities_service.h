// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_CAPABILITIES_SERVICE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_CAPABILITIES_SERVICE_H_

#include <set>
#include <vector>

#include "base/callback.h"
#include "url/origin.h"

namespace password_manager {

// The class fetches capabilities (e.g. availability of a script for automated
// password changes) for different origins.
class CapabilitiesService {
 public:
  using ResponseCallback =
      base::OnceCallback<void(const std::set<url::Origin>&)>;

  virtual ~CapabilitiesService() = default;

  // Returns the subset of provided |origins| for which a password change script
  // is available. In case of a network error while fetching capabilities, the
  // result list will be empty.
  virtual void QueryPasswordChangeScriptAvailability(
      const std::vector<url::Origin>& origins,
      ResponseCallback callback) = 0;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_CAPABILITIES_SERVICE_H_
