// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/mantis/media_app/mantis_media_app_untrusted_service.h"

#include <utility>

#include "chromeos/ash/components/mantis/mojom/mantis_service.mojom.h"
#include "chromeos/ash/components/mojo_service_manager/connection.h"
#include "third_party/cros_system_api/mojo/service_constants.h"

namespace ash {

MantisMediaAppUntrustedService::MantisMediaAppUntrustedService(
    mojo::PendingReceiver<media_app_ui::mojom::MantisMediaAppUntrustedService>
        receiver)
    : receiver_(this, std::move(receiver)) {
  ash::mojo_service_manager::GetServiceManagerProxy()->Request(
      chromeos::mojo_services::kCrosMantisService, std::nullopt,
      service_.BindNewPipeAndPassReceiver().PassPipe());
  service_.reset_on_disconnect();
}

MantisMediaAppUntrustedService::~MantisMediaAppUntrustedService() = default;

}  // namespace ash
