// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TETHER_DISCONNECT_TETHERING_REQUEST_SENDER_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_TETHER_DISCONNECT_TETHERING_REQUEST_SENDER_IMPL_H_

#include <map>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/tether/disconnect_tethering_operation.h"
#include "chromeos/ash/components/tether/disconnect_tethering_request_sender.h"
#include "chromeos/ash/components/tether/host_connection.h"

namespace ash::tether {

class TetherHostFetcher;

class DisconnectTetheringRequestSenderImpl
    : public DisconnectTetheringRequestSender,
      public DisconnectTetheringOperation::Observer {
 public:
  class Factory {
   public:
    static std::unique_ptr<DisconnectTetheringRequestSender> Create(
        raw_ptr<HostConnection::Factory> host_connection_factory,
        TetherHostFetcher* tether_host_fetcher);

    static void SetFactoryForTesting(Factory* factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<DisconnectTetheringRequestSender> CreateInstance(
        raw_ptr<HostConnection::Factory> host_connection_factory,
        TetherHostFetcher* tether_host_fetcher) = 0;

   private:
    static Factory* factory_instance_;
  };

  DisconnectTetheringRequestSenderImpl(
      const DisconnectTetheringRequestSenderImpl&) = delete;
  DisconnectTetheringRequestSenderImpl& operator=(
      const DisconnectTetheringRequestSenderImpl&) = delete;

  ~DisconnectTetheringRequestSenderImpl() override;

  // DisconnectTetheringRequestSender:
  void SendDisconnectRequestToDevice(const std::string& device_id) override;
  bool HasPendingRequests() override;

  // DisconnectTetheringOperation::Observer:
  void OnOperationFinished(const std::string& device_id, bool success) override;

 protected:
  DisconnectTetheringRequestSenderImpl(
      raw_ptr<HostConnection::Factory> host_connection_factory,
      TetherHostFetcher* tether_host_fetcher);

 private:
  raw_ptr<HostConnection::Factory> host_connection_factory_;
  raw_ptr<TetherHostFetcher> tether_host_fetcher_;

  std::map<std::string, std::unique_ptr<DisconnectTetheringOperation>>
      device_id_to_operation_map_;

  base::WeakPtrFactory<DisconnectTetheringRequestSenderImpl> weak_ptr_factory_{
      this};
};

}  // namespace ash::tether

#endif  // CHROMEOS_ASH_COMPONENTS_TETHER_DISCONNECT_TETHERING_REQUEST_SENDER_IMPL_H_
