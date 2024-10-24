// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_MANTIS_MEDIA_APP_MANTIS_MEDIA_APP_UNTRUSTED_SERVICE_H_
#define CHROMEOS_ASH_COMPONENTS_MANTIS_MEDIA_APP_MANTIS_MEDIA_APP_UNTRUSTED_SERVICE_H_

#include "ash/webui/media_app_ui/media_app_ui_untrusted.mojom.h"
#include "base/component_export.h"
#include "chromeos/ash/components/mantis/mojom/mantis_service.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {

// A Mojo connection between Mantis and media app. Its lifetime is bound to the
// Mojo connection and inherently the image file opened in the media app, i.e.
// it gets destructed when the media app window opens a new file, or the media
// app window closes or navigates.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_MANTIS_MEDIA_APP)
    MantisMediaAppUntrustedService
    : public media_app_ui::mojom::MantisMediaAppUntrustedService {
 public:
  explicit MantisMediaAppUntrustedService(
      mojo::PendingReceiver<media_app_ui::mojom::MantisMediaAppUntrustedService>
          receiver);
  MantisMediaAppUntrustedService(const MantisMediaAppUntrustedService&) =
      delete;
  MantisMediaAppUntrustedService& operator=(
      const MantisMediaAppUntrustedService&) = delete;
  ~MantisMediaAppUntrustedService() override;

  // TODO(crbug.com/368986974): Add methods to implement
  // MantisMediaAppUntrustedService.

 private:
  mojo::Receiver<media_app_ui::mojom::MantisMediaAppUntrustedService> receiver_;
  mojo::Remote<mantis::mojom::MantisService> service_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_MANTIS_MEDIA_APP_MANTIS_MEDIA_APP_UNTRUSTED_SERVICE_H_
