// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_EFFECTS_MEDIA_EFFECTS_MANAGER_BINDER_H_
#define COMPONENTS_MEDIA_EFFECTS_MEDIA_EFFECTS_MANAGER_BINDER_H_

#include "content/public/browser/browser_context.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/video_capture/public/mojom/video_effects_manager.mojom.h"

namespace media_effects {

void BindVideoEffectsManager(
    const std::string& device_id,
    content::BrowserContext* browser_context,
    mojo::PendingReceiver<video_capture::mojom::VideoEffectsManager>
        video_effects_manager);

}  // namespace media_effects

#endif  // COMPONENTS_MEDIA_EFFECTS_MEDIA_EFFECTS_MANAGER_BINDER_H_
