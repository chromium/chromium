// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TETHER_FAKE_ASYNCHRONOUS_SHUTDOWN_OBJECT_CONTAINER_H_
#define CHROMEOS_ASH_COMPONENTS_TETHER_FAKE_ASYNCHRONOUS_SHUTDOWN_OBJECT_CONTAINER_H_

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/tether/asynchronous_shutdown_object_container.h"

namespace ash::tether {

// Test double for FakeAsynchronousShutdownObjectContainer.
class FakeAsynchronousShutdownObjectContainer
    : public AsynchronousShutdownObjectContainer {
 public:
  // |deletion_callback| will be invoked when the object is deleted.
  FakeAsynchronousShutdownObjectContainer(
      base::OnceClosure deletion_callback = base::DoNothing());

  FakeAsynchronousShutdownObjectContainer(
      const FakeAsynchronousShutdownObjectContainer&) = delete;
  FakeAsynchronousShutdownObjectContainer& operator=(
      const FakeAsynchronousShutdownObjectContainer&) = delete;

  ~FakeAsynchronousShutdownObjectContainer() override;

  base::OnceClosure TakeShutdownCompleteCallback() {
    return std::move(shutdown_complete_callback_);
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

  void set_host_connection_factory(
      HostConnection::Factory* host_connection_factory) {
    host_connection_factory_ = host_connection_factory;
  }

  // AsynchronousShutdownObjectContainer:
  void Shutdown(base::OnceClosure shutdown_complete_callback) override;
  TetherHostFetcher* tether_host_fetcher() override;
  DisconnectTetheringRequestSender* disconnect_tethering_request_sender()
      override;
  NetworkConfigurationRemover* network_configuration_remover() override;
  WifiHotspotDisconnector* wifi_hotspot_disconnector() override;
  HostConnection::Factory* host_connection_factory() override;

 private:
  base::OnceClosure deletion_callback_;
  base::OnceClosure shutdown_complete_callback_;

  raw_ptr<TetherHostFetcher> tether_host_fetcher_ = nullptr;
  raw_ptr<HostConnection::Factory> host_connection_factory_ = nullptr;
  raw_ptr<DisconnectTetheringRequestSender>
      disconnect_tethering_request_sender_ = nullptr;
  raw_ptr<NetworkConfigurationRemover> network_configuration_remover_ = nullptr;
  raw_ptr<WifiHotspotDisconnector> wifi_hotspot_disconnector_ = nullptr;
};

}  // namespace ash::tether

#endif  // CHROMEOS_ASH_COMPONENTS_TETHER_FAKE_ASYNCHRONOUS_SHUTDOWN_OBJECT_CONTAINER_H_
