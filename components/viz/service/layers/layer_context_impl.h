// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_LAYERS_LAYER_CONTEXT_IMPL_H_
#define COMPONENTS_VIZ_SERVICE_LAYERS_LAYER_CONTEXT_IMPL_H_

#include <memory>

#include "cc/animation/animation_host.h"
#include "cc/trees/local_layer_context.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "services/viz/public/mojom/compositing/layer_context.mojom.h"

namespace viz {

// Implements the Viz LayerContext API backed by a LocalLayerContext. This
// provides the service backend for a client-side VizLayerContext.
class LayerContextImpl : public mojom::LayerContext {
 public:
  // Constructs a new LayerContextImpl with client connection details given by
  // `context`.
  explicit LayerContextImpl(mojom::PendingLayerContext& context);
  ~LayerContextImpl() override;

 private:
  // mojom::LayerContext:
  void SetTargetLocalSurfaceId(const LocalSurfaceId& id) override;
  void SetVisible(bool visible) override;
  void Commit(mojom::LayerTreeUpdatePtr update) override;

  const std::unique_ptr<cc::AnimationHost> animation_host_{
      cc::AnimationHost::CreateMainInstance()};
  cc::LocalLayerContext context_{animation_host_.get()};

  mojo::AssociatedReceiver<mojom::LayerContext> receiver_;
  mojo::AssociatedRemote<mojom::LayerContextClient> client_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_LAYERS_LAYER_CONTEXT_IMPL_H_
