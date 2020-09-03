// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_INVALIDATIONS_INTERESTED_DATA_TYPES_OBSERVER_H_
#define COMPONENTS_SYNC_INVALIDATIONS_INTERESTED_DATA_TYPES_OBSERVER_H_

#include "base/observer_list.h"

namespace syncer {

// An interface to observe changes on data types for which the device wants to
// receive invalidations.
class InterestedDataTypesObserver : public base::CheckedObserver {
 public:
  // Called on each change of interested data types.
  virtual void OnInterestedDataTypesChanged() = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_INVALIDATIONS_INTERESTED_DATA_TYPES_OBSERVER_H_
