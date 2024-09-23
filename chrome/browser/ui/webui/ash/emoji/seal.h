// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_EMOJI_SEAL_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_EMOJI_SEAL_H_

#include "base/base64.h"
#include "chrome/browser/manta/manta_service_factory.h"
#include "chrome/browser/ui/webui/ash/emoji/seal.mojom.h"
#include "components/manta/manta_service.h"
#include "components/manta/snapper_provider.h"
#include "content/public/browser/render_frame_host.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace ash {

class SealService : public seal::mojom::SealService {
 public:
  explicit SealService(
      mojo::PendingReceiver<seal::mojom::SealService> receiver,
      std::unique_ptr<manta::SnapperProvider> snapper_provider);
  ~SealService() override;

  // seal::mojom::SealService
  void GetImages(const std::string& query, GetImagesCallback callback) override;

 private:
  void HandleSnapperResponse(const std::string& query,
                             GetImagesCallback callback,
                             std::unique_ptr<manta::proto::Response> response,
                             manta::MantaStatus status);

  mojo::Receiver<seal::mojom::SealService> receiver_;
  std::unique_ptr<manta::SnapperProvider> snapper_provider_;
  base::WeakPtrFactory<SealService> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_EMOJI_SEAL_H_
