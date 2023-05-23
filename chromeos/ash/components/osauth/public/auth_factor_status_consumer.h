// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_OSAUTH_PUBLIC_AUTH_FACTOR_STATUS_CONSUMER_H_
#define CHROMEOS_ASH_COMPONENTS_OSAUTH_PUBLIC_AUTH_FACTOR_STATUS_CONSUMER_H_

#include "base/component_export.h"
#include "base/containers/enum_set.h"
#include "base/containers/flat_map.h"
#include "chromeos/ash/components/osauth/public/common_types.h"

namespace ash {

// Opaque class that represents state of current user authentication attempt,
// used to interact with particular AuthFactors via API.
class AuthHubConnector;

// Authentication factor state. This is a superset of states for all factors,
// so some states might not be applicable for individual factors.
enum class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_OSAUTH) AuthFactorState {
  kCheckingForPresence,
  kEngineError,
  kNoFactor,
  kFactorReady,
  kDisabledByPolicy,
  kDisabledStrongFactorRequired,
  kLockedOutIndefinite,
  kLockedOutTemporary,
  kDisabledFactorSpecific,
  kDisabledParallelAttempt,
  kOngoingAttempt,
  kMaxValue = kOngoingAttempt,
};

using AuthFactorStateSet = base::EnumSet<AuthFactorState,
                                         AuthFactorState::kCheckingForPresence,
                                         AuthFactorState::kMaxValue>;

using FactorsStatusMap = base::flat_map<AshAuthFactor, AuthFactorState>;

// Interface between AuthHub and a UI panel that is embedded into authentication
// surface and displays available factors and their statuses.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_OSAUTH)
    AuthFactorStatusConsumer {
 public:
  virtual ~AuthFactorStatusConsumer() = default;

  // `AuthHub` calls this method, immediately after it gets a reference to
  // consumer from  `AuthAttemptConsumer::OnUserAuthAttemptConfirmed` call.
  // It is expected that all factors are in `kCheckingForPresence` state
  // initially, and subsequent `OnFactorStatusesChanged` would provide an
  // update. The `connector` provides a way to interact with individual
  // factors.
  virtual void InitializeUi(AuthFactorsSet factors,
                            AuthHubConnector* connector) = 0;

  // This should be a very rare event, happening only when AuthHub's
  // cached data on available factors is stale, and querying AuthEngines
  // results in a different set of factors. Map includes all present factors
  // and their statuses.
  // It is expected that UI might be fully rebuilt in this case.
  virtual void OnFactorListChanged(FactorsStatusMap factors_with_status) = 0;

  // This method would be called by AuthHub when some factor's state changes.
  // `incremental_update` contains data only for updated factor statuses.
  //
  // While usually there would be single element in the map, there are few
  // cases when update would affect multiple factors:
  // * First update after `InitializeUi`, which would provide initial state;
  // * Authentication attempt with particular factor would change state of
  //   that factor to `kOngoingAttempt` and all other factors to
  //   `kDisabledParallelAttempt`;
  // * Policy change might affect multiple factors.
  virtual void OnFactorStatusesChanged(FactorsStatusMap incremental_update) = 0;

  // This method would be called by AuthHub upon attempt failure.
  // UI can provide additional visual feedback in this case.
  // The order of notifications is the following:
  // 1) `OnFactorStatusesChanged`
  // 2) `OnAuthenticationFailure`
  virtual void OnFactorAuthFailure(AshAuthFactor factor) = 0;

  // This method would be called by AuthHub upon auth success.
  // There will be no prior `OnFactorStatusesChanged` call, factor would
  // still be in `kOngoingAttempt` state, to prevent additional attempts.
  virtual void OnFactorAuthSuccess(AshAuthFactor factor) = 0;

  // This method would be called by AuthHub to indicate end of interaction
  // between AuthHub and UI panel. After this call AuthHub drops a reference
  // to this object and `connector` object from `InitializeUi` call should also
  // be considered invalid.
  virtual void OnEndAuthentication() = 0;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_OSAUTH_PUBLIC_AUTH_FACTOR_STATUS_CONSUMER_H_
