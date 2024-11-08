// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/mantis/media_app/mantis_media_app_untrusted_service.h"

#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/sequence_checker.h"
#include "chromeos/ash/components/mantis/media_app/mantis_media_app_untrusted_processor.h"
#include "chromeos/ash/components/mantis/mojom/mantis_service.mojom.h"
#include "chromeos/ash/components/mojo_service_manager/connection.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
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

MantisMediaAppUntrustedService::MantisMediaAppUntrustedService(
    mojo::PendingReceiver<media_app_ui::mojom::MantisMediaAppUntrustedService>
        receiver,
    mojo::PendingRemote<mantis::mojom::MantisService> remote_service)
    : receiver_(this, std::move(receiver)),
      service_(std::move(remote_service)) {}

MantisMediaAppUntrustedService::~MantisMediaAppUntrustedService() = default;

void MantisMediaAppUntrustedService::GetMantisFeatureStatus(
    GetMantisFeatureStatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  service_->GetMantisFeatureStatus(std::move(callback));
}

void MantisMediaAppUntrustedService::Initialize(
    mojo::PendingReceiver<media_app_ui::mojom::MantisMediaAppUntrustedProcessor>
        receiver,
    InitializeCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (processor_ != nullptr) {
    receiver_.ReportBadMessage("Mantis is initialized more than once");
    return;
  }
  processor_ =
      std::make_unique<MantisMediaAppUntrustedProcessor>(std::move(receiver));
  // Media app does not need to show initialization progress per
  // http://go/mantis-bl-dd.
  service_->Initialize(/*progress_observer=*/mojo::NullRemote(),
                       processor_->GetReceiver(),
                       base::BindOnce(
                           [](InitializeCallback callback,
                              mantis::mojom::InitializeResult result) {
                             std::move(callback).Run(result);
                           },
                           std::move(callback)));
}

}  // namespace ash
