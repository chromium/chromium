// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/invalidations/interested_data_types_manager.h"

#include "components/sync/invalidations/interested_data_types_observer.h"

namespace syncer {

InterestedDataTypesManager::InterestedDataTypesManager() = default;

InterestedDataTypesManager::~InterestedDataTypesManager() = default;

void InterestedDataTypesManager::AddInterestedDataTypesObserver(
    InterestedDataTypesObserver* observer) {
  observers_.AddObserver(observer);
}

void InterestedDataTypesManager::RemoveInterestedDataTypesObserver(
    InterestedDataTypesObserver* observer) {
  observers_.RemoveObserver(observer);
}

const ModelTypeSet& InterestedDataTypesManager::GetInterestedDataTypes() const {
  return data_types_;
}

void InterestedDataTypesManager::SetInterestedDataTypes(
    const ModelTypeSet& data_types) {
  data_types_ = data_types;
  for (InterestedDataTypesObserver& observer : observers_) {
    observer.OnInterestedDataTypesChanged();
  }
}

}  // namespace syncer
