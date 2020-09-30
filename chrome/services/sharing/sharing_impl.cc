// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/sharing_impl.h"

#include <utility>

#include "base/callback.h"
#include "chrome/services/sharing/nearby/decoder/nearby_decoder.h"
#include "chrome/services/sharing/nearby/nearby_connections.h"
#include "chrome/services/sharing/public/mojom/nearby_decoder.mojom.h"

namespace sharing {

SharingImpl::SharingImpl(
    mojo::PendingReceiver<mojom::Sharing> receiver,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner)
    : receiver_(this, std::move(receiver)),
      io_task_runner_(std::move(io_task_runner)) {}

SharingImpl::~SharingImpl() = default;

void SharingImpl::CreateNearbyConnections(
    NearbyConnectionsDependenciesPtr dependencies,
    CreateNearbyConnectionsCallback callback) {
  // Reset old instance of Nearby Connections stack.
  nearby_connections_.reset();

  mojo::PendingRemote<NearbyConnectionsMojom> remote;
  nearby_connections_ = std::make_unique<NearbyConnections>(
      remote.InitWithNewPipeAndPassReceiver(), std::move(dependencies),
      io_task_runner_,
      base::BindOnce(&SharingImpl::NearbyConnectionsDisconnected,
                     weak_ptr_factory_.GetWeakPtr()));
  std::move(callback).Run(std::move(remote));
}

void SharingImpl::CreateNearbySharingDecoder(
    CreateNearbySharingDecoderCallback callback) {
  // Reset old instance of Nearby Sharing Decoder stack.
  nearby_decoder_.reset();

  mojo::PendingRemote<sharing::mojom::NearbySharingDecoder> remote;
  nearby_decoder_ = std::make_unique<NearbySharingDecoder>(
      remote.InitWithNewPipeAndPassReceiver());
  std::move(callback).Run(std::move(remote));
}

void SharingImpl::NearbyConnectionsDisconnected() {
  nearby_connections_.reset();
}

}  // namespace sharing
