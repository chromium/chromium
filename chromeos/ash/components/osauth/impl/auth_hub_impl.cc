// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/osauth/impl/auth_hub_impl.h"

#include "base/functional/callback.h"
#include "chromeos/ash/components/osauth/public/auth_factor_engine_factory.h"
#include "chromeos/ash/components/osauth/public/auth_parts.h"

namespace ash {

AuthHubImpl::AuthHubImpl() {
  mode_lifecycle_ = std::make_unique<AuthHubModeLifecycle>(this);
}

AuthHubImpl::~AuthHubImpl() = default;

void AuthHubImpl::InitializeForMode(AuthHubMode target) {
  mode_lifecycle_->SwitchToMode(target);
}

void AuthHubImpl::OnReadyForMode(
    AuthHubMode mode,
    AuthHubModeLifecycle::EnginesMap available_engines) {
  on_initialized_listeners_.Notify();
}

void AuthHubImpl::OnExitedMode(AuthHubMode mode) {}

void AuthHubImpl::OnModeShutdown() {}

void AuthHubImpl::EnsureInitialized(base::OnceClosure on_initialized) {
  if (mode_lifecycle_->IsReady()) {
    std::move(on_initialized).Run();
    return;
  }
  on_initialized_listeners_.AddUnsafe(std::move(on_initialized));
}

}  // namespace ash
