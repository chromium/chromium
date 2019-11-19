// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_TETHER_DISCONNECTOR_IMPL_H_
#define CHROMEOS_COMPONENTS_TETHER_TETHER_DISCONNECTOR_IMPL_H_

#include <memory>
#include <string>
#include <unordered_map>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/components/tether/tether_disconnector.h"
#include "chromeos/components/tether/tether_session_completion_logger.h"
#include "chromeos/network/network_handler_callbacks.h"

namespace chromeos {

namespace tether {

class ActiveHost;
class DeviceIdTetherNetworkGuidMap;
class DisconnectTetheringRequestSender;
class TetherConnector;
class WifiHotspotDisconnector;

class TetherDisconnectorImpl : public TetherDisconnector {
 public:
  TetherDisconnectorImpl(
      ActiveHost* active_host,
      WifiHotspotDisconnector* wifi_hotspot_disconnector,
      DisconnectTetheringRequestSender* disconnect_tethering_request_sender,
      TetherConnector* tether_connector,
      DeviceIdTetherNetworkGuidMap* device_id_tether_network_guid_map,
      TetherSessionCompletionLogger* tether_session_completion_logger);
  ~TetherDisconnectorImpl() override;

  void DisconnectFromNetwork(
      const std::string& tether_network_guid,
      const base::Closure& success_callback,
      const network_handler::StringResultCallback& error_callback,
      const TetherSessionCompletionLogger::SessionCompletionReason&
          session_completion_reason) override;

 private:
  friend class TetherDisconnectorImplTest;

  void DisconnectActiveWifiConnection(
      const std::string& tether_network_guid,
      const std::string& wifi_network_guid,
      const base::Closure& success_callback,
      const network_handler::StringResultCallback& error_callback);

  ActiveHost* active_host_;
  WifiHotspotDisconnector* wifi_hotspot_disconnector_;
  DisconnectTetheringRequestSender* disconnect_tethering_request_sender_;
  TetherConnector* tether_connector_;
  DeviceIdTetherNetworkGuidMap* device_id_tether_network_guid_map_;
  TetherSessionCompletionLogger* tether_session_completion_logger_;

  base::WeakPtrFactory<TetherDisconnectorImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(TetherDisconnectorImpl);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_TETHER_DISCONNECTOR_IMPL_H_
