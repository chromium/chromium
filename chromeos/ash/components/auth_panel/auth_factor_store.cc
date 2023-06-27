// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/auth_panel/auth_factor_store.h"

#include "base/notreached.h"
#include "chromeos/ash/components/osauth/public/common_types.h"

namespace ash {

AuthFactorStore::AuthFactorStore() = default;

AuthFactorStore::~AuthFactorStore() = default;

void AuthFactorStore::OnUserAction(
    AshAuthFactor factor,
    const AuthPanelEventDispatcher::UserAction& action) {
  NOTIMPLEMENTED();
}

void AuthFactorStore::OnFactorStateChanged(AshAuthFactor factor,
                                           AuthFactorState state) {
  NOTIMPLEMENTED();
}

void AuthFactorStore::OnAuthVerdict(
    AshAuthFactor factor,
    AuthPanelEventDispatcher::AuthVerdict verdict) {
  NOTIMPLEMENTED();
}

}  // namespace ash
