// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_INVALIDATIONS_INVALIDATIONS_LISTENER_H_
#define COMPONENTS_SYNC_INVALIDATIONS_INVALIDATIONS_LISTENER_H_

#include <string>

#include "base/observer_list_types.h"

namespace syncer {

// This class provides an interface to handle received invalidations.
class InvalidationsListener : public base::CheckedObserver {
 public:
  // Called on each invalidation. |payload| is passed as is without any parsing.
  virtual void OnInvalidationReceived(const std::string& payload) = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_INVALIDATIONS_INVALIDATIONS_LISTENER_H_
