// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_INVALIDATIONS_INTERESTED_DATA_TYPES_MANAGER_H_
#define COMPONENTS_SYNC_INVALIDATIONS_INTERESTED_DATA_TYPES_MANAGER_H_

#include "base/observer_list.h"
#include "components/sync/base/model_type.h"

namespace syncer {
class InterestedDataTypesObserver;

// Manages for which data types are invalidations sent to this device.
class InterestedDataTypesManager {
 public:
  InterestedDataTypesManager();
  ~InterestedDataTypesManager();
  InterestedDataTypesManager(const InterestedDataTypesManager&) = delete;
  InterestedDataTypesManager& operator=(const InterestedDataTypesManager&) =
      delete;

  // Add or remove a interested data types change observer. |observer| must not
  // be nullptr.
  void AddInterestedDataTypesObserver(InterestedDataTypesObserver* observer);
  void RemoveInterestedDataTypesObserver(InterestedDataTypesObserver* observer);

  // Get or set the interested data types.
  const ModelTypeSet& GetInterestedDataTypes() const;
  void SetInterestedDataTypes(const ModelTypeSet& data_types);

 private:
  base::ObserverList<InterestedDataTypesObserver,
                     /*check_empty=*/true,
                     /*allow_reentrancy=*/false>
      observers_;

  ModelTypeSet data_types_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_INVALIDATIONS_INTERESTED_DATA_TYPES_MANAGER_H_
