// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_IOS_BROWSER_WAIT_FOR_NETWORK_CALLBACK_HELPER_IOS_H_
#define COMPONENTS_SIGNIN_IOS_BROWSER_WAIT_FOR_NETWORK_CALLBACK_HELPER_IOS_H_

#include <vector>

#include "base/functional/callback.h"
#include "components/signin/public/base/wait_for_network_callback_helper.h"
#include "net/base/network_change_notifier.h"

// Class used for delaying callbacks when the network connection is offline and
// invoking them when the network connection becomes online.
class WaitForNetworkCallbackHelperIOS
    : public net::NetworkChangeNotifier::NetworkChangeObserver,
      public WaitForNetworkCallbackHelper {
 public:
  WaitForNetworkCallbackHelperIOS();

  WaitForNetworkCallbackHelperIOS(const WaitForNetworkCallbackHelperIOS&) =
      delete;
  WaitForNetworkCallbackHelperIOS& operator=(
      const WaitForNetworkCallbackHelperIOS&) = delete;

  ~WaitForNetworkCallbackHelperIOS() override;

  // net::NetworkChangeController::NetworkChangeObserver implementation.
  void OnNetworkChanged(
      net::NetworkChangeNotifier::ConnectionType type) override;

  // WaitForNetworkCallbackHelper::
  // Is network offline?
  bool AreNetworkCallsDelayed() override;
  // If `AreNetworkCallsDelayed()`, saves the `callback` to be called later when
  // online. Otherwise, invokes immediately.
  void DelayNetworkCall(base::OnceClosure callback) override;

 private:
  std::vector<base::OnceClosure> delayed_callbacks_;
};

#endif  // COMPONENTS_SIGNIN_IOS_BROWSER_WAIT_FOR_NETWORK_CALLBACK_HELPER_IOS_H_
