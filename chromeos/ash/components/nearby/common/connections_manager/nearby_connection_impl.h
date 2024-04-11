// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NEARBY_COMMON_CONNECTIONS_MANAGER_NEARBY_CONNECTION_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_NEARBY_COMMON_CONNECTIONS_MANAGER_NEARBY_CONNECTION_IMPL_H_

#include <queue>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/nearby/common/connections_manager/nearby_connection.h"
#include "base/sequence_checker.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_connections_types.mojom.h"

class NearbyConnectionsManager;

class NearbyConnectionImpl : public NearbyConnection {
 public:
  NearbyConnectionImpl(
      base::WeakPtr<NearbyConnectionsManager> nearby_connections_manager,
      const std::string& endpoint_id);
  ~NearbyConnectionImpl() override;

  // NearbyConnection:
  void Read(ReadCallback callback) override;
  void Write(std::vector<uint8_t> bytes) override;
  void Close() override;
  void SetDisconnectionListener(base::OnceClosure listener) override;

  // Add |bytes| to the read queue, notifying ReadCallback.
  void WriteMessage(std::vector<uint8_t> bytes);

 private:
  using PayloadContent = nearby::connections::mojom::PayloadContent;
  using BytesPayload = nearby::connections::mojom::BytesPayload;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtr<NearbyConnectionsManager> nearby_connections_manager_;
  std::string endpoint_id_;
  ReadCallback read_callback_;
  base::OnceClosure disconnect_listener_;

  // A read queue. The data that we've read from the remote device ends up here
  // until Read() is called to dequeue it.
  std::queue<std::vector<uint8_t>> reads_;
};

#endif  // CHROMEOS_ASH_COMPONENTS_NEARBY_COMMON_CONNECTIONS_MANAGER_NEARBY_CONNECTION_IMPL_H_
