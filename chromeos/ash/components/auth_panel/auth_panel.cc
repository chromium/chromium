// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/auth_panel/auth_panel.h"

#include "base/logging.h"
#include "base/notreached.h"
#include "chromeos/ash/components/auth_panel/auth_factor_store.h"
#include "chromeos/ash/components/auth_panel/auth_panel_event_dispatcher.h"
#include "chromeos/ash/components/auth_panel/factor_auth_view.h"
#include "chromeos/ash/components/auth_panel/factor_auth_view_factory.h"
#include "chromeos/ash/components/osauth/public/auth_factor_status_consumer.h"
#include "chromeos/ash/components/osauth/public/common_types.h"

namespace ash {

AuthPanel::AuthPanel(std::unique_ptr<FactorAuthViewFactory> view_factory,
                     std::unique_ptr<AuthFactorStore> store,
                     std::unique_ptr<AuthPanelEventDispatcher> event_dispatcher)
    : event_dispatcher_(std::move(event_dispatcher)),
      view_factory_(std::move(view_factory)),
      store_(std::move(store)) {}

AuthPanel::~AuthPanel() = default;

void AuthPanel::InitializeUi(AuthFactorsSet factors,
                             AuthHubConnector* connector) {
  for (auto&& factor : factors) {
    views_.push_back(view_factory_->CreateFactorAuthView(factor));
    event_dispatcher_->DispatchEvent(factor,
                                     AuthFactorState::kCheckingForPresence);
  }
}

void AuthPanel::OnFactorListChanged(FactorsStatusMap factors_with_status) {
  for (const auto& [factor, status] : factors_with_status) {
    event_dispatcher_->DispatchEvent(factor, status);
  }
}

void AuthPanel::OnFactorStatusesChanged(FactorsStatusMap incremental_update) {
  for (const auto& [factor, status] : incremental_update) {
    event_dispatcher_->DispatchEvent(factor, status);
  }
}

void AuthPanel::OnFactorAuthFailure(AshAuthFactor factor) {
  event_dispatcher_->DispatchEvent(
      factor, AuthPanelEventDispatcher::AuthVerdict::kFailure);
}

void AuthPanel::OnFactorAuthSuccess(AshAuthFactor factor) {
  event_dispatcher_->DispatchEvent(
      factor, AuthPanelEventDispatcher::AuthVerdict::kSuccess);
}

void AuthPanel::OnEndAuthentication() {
  NOTIMPLEMENTED();
}

}  // namespace ash
