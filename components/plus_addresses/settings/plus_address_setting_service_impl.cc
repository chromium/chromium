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
#include "components/sync/base/features.h"
#include "components/sync/base/model_type.h"
#include "components/sync/model/client_tag_based_model_type_processor.h"
#include "components/sync/model/forwarding_model_type_controller_delegate.h"
#include "components/sync/model/model_type_controller_delegate.h"
#include "components/sync/model/model_type_store.h"
#include "components/sync/protocol/plus_address_setting_specifics.pb.h"

namespace plus_addresses {

namespace {

// Setting names - must be in sync with the server.
// TODO(crbug.com/342089839): Agree upon names with server-side team.
constexpr std::string_view kPlusAddressEnabledSetting =
    "plus_address.is_enabled";
constexpr std::string_view kAcceptedNoticeSetting =
    "plus_address.has_accepted_notice";

}  // namespace

PlusAddressSettingServiceImpl::PlusAddressSettingServiceImpl(
    std::unique_ptr<PlusAddressSettingSyncBridge> bridge)
    : sync_bridge_(std::move(bridge)) {}

PlusAddressSettingServiceImpl::~PlusAddressSettingServiceImpl() = default;

bool PlusAddressSettingServiceImpl::GetIsPlusAddressesEnabled() const {
  return GetBoolean(kPlusAddressEnabledSetting);
}

bool PlusAddressSettingServiceImpl::GetHasAcceptedNotice() const {
  return GetBoolean(kAcceptedNoticeSetting);
}

void PlusAddressSettingServiceImpl::SetHasAcceptedNotice() {
  if (base::FeatureList::IsEnabled(syncer::kSyncPlusAddressSetting)) {
    sync_bridge_->WriteSetting(
        CreateSettingSpecifics(kAcceptedNoticeSetting, true));
  }
}

std::unique_ptr<syncer::ModelTypeControllerDelegate>
PlusAddressSettingServiceImpl::GetSyncControllerDelegate() {
  CHECK(base::FeatureList::IsEnabled(syncer::kSyncPlusAddressSetting));
  return std::make_unique<syncer::ForwardingModelTypeControllerDelegate>(
      sync_bridge_->change_processor()->GetControllerDelegate().get());
}

bool PlusAddressSettingServiceImpl::GetBoolean(std::string_view name) const {
  if (!base::FeatureList::IsEnabled(syncer::kSyncPlusAddressSetting)) {
    return false;
  }
  auto setting = sync_bridge_->GetSetting(name);
  DCHECK(!setting || setting->has_bool_value());
  if (!setting || !setting->has_bool_value()) {
    return false;
  }
  return setting->bool_value();
}

}  // namespace plus_addresses
