// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_FAKE_ASYNCHRONOUS_SHUTDOWN_OBJECT_CONTAINER_H_
#define CHROMEOS_COMPONENTS_TETHER_FAKE_ASYNCHRONOUS_SHUTDOWN_OBJECT_CONTAINER_H_

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/macros.h"
#include "chromeos/components/tether/asynchronous_shutdown_object_container.h"

namespace chromeos {

namespace tether {

// Test double for FakeAsynchronousShutdownObjectContainer.
class FakeAsynchronousShutdownObjectContainer
    : public AsynchronousShutdownObjectContainer {
 public:
  // |deletion_callback| will be invoked when the object is deleted.
  FakeAsynchronousShutdownObjectContainer(
      const base::Closure& deletion_callback = base::DoNothing());
  ~FakeAsynchronousShutdownObjectContainer() override;

  base::Closure& shutdown_complete_callback() {
    return shutdown_complete_callback_;
  }

  void set_tether_host_fetcher(TetherHostFetcher* tether_host_fetcher) {
    tether_host_fetcher_ = tether_host_fetcher;
  }

  void set_disconnect_tethering_request_sender(
      DisconnectTetheringRequestSender* disconnect_tethering_request_sender) {
    disconnect_tethering_request_sender_ = disconnect_tethering_request_sender;
  }

  void set_network_configuration_remover(
      NetworkConfigurationRemover* network_configuration_remover) {
    network_configuration_remover_ = network_configuration_remover;
  }

  void set_wifi_hotspot_disconnector(
      WifiHotspotDisconnector* wifi_hotspot_disconnector) {
    wifi_hotspot_disconnector_ = wifi_hotspot_disconnector;
  }

  // AsynchronousShutdownObjectContainer:
  void Shutdown(const base::Closure& shutdown_complete_callback) override;
  TetherHostFetcher* tether_host_fetcher() override;
  DisconnectTetheringRequestSender* disconnect_tethering_request_sender()
      override;
  NetworkConfigurationRemover* network_configuration_remover() override;
  WifiHotspotDisconnector* wifi_hotspot_disconnector() override;

 private:
  base::Closure deletion_callback_;
  base::Closure shutdown_complete_callback_;

  TetherHostFetcher* tether_host_fetcher_ = nullptr;
  DisconnectTetheringRequestSender* disconnect_tethering_request_sender_ =
      nullptr;
  NetworkConfigurationRemover* network_configuration_remover_ = nullptr;
  WifiHotspotDisconnector* wifi_hotspot_disconnector_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(FakeAsynchronousShutdownObjectContainer);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_FAKE_ASYNCHRONOUS_SHUTDOWN_OBJECT_CONTAINER_H_
