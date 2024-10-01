// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/settings/plus_address_setting_service_impl.h"

#include <memory>
#include <string_view>

#include "base/check.h"
#include "base/functional/callback_helpers.h"
#include "components/plus_addresses/settings/plus_address_setting_sync_bridge.h"
#include "components/plus_addresses/settings/plus_address_setting_sync_util.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/features.h"
#include "components/sync/model/client_tag_based_data_type_processor.h"
#include "components/sync/model/data_type_controller_delegate.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/model/forwarding_data_type_controller_delegate.h"
#include "components/sync/protocol/plus_address_setting_specifics.pb.h"

namespace plus_addresses {

namespace {

// Setting names - must be in sync with the server.
constexpr std::string_view kPlusAddressEnabledSetting = "has_feature_enabled";
constexpr std::string_view kAcceptedNoticeSetting = "has_accepted_notice";

}  // namespace

PlusAddressSettingServiceImpl::PlusAddressSettingServiceImpl(
    std::unique_ptr<PlusAddressSettingSyncBridge> bridge)
    : sync_bridge_(std::move(bridge)) {}

PlusAddressSettingServiceImpl::~PlusAddressSettingServiceImpl() = default;

bool PlusAddressSettingServiceImpl::GetIsPlusAddressesEnabled() const {
  return GetBoolean(kPlusAddressEnabledSetting, /*default_value=*/true);
}

bool PlusAddressSettingServiceImpl::GetHasAcceptedNotice() const {
  return GetBoolean(kAcceptedNoticeSetting, /*default_value=*/false);
}

void PlusAddressSettingServiceImpl::SetHasAcceptedNotice() {
  if (base::FeatureList::IsEnabled(syncer::kSyncPlusAddressSetting)) {
    sync_bridge_->WriteSetting(
        CreateSettingSpecifics(kAcceptedNoticeSetting, true));
  }
}

std::unique_ptr<syncer::DataTypeControllerDelegate>
PlusAddressSettingServiceImpl::GetSyncControllerDelegate() {
  CHECK(base::FeatureList::IsEnabled(syncer::kSyncPlusAddressSetting));
  return std::make_unique<syncer::ForwardingDataTypeControllerDelegate>(
      sync_bridge_->change_processor()->GetControllerDelegate().get());
}

bool PlusAddressSettingServiceImpl::GetBoolean(std::string_view name,
                                               bool default_value) const {
  if (!base::FeatureList::IsEnabled(syncer::kSyncPlusAddressSetting)) {
    return false;
  }
  auto setting = sync_bridge_->GetSetting(name);
  DCHECK(!setting || setting->has_bool_value());
  if (!setting || !setting->has_bool_value()) {
    return default_value;
  }
  return setting->bool_value();
}

}  // namespace plus_addresses
