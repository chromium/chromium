// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_NETWORK_STATE_INFORMER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_NETWORK_STATE_INFORMER_H_

#include <map>
#include <memory>
#include <optional>
#include <string>

#include "base/cancelable_callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/values.h"
#include "chrome/browser/ash/login/screens/network_error.h"
#include "chrome/browser/ui/ash/login/captive_portal_window_proxy.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"

namespace ash {

// Class which observes network state changes and calls registered callbacks.
// State is considered changed if connection or the active network has been
// changed. Also, it answers to the requests about current network state.
class NetworkStateInformer : public NetworkStateHandlerObserver,
                             public base::RefCounted<NetworkStateInformer> {
 public:
  enum State {
    OFFLINE = 0,
    ONLINE,
    CAPTIVE_PORTAL,
    CONNECTING,
    PROXY_AUTH_REQUIRED,
    UNKNOWN
  };
  friend std::ostream& operator<<(std::ostream& stream, const State& state);

  class NetworkStateInformerObserver {
   public:
    NetworkStateInformerObserver() = default;
    virtual ~NetworkStateInformerObserver() = default;

    virtual void UpdateState(NetworkError::ErrorReason reason) = 0;
    virtual void OnNetworkReady() {}
  };

  NetworkStateInformer();

  void Init();

  // Adds observer to be notified when network state has been changed.
  void AddObserver(NetworkStateInformerObserver* observer);

  // Removes observer.
  void RemoveObserver(NetworkStateInformerObserver* observer);

  // NetworkStateHandlerObserver implementation:
  void DefaultNetworkChanged(const NetworkState* network) override;
  void PortalStateChanged(const NetworkState* network,
                          const NetworkState::PortalState state) override;

  State state() const { return state_; }
  std::string network_path() const { return network_path_; }

  static std::string GetNetworkName(const std::string& service_path);
  static bool IsProxyError(State state, NetworkError::ErrorReason reason);

 private:
  friend class base::RefCounted<NetworkStateInformer>;

  ~NetworkStateInformer() override;

  bool UpdateState(const NetworkState* network);
  bool UpdateProxyConfig(const NetworkState* network);
  void UpdateStateAndNotify(const NetworkState* network);
  void SendStateToObservers(NetworkError::ErrorReason reason);

  State state_;
  std::string network_path_;
  std::optional<base::Value::Dict> proxy_config_;

  base::ObserverList<NetworkStateInformerObserver>::Unchecked observers_;

  base::ScopedObservation<NetworkStateHandler, NetworkStateHandlerObserver>
      network_state_handler_observer_{this};

  base::WeakPtrFactory<NetworkStateInformer> weak_ptr_factory_{this};
};

std::ostream& operator<<(std::ostream& stream,
                         const NetworkStateInformer::State& state);

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_NETWORK_STATE_INFORMER_H_
