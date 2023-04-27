// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_SCREEN_AI_PUBLIC_CPP_SCREEN_AI_SERVICE_ROUTER_H_
#define COMPONENTS_SERVICES_SCREEN_AI_PUBLIC_CPP_SCREEN_AI_SERVICE_ROUTER_H_

#include <vector>

#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/services/screen_ai/public/mojom/screen_ai_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace {
class ComponentModelFiles;
}

namespace screen_ai {

class ScreenAIServiceRouter : public KeyedService {
 public:
  ScreenAIServiceRouter();
  ScreenAIServiceRouter(const ScreenAIServiceRouter&) = delete;
  ScreenAIServiceRouter& operator=(const ScreenAIServiceRouter&) = delete;
  ~ScreenAIServiceRouter() override;

  void BindScreenAIAnnotator(
      mojo::PendingReceiver<mojom::ScreenAIAnnotator> receiver);

  void BindScreenAIAnnotatorClient(
      mojo::PendingRemote<mojom::ScreenAIAnnotatorClient> remote);

  void BindMainContentExtractor(
      mojo::PendingReceiver<mojom::Screen2xMainContentExtractor> receiver);

  void LaunchIfNotRunning();

 private:
  // Triggers loading library and model files and library initialization after
  // the service is up.
  void LoadAndInitializeLibrary(
      std::unique_ptr<ComponentModelFiles> model_files);

  // Callback from Screen AI service with library load result.
  void SetLibraryLoadState(bool successful);

  // Binds queued connections after library is ready.
  void BindQueuedConnections();

  // Queues for pending connections.
  std::vector<mojo::PendingReceiver<mojom::ScreenAIAnnotator>>
      pending_annotators_;
  std::vector<mojo::PendingRemote<mojom::ScreenAIAnnotatorClient>>
      pending_clients_;
  std::vector<mojo::PendingReceiver<mojom::Screen2xMainContentExtractor>>
      pending_main_content_extractors_;

  mojo::Remote<mojom::ScreenAIService> screen_ai_service_;

  base::WeakPtrFactory<ScreenAIServiceRouter> weak_ptr_factory_{this};
};

}  // namespace screen_ai

#endif  // COMPONENTS_SERVICES_SCREEN_AI_PUBLIC_CPP_SCREEN_AI_SERVICE_ROUTER_H_
