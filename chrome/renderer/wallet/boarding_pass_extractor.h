// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_WALLET_BOARDING_PASS_EXTRACTOR_H_
#define CHROME_RENDERER_WALLET_BOARDING_PASS_EXTRACTOR_H_

#include "chrome/common/wallet/boarding_pass_extractor.mojom.h"
#include "content/public/renderer/render_frame_observer.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/service_manager/public/cpp/binder_registry.h"

namespace wallet {

// Implements mojom::BoardingPassExtractor.
class BoardingPassExtractor : public content::RenderFrameObserver,
                              public mojom::BoardingPassExtractor {
 public:
  explicit BoardingPassExtractor(content::RenderFrame* render_frame,
                                 service_manager::BinderRegistry* registry);

  ~BoardingPassExtractor() override;

  // mojom::BoardingPassExtractor
  void ExtractBoardingPass(ExtractBoardingPassCallback callback) override;

  // content::RenderFrameObserver
  void OnDestruct() override;

 private:
  void BindReceiver(
      mojo::PendingReceiver<mojom::BoardingPassExtractor> receiver);

  mojo::Receiver<mojom::BoardingPassExtractor> receiver_{this};
};
}  // namespace wallet

#endif  // CHROME_RENDERER_WALLET_BOARDING_PASS_EXTRACTOR_H_
