// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/nearby/common/connections_manager/nearby_connection_impl.h"

#include "chromeos/ash/components/nearby/common/connections_manager/nearby_connections_manager.h"
#include "crypto/random.h"

NearbyConnectionImpl::NearbyConnectionImpl(
    base::WeakPtr<NearbyConnectionsManager> nearby_connections_manager,
    const std::string& endpoint_id)
    : nearby_connections_manager_(nearby_connections_manager),
      endpoint_id_(endpoint_id) {}

NearbyConnectionImpl::~NearbyConnectionImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (disconnect_listener_)
    std::move(disconnect_listener_).Run();

  if (read_callback_)
    std::move(read_callback_).Run(std::nullopt);
}

void NearbyConnectionImpl::Read(ReadCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  read_callback_ = std::move(callback);

  if (reads_.empty())
    return;

  std::vector<uint8_t> bytes = std::move(reads_.front());
  reads_.pop();
  std::move(read_callback_).Run(std::move(bytes));
}

void NearbyConnectionImpl::Write(std::vector<uint8_t> bytes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  NearbyConnectionsManager::PayloadPtr payload =
      NearbyConnectionsManager::Payload::New();
  crypto::RandBytes(base::byte_span_from_ref(payload->id));
  payload->content =
      PayloadContent::NewBytes(BytesPayload::New(std::move(bytes)));
  nearby_connections_manager_->Send(endpoint_id_, std::move(payload),
                                    /*listener=*/nullptr);
}

void NearbyConnectionImpl::Close() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // As |this| therefore endpoint_id_ will be distroyed in Disconnect, make a
  // copy of |endpoint_id| as the parameter is a const ref.
  nearby_connections_manager_->Disconnect(std::string(endpoint_id_));
}

void NearbyConnectionImpl::SetDisconnectionListener(
    base::OnceClosure listener) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  disconnect_listener_ = std::move(listener);
}

void NearbyConnectionImpl::WriteMessage(std::vector<uint8_t> bytes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (read_callback_) {
    std::move(read_callback_).Run(std::move(bytes));
    return;
  }

  reads_.push(std::move(bytes));
}
