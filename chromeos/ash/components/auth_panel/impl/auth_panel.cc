// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/auth_panel/impl/auth_panel.h"

#include <memory>
#include <vector>

#include "ash/auth/views/auth_textfield.h"
#include "ash/login/ui/arrow_button_view.h"
#include "ash/login/ui/non_accessible_view.h"
#include "ash/public/cpp/ime_controller.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "chromeos/ash/components/auth_panel/impl/auth_factor_store.h"
#include "chromeos/ash/components/auth_panel/impl/auth_panel_event_dispatcher.h"
#include "chromeos/ash/components/auth_panel/impl/factor_auth_view.h"
#include "chromeos/ash/components/auth_panel/impl/factor_auth_view_factory.h"
#include "chromeos/ash/components/auth_panel/impl/views/password_auth_view.h"
#include "chromeos/ash/components/osauth/public/auth_factor_status_consumer.h"
#include "chromeos/ash/components/osauth/public/common_types.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_types.h"

namespace ash {

namespace {

std::optional<AshAuthFactor> GetPasswordFactorType(AuthFactorsSet factors) {
  bool has_local_password = factors.Has(AshAuthFactor::kLocalPassword);
  bool has_gaia_password = factors.Has(AshAuthFactor::kGaiaPassword);

  // We currently only support having either local passwords or gaia passwords,
  // but not both.
  CHECK(!has_local_password || !has_gaia_password);

  if (has_local_password) {
    return AshAuthFactor::kLocalPassword;
  } else if (has_gaia_password) {
    return AshAuthFactor::kGaiaPassword;
  } else {
    return std::nullopt;
  }
}

}  // namespace

AuthPanel::TestApi::TestApi(AuthPanel* auth_panel) : auth_panel_(auth_panel) {}

AuthPanel::TestApi::~TestApi() = default;

PasswordAuthView* AuthPanel::TestApi::GetPasswordAuthView() {
  auto password_view_wrapper =
      auth_panel_->views_[AshAuthFactor::kLocalPassword]
          ? auth_panel_->views_[AshAuthFactor::kLocalPassword]
          : auth_panel_->views_[AshAuthFactor::kGaiaPassword];

  auto children = password_view_wrapper->GetChildrenInZOrder();

  // Each wrapper should only contain a single FactorAuthView.
  // Verify this invariant here.
  CHECK(children.size() == 1);

  return static_cast<PasswordAuthView*>(children.front());
}

void AuthPanel::TestApi::SetSubmitPasswordCallback(
    auth_panel::SubmitPasswordCallback callback) {
  auth_panel_->store_->submit_password_callback_ = std::move(callback);
}

AuthPanel::AuthPanel(
    std::unique_ptr<FactorAuthViewFactory> view_factory,
    std::unique_ptr<AuthFactorStoreFactory> store_factory,
    std::unique_ptr<AuthPanelEventDispatcherFactory> event_dispatcher_factory,
    base::OnceClosure on_end_authentication,
    base::RepeatingClosure on_prefered_size_changed,
    AuthHubConnector* connector)
    : event_dispatcher_factory_(std::move(event_dispatcher_factory)),
      view_factory_(std::move(view_factory)),
      store_factory_(std::move(store_factory)),
      on_end_authentication_(std::move(on_end_authentication)),
      on_ui_changed_(std::move(on_prefered_size_changed)),
      auth_hub_connector_(connector) {
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
      .SetCollapseMargins(false);

  InitializeViewPlaceholders();
}

AuthPanel::~AuthPanel() = default;

void AuthPanel::InitializeViewPlaceholders() {
  // The order in which the views will be laid out in AuthPanel. We create
  // placeholder spots for them in the UI beforehand, and fill them out later
  // depending on the presence of factors.
  std::vector<AshAuthFactor> view_order{
      AshAuthFactor::kGaiaPassword,
  };

  for (AshAuthFactor factor : view_order) {
    views_[factor] = AddChildView(std::make_unique<NonAccessibleView>());
    views_[factor]->SetLayoutManager(std::make_unique<views::FlexLayout>());
  }
}

void AuthPanel::InitializeUi(AuthFactorsSet factors,
                             AuthHubConnector* connector) {
  std::optional<AshAuthFactor> password_type = GetPasswordFactorType(factors);

  store_ =
      store_factory_->CreateAuthFactorStore(ImeController::Get(), connector,
                                            /*password_type=*/password_type);
  event_dispatcher_ =
      event_dispatcher_factory_->CreateAuthPanelEventDispatcher(store_.get());

  for (auto&& factor : factors) {
    auto factor_auth_view = view_factory_->CreateFactorAuthView(
        factor, store_.get(), event_dispatcher_.get());
    if (factor_auth_view != nullptr) {
      // We ignore views that the factory doesn't know how to construct.
      views_[factor]->AddChildView(std::move(factor_auth_view));
    }
    event_dispatcher_->DispatchEvent(factor,
                                     AuthFactorState::kCheckingForPresence);
  }

  on_ui_changed_.Run();
}

void AuthPanel::OnFactorListChanged(FactorsStatusMap factors_with_status) {
  // Here, the list of auth factors can potentially change. Previously
  // available auth factors (at the time of `AuthPanel::InitializeUI`) can now
  // be unavailable. The converse is also true.
  // Existing auth factors can have their status changed.
  // To that end, we destroy the UI and recreate it, to ensure consistency.
  // It is also assumed that the password type will not change from
  // `kGaiaPassword` to `kLocalPassword` or vice-versa. Therefore we don't have
  // to inform `store_` of a new password type.

  // Order of operations is important, removing child views before clearing
  // our non-owning references will cause them to dangle.
  views_.clear();
  RemoveAllChildViews();

  InitializeViewPlaceholders();

  for (const auto& [factor, status] : factors_with_status) {
    auto factor_auth_view = view_factory_->CreateFactorAuthView(
        factor, store_.get(), event_dispatcher_.get());
    if (factor_auth_view != nullptr) {
      // We ignore views that the factory doesn't know how to construct.
      views_[factor]->AddChildView(std::move(factor_auth_view));
    }
    event_dispatcher_->DispatchEvent(factor, status);
  }

  on_ui_changed_.Run();
}

void AuthPanel::OnFactorStatusesChanged(FactorsStatusMap incremental_update) {
  for (const auto& [factor, status] : incremental_update) {
    event_dispatcher_->DispatchEvent(factor, status);
  }
}

void AuthPanel::OnFactorCustomSignal(AshAuthFactor factor) {
  NOTIMPLEMENTED();
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
  auth_hub_connector_ = nullptr;
  std::move(on_end_authentication_).Run();
}

BEGIN_METADATA(AuthPanel)
END_METADATA

}  // namespace ash
