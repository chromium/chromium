// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/ios/browser/wait_for_network_callback_helper_ios.h"

#include <utility>

WaitForNetworkCallbackHelperIOS::WaitForNetworkCallbackHelperIOS() {
  net::NetworkChangeNotifier::AddNetworkChangeObserver(this);
}

WaitForNetworkCallbackHelperIOS::~WaitForNetworkCallbackHelperIOS() {
  net::NetworkChangeNotifier::RemoveNetworkChangeObserver(this);
}

void WaitForNetworkCallbackHelperIOS::OnNetworkChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  if (net::NetworkChangeNotifier::IsOffline()) {
    return;
  }

  std::vector<base::OnceClosure> callbacks;
  delayed_callbacks_.swap(callbacks);
  for (base::OnceClosure& callback : callbacks) {
    std::move(callback).Run();
  }
}

bool WaitForNetworkCallbackHelperIOS::AreNetworkCallsDelayed() {
  return net::NetworkChangeNotifier::IsOffline();
}

void WaitForNetworkCallbackHelperIOS::DelayNetworkCall(
    base::OnceClosure callback) {
  if (AreNetworkCallsDelayed()) {
    // Will be processed by `OnNetworkChanged()`.
    delayed_callbacks_.push_back(std::move(callback));
    return;
  }

  std::move(callback).Run();
}
