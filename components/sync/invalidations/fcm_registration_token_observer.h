// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_INVALIDATIONS_FCM_REGISTRATION_TOKEN_OBSERVER_H_
#define COMPONENTS_SYNC_INVALIDATIONS_FCM_REGISTRATION_TOKEN_OBSERVER_H_

#include "base/observer_list_types.h"

namespace syncer {

// An interface to observe changes on FCM registration token.
class FCMRegistrationTokenObserver : public base::CheckedObserver {
 public:
  // Called on each change of FCM registration token.
  virtual void OnFCMRegistrationTokenChanged() = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_INVALIDATIONS_FCM_REGISTRATION_TOKEN_OBSERVER_H_
