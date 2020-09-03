// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mirroring/service/mirroring_service.h"

#include "base/bind.h"
#include "base/callback.h"
#include "components/mirroring/service/session.h"
#include "services/viz/public/cpp/gpu/gpu.h"

namespace mirroring {

MirroringService::MirroringService(
    mojo::PendingReceiver<mojom::MirroringService> receiver,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
    : receiver_(this, std::move(receiver)),
      io_task_runner_(std::move(io_task_runner)) {}

MirroringService::~MirroringService() = default;

void MirroringService::Start(
    mojom::SessionParametersPtr params,
    const gfx::Size& max_resolution,
    mojo::PendingRemote<mojom::SessionObserver> observer,
    mojo::PendingRemote<mojom::ResourceProvider> resource_provider,
    mojo::PendingRemote<mojom::CastMessageChannel> outbound_channel,
    mojo::PendingReceiver<mojom::CastMessageChannel> inbound_channel) {
  session_.reset();  // Stops the current session if active.
  session_ = std::make_unique<Session>(
      std::move(params), max_resolution, std::move(observer),
      std::move(resource_provider), std::move(outbound_channel),
      std::move(inbound_channel), io_task_runner_);
}

}  // namespace mirroring
