// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_ITEMS_COLLECTION_CORE_UPDATE_DELTA_H_
#define COMPONENTS_OFFLINE_ITEMS_COLLECTION_CORE_UPDATE_DELTA_H_

#include "base/optional.h"

namespace offline_items_collection {

// This struct holds any important information that might have been changed
// since last update to the observers. The observers should use this information
// to make a decision on whether to act on the new info contained in the offline
// item.
struct UpdateDelta {
  static base::Optional<UpdateDelta> MergeUpdates(
      const base::Optional<UpdateDelta>& update1,
      const base::Optional<UpdateDelta>& update2);

  UpdateDelta();
  UpdateDelta(const UpdateDelta& other);

  ~UpdateDelta();

  // Whether the state of the offline item has been changed. The new
  // state can be found from current offline item update.
  bool state_changed;

  // Whether the visuals have been changed since last update. The observers
  // should query for the visuals again.
  bool visuals_changed;
};

}  // namespace offline_items_collection

#endif  // COMPONENTS_OFFLINE_ITEMS_COLLECTION_CORE_UPDATE_DELTA_H_
