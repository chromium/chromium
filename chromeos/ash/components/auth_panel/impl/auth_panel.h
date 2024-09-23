// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_AUTH_PANEL_IMPL_AUTH_PANEL_H_
#define CHROMEOS_ASH_COMPONENTS_AUTH_PANEL_IMPL_AUTH_PANEL_H_

#include <vector>

#include "ash/login/ui/non_accessible_view.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "chromeos/ash/components/auth_panel/public/shared_types.h"
#include "chromeos/ash/components/osauth/public/auth_factor_status_consumer.h"
#include "chromeos/ash/components/osauth/public/auth_hub.h"
#include "chromeos/ash/components/osauth/public/common_types.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/view.h"

namespace ash {

class AuthHubConnector;
class AuthPanelEventDispatcher;
class AuthPanelEventDispatcherFactory;
class AuthFactorStore;
class AuthFactorStoreFactory;
class FactorAuthView;
class FactorAuthViewFactory;
class PasswordAuthView;

// Controller class that orchestrates the several `FactorAuthView` objects.
// Responsible for :
// - Listening for changes in auth factor state, auth attempt
// results, and propagating them to the respective `FactorAuthView` objects.
// - The general layout of the authentication UI, hiding and
// showing UI elements for particular auth factors when their status change.
// - Tracking selected factors, in the event where a factor can be toggled,
// for instance, with password/pin.
class AuthPanel : public NonAccessibleView, public AuthFactorStatusConsumer {
  METADATA_HEADER(AuthPanel, views::View)

 public:
  class TestApi {
   public:
    explicit TestApi(AuthPanel*);
    ~TestApi();
    TestApi(const TestApi&) = delete;
    TestApi& operator=(const TestApi&) = delete;

    PasswordAuthView* GetPasswordAuthView();

    AuthHubConnector* GetAuthHubConnector();

    void SetSubmitPasswordCallback(auth_panel::SubmitPasswordCallback);

   private:
    raw_ptr<AuthPanel> auth_panel_;
  };

  AuthPanel(
      std::unique_ptr<FactorAuthViewFactory> view_factory,
      std::unique_ptr<AuthFactorStoreFactory> store_factory,
      std::unique_ptr<AuthPanelEventDispatcherFactory> event_dispatcher_factory,
      base::OnceClosure on_end_authentication,
      base::RepeatingClosure on_ui_initialized,
      AuthHubConnector* connector);
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
  friend class TestApi;

  void InitializeViewPlaceholders();

  base::flat_map<AshAuthFactor, raw_ptr<views::View>> views_;
  std::unique_ptr<AuthPanelEventDispatcherFactory> event_dispatcher_factory_;
  std::unique_ptr<FactorAuthViewFactory> view_factory_;
  std::unique_ptr<AuthFactorStoreFactory> store_factory_;

  std::unique_ptr<AuthFactorStore> store_;
  std::unique_ptr<AuthPanelEventDispatcher> event_dispatcher_;

  base::OnceClosure on_end_authentication_;
  base::RepeatingClosure on_ui_changed_;

  raw_ptr<AuthHubConnector> auth_hub_connector_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_AUTH_PANEL_IMPL_AUTH_PANEL_H_
