// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/account_settings/account_setting_service_impl.h"

#include <memory>

#include "base/check.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "components/account_settings/account_setting_sync_bridge.h"
#include "components/account_settings/account_settings.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/features.h"
#include "components/sync/model/client_tag_based_data_type_processor.h"
#include "components/sync/model/data_type_controller_delegate.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/model/forwarding_data_type_controller_delegate.h"

namespace account_settings {
namespace {
bool CheckFeatureRequirements(const AccountSetting& setting) {
  if (!base::FeatureList::IsEnabled(syncer::kSyncAccountSettings)) {
    return false;
  }
  if (!setting.feature) {
    return true;
  }
  return base::FeatureList::IsEnabled(*setting.feature);
}
}  // namespace

AccountSettingServiceImpl::AccountSettingServiceImpl(
    std::unique_ptr<AccountSettingSyncBridge> sync_bridge)
    : sync_bridge_(std::move(sync_bridge)) {
  // TODO(crbug.com/436547684): Remove this check once `sync_bridge` is
  // guaranteed not to be null. This will happen when the bridge construction
  // isn't gated by the sync feature flag anymore.
  if (sync_bridge_) {
    scoped_observation_.Observe(sync_bridge_.get());
  }
}

AccountSettingServiceImpl::~AccountSettingServiceImpl() = default;

void AccountSettingServiceImpl::AddObserver(
    AccountSettingService::Observer* observer) {
  observers_.AddObserver(observer);
}

void AccountSettingServiceImpl::RemoveObserver(
    AccountSettingService::Observer* observer) {
  observers_.RemoveObserver(observer);
}

std::optional<bool> AccountSettingServiceImpl::GetBoolean(
    const AccountSetting& setting) const {
  CHECK(setting.type == base::Value::Type::BOOLEAN);
  if (!CheckFeatureRequirements(setting)) {
    return std::nullopt;
  }
  return sync_bridge_->GetBooleanSetting(setting.name);
}

std::optional<int> AccountSettingServiceImpl::GetInteger(
    const AccountSetting& setting) const {
  CHECK(setting.type == base::Value::Type::INTEGER);
  if (!CheckFeatureRequirements(setting)) {
    return std::nullopt;
  }
  return sync_bridge_->GetIntSetting(setting.name);
}

std::optional<std::string> AccountSettingServiceImpl::GetString(
    const AccountSetting& setting) const {
  CHECK(setting.type == base::Value::Type::STRING);
  if (!CheckFeatureRequirements(setting)) {
    return std::nullopt;
  }
  return sync_bridge_->GetStringSetting(setting.name);
}

std::unique_ptr<syncer::DataTypeControllerDelegate>
AccountSettingServiceImpl::GetSyncControllerDelegate() {
  CHECK(base::FeatureList::IsEnabled(syncer::kSyncAccountSettings));
  return std::make_unique<syncer::ForwardingDataTypeControllerDelegate>(
      sync_bridge_->change_processor()->GetControllerDelegate().get());
}

void AccountSettingServiceImpl::OnDataLoadedFromDisk() {
  base::UmaHistogramBoolean(
      "Autofill.Ai.WalletContextualSurfacingEnabled",
      GetBoolean(kWalletPrivacyContextualSurfacing).value_or(false));
}

void AccountSettingServiceImpl::OnDataUpdated(const std::string& setting_name) {
  observers_.Notify(
      &AccountSettingService::Observer::OnAccountSettingDataUpdated,
      setting_name);
}

}  // namespace account_settings
