// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_CONNECTION_NOTIFIER_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_CONNECTION_NOTIFIER_H_

#include "base/functional/callback.h"
#include "net/base/network_change_notifier.h"

namespace offline_pages {

class ConnectionNotifier
    : public net::NetworkChangeNotifier::NetworkChangeObserver {
 public:
  // Callback to call when we become connected.
  typedef base::OnceCallback<void()> ConnectedCallback;

  ConnectionNotifier(ConnectionNotifier::ConnectedCallback callback);

  ConnectionNotifier(const ConnectionNotifier&) = delete;
  ConnectionNotifier& operator=(const ConnectionNotifier&) = delete;

  ~ConnectionNotifier() override;

  // net::NetworkChangeNotifier::NetworkChangeObserver implementation.
  void OnNetworkChanged(
      net::NetworkChangeNotifier::ConnectionType type) override;

 private:
  base::OnceCallback<void()> callback_;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_CONNECTION_NOTIFIER_H_
