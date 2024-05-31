// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/settings/plus_address_setting_service.h"

#include <memory>

#include "base/check.h"
#include "base/functional/callback_helpers.h"
#include "components/plus_addresses/settings/plus_address_setting_sync_bridge.h"
#include "components/sync/base/features.h"
#include "components/sync/base/model_type.h"
#include "components/sync/model/client_tag_based_model_type_processor.h"
#include "components/sync/model/forwarding_model_type_controller_delegate.h"
#include "components/sync/model/model_type_controller_delegate.h"
#include "components/sync/model/model_type_store.h"
#include "components/sync/protocol/plus_address_setting_specifics.pb.h"

namespace plus_addresses {

PlusAddressSettingService::PlusAddressSettingService(
    syncer::OnceModelTypeStoreFactory store_factory) {
  if (base::FeatureList::IsEnabled(syncer::kSyncPlusAddressSetting)) {
    sync_bridge_ = std::make_unique<PlusAddressSettingSyncBridge>(
        std::make_unique<syncer::ClientTagBasedModelTypeProcessor>(
            syncer::PLUS_ADDRESS_SETTING,
            /*dump_stack=*/base::DoNothing()),
        std::move(store_factory));
  }
}

PlusAddressSettingService::~PlusAddressSettingService() = default;

bool PlusAddressSettingService::GetIsPlusAddressesEnabled() const {
  // TODO(b/342089839): Finalize setting name.
  return GetBoolean("plus_address.is_enabled");
}

bool PlusAddressSettingService::GetHasAcceptedNotice() const {
  // TODO(b/342089839): Finalize setting name.
  return GetBoolean("plus_address.has_accepted_notice");
}

bool PlusAddressSettingService::GetIsOptedInToDogfood() const {
  // TODO(b/342089839): Finalize setting name.
  return GetBoolean("plus_address.is_opted_in_to_dogfood");
}

std::unique_ptr<syncer::ModelTypeControllerDelegate>
PlusAddressSettingService::GetSyncControllerDelegate() {
  CHECK(base::FeatureList::IsEnabled(syncer::kSyncPlusAddressSetting));
  return std::make_unique<syncer::ForwardingModelTypeControllerDelegate>(
      sync_bridge_->change_processor()->GetControllerDelegate().get());
}

bool PlusAddressSettingService::GetBoolean(std::string_view name) const {
  if (auto setting = sync_bridge_->GetSetting(name)) {
    CHECK(setting->has_bool_value());
    return setting->bool_value();
  }
  return false;
}

}  // namespace plus_addresses
