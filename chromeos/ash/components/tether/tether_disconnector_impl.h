// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TETHER_TETHER_DISCONNECTOR_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_TETHER_TETHER_DISCONNECTOR_IMPL_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/tether/tether_disconnector.h"
#include "chromeos/ash/components/tether/tether_session_completion_logger.h"

namespace ash {

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

  TetherDisconnectorImpl(const TetherDisconnectorImpl&) = delete;
  TetherDisconnectorImpl& operator=(const TetherDisconnectorImpl&) = delete;

  ~TetherDisconnectorImpl() override;

  void DisconnectFromNetwork(
      const std::string& tether_network_guid,
      base::OnceClosure success_callback,
      StringErrorCallback error_callback,
      const TetherSessionCompletionLogger::SessionCompletionReason&
          session_completion_reason) override;

 private:
  friend class TetherDisconnectorImplTest;

  void DisconnectActiveWifiConnection(const std::string& tether_network_guid,
                                      const std::string& wifi_network_guid,
                                      base::OnceClosure success_callback,
                                      StringErrorCallback error_callback);

  raw_ptr<ActiveHost> active_host_;
  raw_ptr<WifiHotspotDisconnector> wifi_hotspot_disconnector_;
  raw_ptr<DisconnectTetheringRequestSender>
      disconnect_tethering_request_sender_;
  raw_ptr<TetherConnector> tether_connector_;
  raw_ptr<DeviceIdTetherNetworkGuidMap> device_id_tether_network_guid_map_;
  raw_ptr<TetherSessionCompletionLogger> tether_session_completion_logger_;

  base::WeakPtrFactory<TetherDisconnectorImpl> weak_ptr_factory_{this};
};

}  // namespace tether

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_TETHER_TETHER_DISCONNECTOR_IMPL_H_
