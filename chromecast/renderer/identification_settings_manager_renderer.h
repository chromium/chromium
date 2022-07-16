// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_RENDERER_IDENTIFICATION_SETTINGS_MANAGER_RENDERER_H_
#define CHROMECAST_RENDERER_IDENTIFICATION_SETTINGS_MANAGER_RENDERER_H_

#include "base/callback_forward.h"
#include "chromecast/common/identification_settings_manager.h"
#include "content/public/renderer/render_frame_observer.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"

namespace chromecast {

// Receives messages from the browser process and stores identification settings
// to feed into URLLoaderThrottles for throttling url requests in renderers.
// Note: this class could be deleted on a different thread from the main thread.
class IdentificationSettingsManagerRenderer
    : public content::RenderFrameObserver,
      public IdentificationSettingsManager {
 public:
  IdentificationSettingsManagerRenderer(
      content::RenderFrame* render_frame,
      base::OnceCallback<void()> on_removed_callback);
  IdentificationSettingsManagerRenderer(
      const IdentificationSettingsManagerRenderer&) = delete;
  IdentificationSettingsManagerRenderer& operator=(
      const IdentificationSettingsManagerRenderer&) = delete;

 protected:
  ~IdentificationSettingsManagerRenderer() override;

 private:
  // content::RenderFrameObserver implementation:
  void OnDestruct() override;

  void OnIdentificationSettingsManagerAssociatedRequest(
      mojo::PendingAssociatedReceiver<mojom::IdentificationSettingsManager>
          receiver);

  base::OnceCallback<void()> on_removed_callback_;
  mojo::AssociatedReceiver<mojom::IdentificationSettingsManager>
      associated_receiver_{this};
};

}  // namespace chromecast

#endif  // CHROMECAST_RENDERER_IDENTIFICATION_SETTINGS_MANAGER_RENDERER_H_
