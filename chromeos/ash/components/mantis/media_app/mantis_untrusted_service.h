// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_MANTIS_MEDIA_APP_MANTIS_UNTRUSTED_SERVICE_H_
#define CHROMEOS_ASH_COMPONENTS_MANTIS_MEDIA_APP_MANTIS_UNTRUSTED_SERVICE_H_

#include "ash/webui/media_app_ui/media_app_ui_untrusted.mojom.h"
#include "base/component_export.h"
#include "base/functional/callback_forward.h"
#include "base/sequence_checker.h"
#include "chromeos/ash/components/mantis/mojom/mantis_processor.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {

// A Mojo connection between Mantis processor in CrOS and Mantis untrusted
// service in media app. Its lifetime is bound to the Mojo connection and
// inherently the image file opened in the media app, i.e. it gets destructed
// when the media app window opens a new file, or the media app window closes or
// navigates.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_MANTIS_MEDIA_APP)
    MantisUntrustedService
    : public media_app_ui::mojom::MantisUntrustedService {
 public:
  explicit MantisUntrustedService(
      mojo::PendingRemote<mantis::mojom::MantisProcessor> processor);
  MantisUntrustedService(const MantisUntrustedService&) = delete;
  MantisUntrustedService& operator=(const MantisUntrustedService&) = delete;
  ~MantisUntrustedService() override;

  mojo::PendingRemote<media_app_ui::mojom::MantisUntrustedService>
  BindNewPipeAndPassRemote(base::OnceClosure disconnect_handler);

  // Implements `media_app_ui::mojom::MantisUntrustedService`:
  void SegmentImage(const std::vector<uint8_t>& image,
                    const std::vector<uint8_t>& selection,
                    SegmentImageCallback callback) override;

  void GenerativeFillImage(const std::vector<uint8_t>& image,
                           const std::vector<uint8_t>& mask,
                           const std::string& text,
                           uint32_t seed,
                           GenerativeFillImageCallback callback) override;

  void InpaintImage(const std::vector<uint8_t>& image,
                    const std::vector<uint8_t>& mask,
                    uint32_t seed,
                    InpaintImageCallback callback) override;

  void ClassifyImageSafety(const std::vector<uint8_t>& image,
                           ClassifyImageSafetyCallback callback) override;

  void OutpaintImage(const std::vector<uint8_t>& image,
                     const std::vector<uint8_t>& mask,
                     uint32_t seed,
                     OutpaintImageCallback callback) override;

  void InferSegmentationMode(std::vector<mantis::mojom::TouchPointPtr> gesture,
                             InferSegmentationModeCallback callback) override;

 private:
  mojo::Receiver<media_app_ui::mojom::MantisUntrustedService> receiver_;
  mojo::Remote<mantis::mojom::MantisProcessor> processor_;
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_MANTIS_MEDIA_APP_MANTIS_UNTRUSTED_SERVICE_H_
