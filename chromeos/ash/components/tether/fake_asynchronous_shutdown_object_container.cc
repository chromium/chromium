// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/fake_asynchronous_shutdown_object_container.h"

namespace ash::tether {

FakeAsynchronousShutdownObjectContainer::
    FakeAsynchronousShutdownObjectContainer(base::OnceClosure deletion_callback)
    : deletion_callback_(std::move(deletion_callback)) {}

FakeAsynchronousShutdownObjectContainer::
    ~FakeAsynchronousShutdownObjectContainer() {
  std::move(deletion_callback_).Run();
}

void FakeAsynchronousShutdownObjectContainer::Shutdown(
    base::OnceClosure shutdown_complete_callback) {
  shutdown_complete_callback_ = std::move(shutdown_complete_callback);
}

TetherHostFetcher*
FakeAsynchronousShutdownObjectContainer::tether_host_fetcher() {
  return tether_host_fetcher_;
}

DisconnectTetheringRequestSender*
FakeAsynchronousShutdownObjectContainer::disconnect_tethering_request_sender() {
  return disconnect_tethering_request_sender_;
}

NetworkConfigurationRemover*
FakeAsynchronousShutdownObjectContainer::network_configuration_remover() {
  return network_configuration_remover_;
}

WifiHotspotDisconnector*
FakeAsynchronousShutdownObjectContainer::wifi_hotspot_disconnector() {
  return wifi_hotspot_disconnector_;
}

HostConnection::Factory*
FakeAsynchronousShutdownObjectContainer::host_connection_factory() {
  return host_connection_factory_;
}

}  // namespace ash::tether
