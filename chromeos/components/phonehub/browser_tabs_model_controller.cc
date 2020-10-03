// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/browser_tabs_model_controller.h"

namespace chromeos {
namespace phonehub {

BrowserTabsModelController::BrowserTabsModelController(
    BrowserTabsModelProvider* browser_tabs_model_provider,
    MutablePhoneModel* mutable_phone_model)
    : browser_tabs_model_provider_(browser_tabs_model_provider),
      mutable_phone_model_(mutable_phone_model) {
  browser_tabs_model_provider_->AddObserver(this);
}

BrowserTabsModelController::~BrowserTabsModelController() {
  browser_tabs_model_provider_->RemoveObserver(this);
}

void BrowserTabsModelController::OnBrowserTabsUpdated(
    bool is_sync_enabled,
    const std::vector<BrowserTabsModel::BrowserTabMetadata>&
        browser_tabs_metadata) {
  mutable_phone_model_->SetBrowserTabsModel(BrowserTabsModel(
      /*is_tab_sync_enabled=*/is_sync_enabled, browser_tabs_metadata));
}

}  // namespace phonehub
}  // namespace chromeos
