// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/renderer/identification_settings_manager_renderer.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/check.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"

namespace chromecast {

IdentificationSettingsManagerRenderer::IdentificationSettingsManagerRenderer(
    content::RenderFrame* render_frame,
    base::OnceCallback<void()> on_removed_callback)
    : content::RenderFrameObserver(render_frame),
      on_removed_callback_(std::move(on_removed_callback)) {
  // base::Unretained is safe here since |this| won't get more binding requests
  // after |render_frame| is gone.
  render_frame->GetAssociatedInterfaceRegistry()->AddInterface(
      base::BindRepeating(&IdentificationSettingsManagerRenderer::
                              OnIdentificationSettingsManagerAssociatedRequest,
                          base::Unretained(this)));
}

IdentificationSettingsManagerRenderer::
    ~IdentificationSettingsManagerRenderer() {
  if (on_removed_callback_) {
    std::move(on_removed_callback_).Run();
  }
}

void IdentificationSettingsManagerRenderer::OnDestruct() {
  DCHECK(on_removed_callback_);
  std::move(on_removed_callback_).Run();
}

void IdentificationSettingsManagerRenderer::
    OnIdentificationSettingsManagerAssociatedRequest(
        mojo::PendingAssociatedReceiver<mojom::IdentificationSettingsManager>
            receiver) {
  associated_receiver_.Bind(std::move(receiver));
}

}  // namespace chromecast
