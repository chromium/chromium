// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_MANTIS_MEDIA_APP_MANTIS_MEDIA_APP_UNTRUSTED_SERVICE_H_
#define CHROMEOS_ASH_COMPONENTS_MANTIS_MEDIA_APP_MANTIS_MEDIA_APP_UNTRUSTED_SERVICE_H_

#include "ash/webui/media_app_ui/media_app_ui_untrusted.mojom.h"
#include "base/component_export.h"
#include "base/sequence_checker.h"
#include "chromeos/ash/components/mantis/media_app/mantis_media_app_untrusted_processor.h"
#include "chromeos/ash/components/mantis/mojom/mantis_service.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {

// A Mojo connection between Mantis service and media app. Its lifetime is bound
// to the Mojo connection and inherently the image file opened in the media app,
// i.e. it gets destructed when the media app window opens a new file, or the
// media app window closes or navigates.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_MANTIS_MEDIA_APP)
    MantisMediaAppUntrustedService
    : public media_app_ui::mojom::MantisMediaAppUntrustedService {
 public:
  // Constructs the class and bind `service_` using CrOS mojo service manager.
  explicit MantisMediaAppUntrustedService(
      mojo::PendingReceiver<media_app_ui::mojom::MantisMediaAppUntrustedService>
          receiver);
  // Constructs the class and bind `service_` by the given `remote_service`.
  // This can be useful if the caller have the `mojo::PendingRemote`, e.g. for
  // testing.
  MantisMediaAppUntrustedService(
      mojo::PendingReceiver<media_app_ui::mojom::MantisMediaAppUntrustedService>
          receiver,
      mojo::PendingRemote<mantis::mojom::MantisService> remote_service);
  MantisMediaAppUntrustedService(const MantisMediaAppUntrustedService&) =
      delete;
  MantisMediaAppUntrustedService& operator=(
      const MantisMediaAppUntrustedService&) = delete;
  ~MantisMediaAppUntrustedService() override;

  // Implements `media_app_ui::mojom::MantisMediaAppUntrustedService`:
  void GetMantisFeatureStatus(GetMantisFeatureStatusCallback callback) override;

  void Initialize(
      mojo::PendingReceiver<
          media_app_ui::mojom::MantisMediaAppUntrustedProcessor> receiver,
      InitializeCallback callback) override;

 private:
  mojo::Receiver<media_app_ui::mojom::MantisMediaAppUntrustedService> receiver_;
  mojo::Remote<mantis::mojom::MantisService> service_;
  std::unique_ptr<MantisMediaAppUntrustedProcessor> processor_;
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_MANTIS_MEDIA_APP_MANTIS_MEDIA_APP_UNTRUSTED_SERVICE_H_
