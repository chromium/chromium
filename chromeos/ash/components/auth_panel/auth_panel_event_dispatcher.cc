// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/auth_panel/auth_panel_event_dispatcher.h"

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "chromeos/ash/components/auth_panel/auth_factor_store.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

AuthPanelEventDispatcher::UserAction::UserAction(
    Type type,
    absl::optional<std::string> payload)
    : type_(type), payload_(payload) {}

AuthPanelEventDispatcher::UserAction::~UserAction() = default;

AuthPanelEventDispatcher::AuthPanelEventDispatcher(
    raw_ptr<AuthFactorStore> store)
    : store_(store) {}

AuthPanelEventDispatcher::~AuthPanelEventDispatcher() = default;

void AuthPanelEventDispatcher::DispatchEvent(const UserAction& action) {
  store_->OnUserAction(action);
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
