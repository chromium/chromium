// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/settings/plus_address_setting_service_impl.h"

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

PlusAddressSettingServiceImpl::PlusAddressSettingServiceImpl(
    syncer::OnceModelTypeStoreFactory store_factory) {
  if (base::FeatureList::IsEnabled(syncer::kSyncPlusAddressSetting)) {
    sync_bridge_ = std::make_unique<PlusAddressSettingSyncBridge>(
        std::make_unique<syncer::ClientTagBasedModelTypeProcessor>(
            syncer::PLUS_ADDRESS_SETTING,
            /*dump_stack=*/base::DoNothing()),
        std::move(store_factory));
  }
}

PlusAddressSettingServiceImpl::~PlusAddressSettingServiceImpl() = default;

bool PlusAddressSettingServiceImpl::GetIsPlusAddressesEnabled() const {
  // TODO(crbug.com/342089839): Finalize setting name.
  return GetBoolean("plus_address.is_enabled");
}

bool PlusAddressSettingServiceImpl::GetHasAcceptedNotice() const {
  // TODO(crbug.com/342089839): Finalize setting name.
  return GetBoolean("plus_address.has_accepted_notice");
}

bool PlusAddressSettingServiceImpl::GetIsOptedInToDogfood() const {
  // TODO(crbug.com/342089839): Finalize setting name.
  return GetBoolean("plus_address.is_opted_in_to_dogfood");
}

void PlusAddressSettingServiceImpl::SetHasAcceptedNotice() {
  // TODO(crbug.com/342089839): Implement.
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
  if (auto setting = sync_bridge_->GetSetting(name)) {
    CHECK(setting->has_bool_value());
    return setting->bool_value();
  }
  return false;
}

}  // namespace plus_addresses
