// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/wallet/boarding_pass_extractor.h"

#include "chrome/common/chrome_isolated_world_ids.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"

namespace wallet {

BoardingPassExtractor::BoardingPassExtractor(
    content::RenderFrame* render_frame,
    service_manager::BinderRegistry* registry)
    : content::RenderFrameObserver(render_frame) {
  // Being a RenderFrameObserver, this object is scoped to the RenderFrame.
  // Unretained is safe here because `registry` is also scoped to the
  // RenderFrame.
  registry->AddInterface(base::BindRepeating(
      &BoardingPassExtractor::BindReceiver, base::Unretained(this)));
}

BoardingPassExtractor::~BoardingPassExtractor() = default;

void BoardingPassExtractor::BindReceiver(
    mojo::PendingReceiver<mojom::BoardingPassExtractor> receiver) {
  receiver_.reset();
  receiver_.Bind(std::move(receiver));
}

void BoardingPassExtractor::OnDestruct() {
  delete this;
}

void BoardingPassExtractor::ExtractBoardingPass(
    ExtractBoardingPassCallback callback) {
  // TODO(crbug/1502408): Implement boarding pass extractor in render process
}

}  // namespace wallet
