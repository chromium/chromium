// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/browser/mirroring_media_controller_host.h"

namespace media_router {

MirroringMediaControllerHost::MirroringMediaControllerHost(
    mojo::Remote<media_router::mojom::MediaController> mirroring_controller)
    : mirroring_controller_(std::move(mirroring_controller)) {}

MirroringMediaControllerHost::~MirroringMediaControllerHost() = default;

mojo::PendingRemote<media_router::mojom::MediaStatusObserver>
MirroringMediaControllerHost::GetMediaStatusObserverPendingRemote() {
  return observer_receiver_.BindNewPipeAndPassRemote();
}

void MirroringMediaControllerHost::OnMediaStatusUpdated(
    media_router::mojom::MediaStatusPtr status) {}

}  // namespace media_router
