// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/invalidations/interested_data_types_manager.h"

#include <utility>

#include "base/feature_list.h"
#include "components/sync/base/features.h"
#include "components/sync/base/model_type.h"
#include "components/sync/invalidations/interested_data_types_handler.h"

namespace syncer {

InterestedDataTypesManager::InterestedDataTypesManager() = default;

InterestedDataTypesManager::~InterestedDataTypesManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!interested_data_types_handler_);
}

void InterestedDataTypesManager::SetInterestedDataTypesHandler(
    InterestedDataTypesHandler* handler) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!interested_data_types_handler_ || !handler);
  interested_data_types_handler_ = handler;
}

absl::optional<ModelTypeSet>
InterestedDataTypesManager::GetInterestedDataTypes() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return data_types_;
}

void InterestedDataTypesManager::SetInterestedDataTypes(
    const ModelTypeSet& data_types) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(interested_data_types_handler_);

  data_types_ = data_types;
  interested_data_types_handler_->OnInterestedDataTypesChanged();
}

void InterestedDataTypesManager::
    SetCommittedAdditionalInterestedDataTypesCallback(
        SyncInvalidationsService::InterestedDataTypesAppliedCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(interested_data_types_handler_);

  // Do not send an additional GetUpdates request when invalidations are
  // disabled.
  if (base::FeatureList::IsEnabled(kUseSyncInvalidations)) {
    interested_data_types_handler_
        ->SetCommittedAdditionalInterestedDataTypesCallback(
            std::move(callback));
  }
}

}  // namespace syncer
