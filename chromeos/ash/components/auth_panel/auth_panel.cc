// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/auth_panel/auth_panel.h"

#include "base/logging.h"
#include "base/notreached.h"
#include "chromeos/ash/components/auth_panel/auth_panel_event_dispatcher.h"
#include "chromeos/ash/components/auth_panel/factor_auth_view.h"
#include "chromeos/ash/components/auth_panel/factor_auth_view_factory.h"
#include "chromeos/ash/components/osauth/public/common_types.h"

namespace ash {

AuthPanel::AuthPanel(std::unique_ptr<FactorAuthViewFactory> view_factory,
                     std::unique_ptr<AuthPanelEventDispatcher> event_dispatcher)
    : view_factory_(std::move(view_factory)),
      event_dispatcher_(std::move(event_dispatcher)) {}

AuthPanel::~AuthPanel() = default;

void AuthPanel::InitializeUi(AuthFactorsSet factors,
                             AuthHubConnector* connector) {
  CHECK(views_.empty());
  for (const auto factor : factors) {
    auto view = view_factory_->CreateFactorAuthView(factor);
    view->OnFactorStateChanged(AuthFactorState::kCheckingForPresence);
    views_[factor] = std::move(view);
  }
}

void AuthPanel::OnFactorListChanged(FactorsStatusMap factors_with_status) {
  views_.clear();
  for (const auto& [factor, status] : factors_with_status) {
    auto view = view_factory_->CreateFactorAuthView(factor);
    view->OnFactorStateChanged(status);
    views_[factor] = std::move(view);
  }
}

void AuthPanel::OnFactorStatusesChanged(FactorsStatusMap incremental_update) {
  for (const auto& [factor, status] : incremental_update) {
    CHECK(views_.contains(factor));
    views_[factor]->OnFactorStateChanged(status);
  }
}

void AuthPanel::OnFactorAuthFailure(AshAuthFactor factor) {
  CHECK(views_.contains(factor));
  views_[factor]->OnAuthFailure();
}

void AuthPanel::OnFactorAuthSuccess(AshAuthFactor factor) {
  CHECK(views_.contains(factor));
  views_[factor]->OnAuthSuccess();
}

void AuthPanel::OnEndAuthentication() {
  NOTIMPLEMENTED();
}

}  // namespace ash
