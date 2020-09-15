// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_INVALIDATIONS_INTERESTED_DATA_TYPES_HANDLER_H_
#define COMPONENTS_SYNC_INVALIDATIONS_INTERESTED_DATA_TYPES_HANDLER_H_

#include "base/callback.h"

namespace syncer {

// An interface to handle changes on data types for which the device wants to
// receive invalidations. Implementations are expected to call the provided
// callback when the list of data types is sent to the Sync server.
class InterestedDataTypesHandler {
 public:
  // Called on each change of interested data types.
  virtual void OnInterestedDataTypesChanged(base::OnceClosure callback) = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_INVALIDATIONS_INTERESTED_DATA_TYPES_HANDLER_H_
