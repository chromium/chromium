// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_AUTH_PANEL_AUTH_FACTOR_STORE_H_
#define CHROMEOS_ASH_COMPONENTS_AUTH_PANEL_AUTH_FACTOR_STORE_H_

#include "base/callback_list.h"
#include "chromeos/ash/components/auth_panel/auth_panel_event_dispatcher.h"
#include "chromeos/ash/components/auth_panel/factor_auth_view.h"
#include "chromeos/ash/components/osauth/public/common_types.h"

namespace ash {

// This class encapsulates the UI state of `AuthPanel`.
class AuthFactorStore {
 public:
  using OnStateUpdatedCallback = base::RepeatingCallback<void()>;

  AuthFactorStore();
  ~AuthFactorStore();

  void Subscribe(OnStateUpdatedCallback callback);

  void OnUserAction(AshAuthFactor factor,
                    const AuthPanelEventDispatcher::UserAction& action);
  void OnFactorStateChanged(AshAuthFactor factor, AuthFactorState state);
  void OnAuthVerdict(AshAuthFactor factor,
                     AuthPanelEventDispatcher::AuthVerdict verdict);
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_AUTH_PANEL_AUTH_FACTOR_STORE_H_
