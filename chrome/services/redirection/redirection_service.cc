// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/redirection/redirection_service.h"

#include <utility>

#include "media/mojo/mojom/remoting_common.mojom.h"

namespace redirection {

RedirectionService::RedirectionService(
    mojo::PendingReceiver<mojom::RedirectionService> receiver)
    : receiver_(this, std::move(receiver)) {}

RedirectionService::~RedirectionService() = default;

void RedirectionService::Start(StartCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(crbug.com/525333080) Populate the sink metadata with the actual
  // remoting sink information from the IMMR* COM interface.
  auto sink_metadata = media::mojom::RemotingSinkMetadata::New();
  std::move(callback).Run(std::move(sink_metadata));
}

}  // namespace redirection
