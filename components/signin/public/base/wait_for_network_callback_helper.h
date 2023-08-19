// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_BASE_WAIT_FOR_NETWORK_CALLBACK_HELPER_H_
#define COMPONENTS_SIGNIN_PUBLIC_BASE_WAIT_FOR_NETWORK_CALLBACK_HELPER_H_

#include "base/functional/callback_forward.h"

// Class used for delaying callbacks when the network connection is offline and
// invoking them when the network connection becomes online.
class WaitForNetworkCallbackHelper {
 public:
  virtual ~WaitForNetworkCallbackHelper() = default;

  // Returns `true` if network is offline.
  virtual bool AreNetworkCallsDelayed() = 0;

  // Executes `callback` if and when there is a network connection. Also see
  // `AreNetworkCallsDelayed()`.
  virtual void DelayNetworkCall(base::OnceClosure callback) = 0;

  // If `disable` is true, `AreNetworkCallsDelayed()` will return false and
  // calls to `DelayNetworkCall()` will run the callback immediately.
  // Otherwise, delaying network callbacks is enabled, network calls might be
  // delayed.
  // By default delaying network calls is enabled.
  virtual void DisableNetworkCallsDelayedForTesting(bool disable);
};
#endif  // COMPONENTS_SIGNIN_PUBLIC_BASE_WAIT_FOR_NETWORK_CALLBACK_HELPER_H_
