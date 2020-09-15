// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/invalidations/interested_data_types_manager.h"

#include <utility>

#include "base/bind.h"
#include "components/sync/base/model_type.h"
#include "components/sync/invalidations/interested_data_types_handler.h"

namespace syncer {

InterestedDataTypesManager::InterestedDataTypesManager() = default;

InterestedDataTypesManager::~InterestedDataTypesManager() {
  DCHECK(!interested_data_types_handler_);
}

void InterestedDataTypesManager::SetInterestedDataTypesHandler(
    InterestedDataTypesHandler* handler) {
  DCHECK(!interested_data_types_handler_ || !handler);
  interested_data_types_handler_ = handler;
}

const ModelTypeSet& InterestedDataTypesManager::GetInterestedDataTypes() const {
  return data_types_;
}

void InterestedDataTypesManager::SetInterestedDataTypes(
    const ModelTypeSet& data_types,
    SyncInvalidationsService::InterestedDataTypesAppliedCallback callback) {
  ModelTypeSet new_data_types = Difference(data_types, data_types_);
  data_types_ = data_types;
  if (interested_data_types_handler_) {
    interested_data_types_handler_->OnInterestedDataTypesChanged(
        base::BindOnce(std::move(callback), new_data_types));
  }
}

}  // namespace syncer
