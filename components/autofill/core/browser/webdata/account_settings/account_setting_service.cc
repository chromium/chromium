// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/account_settings/account_setting_service.h"

#include <memory>

#include "base/check.h"
#include "base/functional/callback_helpers.h"
#include "components/autofill/core/browser/webdata/account_settings/account_setting_sync_bridge.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/features.h"
#include "components/sync/model/client_tag_based_data_type_processor.h"
#include "components/sync/model/data_type_controller_delegate.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/model/forwarding_data_type_controller_delegate.h"

namespace autofill {

namespace {
constexpr std::string_view kWalletPrivacyContextualSurfacingSetting =
    "WALLET_PRIVACY_CONTEXTUAL_SURFACING";
}

AccountSettingService::AccountSettingService(
    std::unique_ptr<AccountSettingSyncBridge> sync_bridge)
    : sync_bridge_(std::move(sync_bridge)) {}

AccountSettingService::~AccountSettingService() = default;

bool AccountSettingService::IsWalletPrivacyContextualSurfacingEnabled() const {
  return GetBoolean(kWalletPrivacyContextualSurfacingSetting,
                    /*default_value=*/false);
}

std::unique_ptr<syncer::DataTypeControllerDelegate>
AccountSettingService::GetSyncControllerDelegate() {
  CHECK(base::FeatureList::IsEnabled(syncer::kSyncAccountSettings));
  return std::make_unique<syncer::ForwardingDataTypeControllerDelegate>(
      sync_bridge_->change_processor()->GetControllerDelegate().get());
}

bool AccountSettingService::GetBoolean(std::string_view name,
                                       bool default_value) const {
  if (!base::FeatureList::IsEnabled(syncer::kSyncAccountSettings)) {
    return default_value;
  }
  auto setting = sync_bridge_->GetSetting(name);
  DCHECK(!setting || setting->has_bool_value());
  if (!setting || !setting->has_bool_value()) {
    return default_value;
  }
  return setting->bool_value();
}

}  // namespace autofill
