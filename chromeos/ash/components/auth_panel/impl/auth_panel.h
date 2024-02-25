// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_AUTH_PANEL_IMPL_AUTH_PANEL_H_
#define CHROMEOS_ASH_COMPONENTS_AUTH_PANEL_IMPL_AUTH_PANEL_H_

#include <vector>

#include "base/containers/flat_map.h"
#include "chromeos/ash/components/auth_panel/public/shared_types.h"
#include "chromeos/ash/components/osauth/public/auth_factor_status_consumer.h"
#include "chromeos/ash/components/osauth/public/common_types.h"
#include "ui/views/view.h"

namespace ash {

class AuthPanelEventDispatcher;
class AuthPanelEventDispatcherFactory;
class AuthFactorStore;
class AuthFactorStoreFactory;
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
class AuthPanel : public views::View, public AuthFactorStatusConsumer {
 public:
  AuthPanel(
      std::unique_ptr<FactorAuthViewFactory> view_factory,
      std::unique_ptr<AuthFactorStoreFactory> store_factory,
      std::unique_ptr<AuthPanelEventDispatcherFactory> event_dispatcher_factory,
      auth_panel::AuthCompletionCallback on_auth_complete);
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
  void OnFactorCustomSignal(AshAuthFactor factor) override;
  void OnFactorAuthFailure(AshAuthFactor factor) override;
  void OnFactorAuthSuccess(AshAuthFactor factor) override;
  void OnEndAuthentication() override;

 private:
  void InitializeViewPlaceholders();

  std::unique_ptr<AuthPanelEventDispatcherFactory> event_dispatcher_factory_;
  std::unique_ptr<FactorAuthViewFactory> view_factory_;
  std::unique_ptr<AuthFactorStoreFactory> store_factory_;

  std::unique_ptr<AuthFactorStore> store_;
  std::unique_ptr<AuthPanelEventDispatcher> event_dispatcher_;
  base::flat_map<AshAuthFactor, views::View*> views_;

  auth_panel::AuthCompletionCallback on_auth_complete_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_AUTH_PANEL_IMPL_AUTH_PANEL_H_
