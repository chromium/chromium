// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/browser_tabs_model_controller.h"

namespace ash {
namespace phonehub {

namespace {

using multidevice_setup::mojom::Feature;
using multidevice_setup::mojom::FeatureState;

}  // namespace

BrowserTabsModelController::BrowserTabsModelController(
    multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
    BrowserTabsModelProvider* browser_tabs_model_provider,
    MutablePhoneModel* mutable_phone_model)
    : multidevice_setup_client_(multidevice_setup_client),
      cached_model_(/*is_tab_sync_enabled=*/false),
      browser_tabs_model_provider_(browser_tabs_model_provider),
      mutable_phone_model_(mutable_phone_model) {
  multidevice_setup_client_->AddObserver(this);
  browser_tabs_model_provider_->AddObserver(this);
}

BrowserTabsModelController::~BrowserTabsModelController() {
  multidevice_setup_client_->RemoveObserver(this);
  browser_tabs_model_provider_->RemoveObserver(this);
}

void BrowserTabsModelController::OnBrowserTabsUpdated(
    bool is_sync_enabled,
    const std::vector<BrowserTabsModel::BrowserTabMetadata>&
        browser_tabs_metadata) {
  cached_model_ = BrowserTabsModel(is_sync_enabled, browser_tabs_metadata);
  UpdateBrowserTabsModel();
}

void BrowserTabsModelController::OnFeatureStatesChanged(
    const multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap&
        feature_states_map) {
  UpdateBrowserTabsModel();
}

void BrowserTabsModelController::UpdateBrowserTabsModel() {
  FeatureState feature_state = multidevice_setup_client_->GetFeatureState(
      Feature::kPhoneHubTaskContinuation);
  if (feature_state == FeatureState::kEnabledByUser)
    mutable_phone_model_->SetBrowserTabsModel(cached_model_);
  else
    mutable_phone_model_->SetBrowserTabsModel(std::nullopt);
}

}  // namespace phonehub
}  // namespace ash
