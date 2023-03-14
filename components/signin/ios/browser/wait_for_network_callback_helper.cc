// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/ios/browser/wait_for_network_callback_helper.h"

#include <utility>

WaitForNetworkCallbackHelper::WaitForNetworkCallbackHelper() {
  net::NetworkChangeNotifier::AddNetworkChangeObserver(this);
}

WaitForNetworkCallbackHelper::~WaitForNetworkCallbackHelper() {
  net::NetworkChangeNotifier::RemoveNetworkChangeObserver(this);
}

void WaitForNetworkCallbackHelper::OnNetworkChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  if (net::NetworkChangeNotifier::IsOffline())
    return;

  for (base::OnceClosure& callback : delayed_callbacks_)
    std::move(callback).Run();

  delayed_callbacks_.clear();
}

bool WaitForNetworkCallbackHelper::AreNetworkCallsDelayed() {
  return net::NetworkChangeNotifier::IsOffline();
}

void WaitForNetworkCallbackHelper::HandleCallback(base::OnceClosure callback) {
  if (AreNetworkCallsDelayed()) {
    // Will be processed by `OnNetworkChanged()`.
    delayed_callbacks_.push_back(std::move(callback));
    return;
  }

  std::move(callback).Run();
}
