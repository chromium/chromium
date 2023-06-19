// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_AUTH_PANEL_AUTH_PANEL_H_
#define CHROMEOS_ASH_COMPONENTS_AUTH_PANEL_AUTH_PANEL_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "chromeos/ash/components/auth_panel/auth_panel_event_dispatcher.h"
#include "chromeos/ash/components/osauth/public/auth_factor_status_consumer.h"
#include "chromeos/ash/components/osauth/public/common_types.h"

namespace ash {

class FactorAuthView;
class FactorAuthViewFactory;

// Controller class that orchestrates the several `FactorAuthView` objects.
// Responsible for :
// - Listening for changes in auth factor state, auth attempt
// results, and propagating them to the respective `FactorAuthView` objects.
// - The general layout of the authentication UI, hiding and
// showing UI elements for particular auth factors when their status change.
// - Tracking selected factors, in the event where a factor can be toggled,
// for instance, with password/pin.
class AuthPanel : public AuthFactorStatusConsumer {
 public:
  AuthPanel(std::unique_ptr<FactorAuthViewFactory> view_factory,
            std::unique_ptr<AuthPanelEventDispatcher> event_dispatcher);
  AuthPanel(const AuthPanel&) = delete;
  AuthPanel(AuthPanel&&) = delete;
  AuthPanel& operator=(const AuthPanel&) = delete;
  AuthPanel& operator=(AuthPanel&&) = delete;
  ~AuthPanel() override;

  // AuthFactorStatusConsumer:
  void InitializeUi(AuthFactorsSet factors,
                    AuthHubConnector* connector) override;
  void OnFactorListChanged(FactorsStatusMap factors_with_status) override;
  void OnFactorStatusesChanged(FactorsStatusMap incremental_update) override;
  void OnFactorAuthFailure(AshAuthFactor factor) override;
  void OnFactorAuthSuccess(AshAuthFactor factor) override;
  void OnEndAuthentication() override;

 private:
  base::flat_map<AshAuthFactor, std::unique_ptr<FactorAuthView>> views_;
  std::unique_ptr<FactorAuthViewFactory> view_factory_;
  std::unique_ptr<AuthPanelEventDispatcher> event_dispatcher_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_AUTH_PANEL_AUTH_PANEL_H_
