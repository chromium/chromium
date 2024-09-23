// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_AUTH_FACTOR_PRESENCE_CACHE_H_
#define CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_AUTH_FACTOR_PRESENCE_CACHE_H_

#include <memory>

#include "base/component_export.h"
#include "chromeos/ash/components/osauth/public/common_types.h"
#include "components/user_manager/known_user.h"

class PrefService;

namespace ash {

// When authentication is attempted it is important for the UI part of
// authentication code to get a list of available factors in a synchronous
// way. However, the actual nature of factors makes factor presence check
// an asynchronous operation. The solution is the caching the results
// of the previous check.
// This class encapsulates details of such caching.

class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_OSAUTH) AuthFactorPresenceCache {
 public:
  explicit AuthFactorPresenceCache(PrefService* local_state);
  ~AuthFactorPresenceCache();

  void StoreFactorPresenceCache(AuthAttemptVector vector,
                                AuthFactorsSet factors);
  AuthFactorsSet GetExpectedFactorsPresence(AuthAttemptVector vector);

 private:
  std::unique_ptr<user_manager::KnownUser> known_user_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_AUTH_FACTOR_PRESENCE_CACHE_H_
