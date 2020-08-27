// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_INVALIDATIONS_SUBSCRIBED_DATA_TYPES_MANAGER_H_
#define COMPONENTS_SYNC_INVALIDATIONS_SUBSCRIBED_DATA_TYPES_MANAGER_H_

#include "base/observer_list.h"
#include "components/sync/base/model_type.h"

namespace syncer {
class SubscribedDataTypesObserver;

// Manages for which data types are invalidations sent to this device.
class SubscribedDataTypesManager {
 public:
  SubscribedDataTypesManager();
  ~SubscribedDataTypesManager();
  SubscribedDataTypesManager(const SubscribedDataTypesManager&) = delete;
  SubscribedDataTypesManager& operator=(const SubscribedDataTypesManager&) =
      delete;

  // Add or remove a subscribed data types change observer. |observer| must not
  // be nullptr.
  void AddSubscribedDataTypesObserver(SubscribedDataTypesObserver* observer);
  void RemoveSubscribedDataTypesObserver(SubscribedDataTypesObserver* observer);

  // Get or set the subscribed data types.
  const ModelTypeSet& GetSubscribedDataTypes() const;
  void SetSubscribedDataTypes(const ModelTypeSet& data_types);

 private:
  base::ObserverList<SubscribedDataTypesObserver,
                     /*check_empty=*/true,
                     /*allow_reentrancy=*/false>
      observers_;

  ModelTypeSet data_types_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_INVALIDATIONS_SUBSCRIBED_DATA_TYPES_MANAGER_H_
