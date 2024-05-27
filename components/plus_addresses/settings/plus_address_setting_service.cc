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

std::unique_ptr<syncer::ModelTypeControllerDelegate>
PlusAddressSettingService::GetSyncControllerDelegate() {
  CHECK(base::FeatureList::IsEnabled(syncer::kSyncPlusAddressSetting));
  return std::make_unique<syncer::ForwardingModelTypeControllerDelegate>(
      sync_bridge_->change_processor()->GetControllerDelegate().get());
}

}  // namespace plus_addresses
