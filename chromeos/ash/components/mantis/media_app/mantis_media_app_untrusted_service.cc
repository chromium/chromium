// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/mantis/media_app/mantis_media_app_untrusted_service.h"

#include <utility>

namespace ash {

MantisMediaAppUntrustedService::MantisMediaAppUntrustedService(
    mojo::PendingReceiver<media_app_ui::mojom::MantisMediaAppUntrustedService>
        receiver)
    : receiver_(this, std::move(receiver)) {}

MantisMediaAppUntrustedService::~MantisMediaAppUntrustedService() = default;

}  // namespace ash
