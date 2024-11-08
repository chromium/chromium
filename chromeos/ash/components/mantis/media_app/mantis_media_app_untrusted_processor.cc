// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/mantis/media_app/mantis_media_app_untrusted_processor.h"

#include <utility>

#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace ash {

MantisMediaAppUntrustedProcessor::MantisMediaAppUntrustedProcessor(
    mojo::PendingReceiver<media_app_ui::mojom::MantisMediaAppUntrustedProcessor>
        receiver)
    : receiver_(this, std::move(receiver)) {}

MantisMediaAppUntrustedProcessor::~MantisMediaAppUntrustedProcessor() = default;

mojo::PendingReceiver<mantis::mojom::MantisProcessor>
MantisMediaAppUntrustedProcessor::GetReceiver() {
  return processor_.BindNewPipeAndPassReceiver();
}

}  // namespace ash
