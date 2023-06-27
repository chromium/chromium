// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/auth_panel/auth_panel_event_dispatcher.h"

#include "base/notreached.h"
#include "base/ranges/algorithm.h"

namespace ash {

AuthPanelEventDispatcher::AuthPanelEventDispatcher(
    base::raw_ptr<AuthFactorStore> store)
    : store_(store) {}

AuthPanelEventDispatcher::~AuthPanelEventDispatcher() = default;

void AuthPanelEventDispatcher::DispatchEvent(AshAuthFactor factor,
                                             const UserAction& action) {
  NOTIMPLEMENTED();
}

void AuthPanelEventDispatcher::DispatchEvent(AshAuthFactor factor,
                                             AuthVerdict verdict) {
  NOTIMPLEMENTED();
}

void AuthPanelEventDispatcher::DispatchEvent(AshAuthFactor factor,
                                             AuthFactorState state) {
  NOTIMPLEMENTED();
}

}  // namespace ash
