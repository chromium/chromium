// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/invalidations/subscribed_data_types_manager.h"

#include "components/sync/invalidations/subscribed_data_types_observer.h"

namespace syncer {

SubscribedDataTypesManager::SubscribedDataTypesManager() = default;

SubscribedDataTypesManager::~SubscribedDataTypesManager() = default;

void SubscribedDataTypesManager::AddSubscribedDataTypesObserver(
    SubscribedDataTypesObserver* observer) {
  observers_.AddObserver(observer);
}

void SubscribedDataTypesManager::RemoveSubscribedDataTypesObserver(
    SubscribedDataTypesObserver* observer) {
  observers_.RemoveObserver(observer);
}

const ModelTypeSet& SubscribedDataTypesManager::GetSubscribedDataTypes() const {
  return data_types_;
}

void SubscribedDataTypesManager::SetSubscribedDataTypes(
    const ModelTypeSet& data_types) {
  data_types_ = data_types;
  for (SubscribedDataTypesObserver& observer : observers_) {
    observer.OnSubscribedDataTypesChanged();
  }
}

}  // namespace syncer
