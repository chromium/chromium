// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/account_settings/account_setting_service.h"

#include <memory>

#include "base/check.h"
#include "base/command_line.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
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

// TODO(crbug.com/441735283): Remove once the rollout starts.
// Overrides the return value of
// AccountSettingService::IsWalletPrivacyContextualSurfacingEnabled() to true.
constexpr char kEnableAutofillWalletPrivacyContextualSurfacingForTesting[] =
    "enable-autofill-wallet-privacy-contextual-surfacing";
}

AccountSettingService::AccountSettingService(
    std::unique_ptr<AccountSettingSyncBridge> sync_bridge)
    : sync_bridge_(std::move(sync_bridge)) {
  // TODO(crbug.com/436547684): Remove this check once `sync_bridge` is
  // guaranteed not to be null. This will happen when the bridge construction
  // isn't gated by the sync feature flag anymore.
  if (sync_bridge_) {
    scoped_observation_.Observe(sync_bridge_.get());
  }
}

AccountSettingService::~AccountSettingService() = default;

bool AccountSettingService::IsWalletPrivacyContextualSurfacingEnabled() const {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          kEnableAutofillWalletPrivacyContextualSurfacingForTesting)) {
    return true;
  }
  if (!base::FeatureList::IsEnabled(syncer::kSyncAccountSettings)) {
    return false;
  }
  std::optional<bool> setting =
      sync_bridge_->GetBoolSetting(kWalletPrivacyContextualSurfacingSetting);
  return setting.has_value() && *setting;
}

std::unique_ptr<syncer::DataTypeControllerDelegate>
AccountSettingService::GetSyncControllerDelegate() {
  CHECK(base::FeatureList::IsEnabled(syncer::kSyncAccountSettings));
  return std::make_unique<syncer::ForwardingDataTypeControllerDelegate>(
      sync_bridge_->change_processor()->GetControllerDelegate().get());
}

void AccountSettingService::OnDataLoadedFromDisk() {
  base::UmaHistogramBoolean("Autofill.Ai.WalletContextualSurfacingEnabled",
                            IsWalletPrivacyContextualSurfacingEnabled());
}

}  // namespace autofill
