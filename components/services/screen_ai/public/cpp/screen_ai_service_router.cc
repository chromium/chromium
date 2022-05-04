// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/screen_ai/public/cpp/screen_ai_service_router.h"

#include "content/public/browser/service_process_host.h"

namespace screen_ai {

ScreenAIServiceRouter::ScreenAIServiceRouter() = default;
ScreenAIServiceRouter::~ScreenAIServiceRouter() = default;

void ScreenAIServiceRouter::BindScreenAIAnnotator(
    mojo::PendingReceiver<screen_ai::mojom::ScreenAIAnnotator> receiver) {
  LaunchIfNotRunning();

  if (screen_ai_service_.is_bound())
    screen_ai_service_->BindAnnotator(std::move(receiver));
}

void ScreenAIServiceRouter::BindMainContentExtractor(
    mojo::PendingReceiver<screen_ai::mojom::Screen2xMainContentExtractor>
        receiver) {
  LaunchIfNotRunning();

  if (screen_ai_service_.is_bound())
    screen_ai_service_->BindMainContentExtractor(std::move(receiver));
}

void ScreenAIServiceRouter::LaunchIfNotRunning() {
  if (screen_ai_service_.is_bound())
    return;

  content::ServiceProcessHost::Launch(
      screen_ai_service_.BindNewPipeAndPassReceiver(),
      content::ServiceProcessHost::Options()
          .WithDisplayName("Screen AI Service")
          .Pass());
}

}  // namespace screen_ai
