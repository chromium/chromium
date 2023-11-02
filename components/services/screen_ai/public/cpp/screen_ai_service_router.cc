// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/screen_ai/public/cpp/screen_ai_service_router.h"

#include "components/services/screen_ai/public/cpp/screen_ai_install_state.h"
#include "content/public/browser/service_process_host.h"

namespace screen_ai {

ScreenAIServiceRouter::ScreenAIServiceRouter() = default;
ScreenAIServiceRouter::~ScreenAIServiceRouter() = default;

void ScreenAIServiceRouter::BindScreenAIAnnotator(
    mojo::PendingReceiver<mojom::ScreenAIAnnotator> receiver) {
  LaunchIfNotRunning();

  if (screen_ai_service_.is_bound())
    screen_ai_service_->BindAnnotator(std::move(receiver));
}

void ScreenAIServiceRouter::BindScreenAIAnnotatorClient(
    mojo::PendingRemote<mojom::ScreenAIAnnotatorClient> remote) {
  LaunchIfNotRunning();

  if (screen_ai_service_.is_bound())
    screen_ai_service_->BindAnnotatorClient(std::move(remote));
}

void ScreenAIServiceRouter::BindMainContentExtractor(
    mojo::PendingReceiver<mojom::Screen2xMainContentExtractor> receiver) {
  LaunchIfNotRunning();

  if (screen_ai_service_.is_bound())
    screen_ai_service_->BindMainContentExtractor(std::move(receiver));
}

void ScreenAIServiceRouter::LaunchIfNotRunning() {
  if (screen_ai_service_.is_bound())
    return;

  if (!ScreenAIInstallState::GetInstance()->is_component_ready()) {
    VLOG(0)
        << "ScreenAI service launch triggered before the component is ready.";
    return;
  }

  content::ServiceProcessHost::Launch(
      screen_ai_service_.BindNewPipeAndPassReceiver(),
      content::ServiceProcessHost::Options()
          .WithDisplayName("Screen AI Service")
          .Pass());
}

}  // namespace screen_ai
