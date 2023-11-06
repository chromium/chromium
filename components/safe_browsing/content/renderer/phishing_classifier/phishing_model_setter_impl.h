// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_RENDERER_PHISHING_CLASSIFIER_PHISHING_MODEL_SETTER_IMPL_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_RENDERER_PHISHING_CLASSIFIER_PHISHING_MODEL_SETTER_IMPL_H_

#include "components/safe_browsing/content/common/safe_browsing.mojom.h"
#include "content/public/renderer/render_thread_observer.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace safe_browsing {

class PhishingModelSetterImpl : public mojom::PhishingModelSetter,
                                public content::RenderThreadObserver {
 public:
  PhishingModelSetterImpl();

  PhishingModelSetterImpl(const PhishingModelSetterImpl&) = delete;
  PhishingModelSetterImpl& operator=(const PhishingModelSetterImpl&) = delete;

  ~PhishingModelSetterImpl() override;

 private:
  // content::RenderThreadObserver overrides:
  void RegisterMojoInterfaces(
      blink::AssociatedInterfaceRegistry* associated_interfaces) override;
  void UnregisterMojoInterfaces(
      blink::AssociatedInterfaceRegistry* associated_interfaces) override;

  // mojom::PhishingModelSetter overrides:
  void SetImageEmbeddingAndPhishingFlatBufferModel(
      base::ReadOnlySharedMemoryRegion flatbuffer_region,
      base::File tflite_visual_model,
      base::File image_embedding_model) override;
  void SetPhishingFlatBufferModel(
      base::ReadOnlySharedMemoryRegion flatbuffer_region,
      base::File tflite_visual_model) override;
  void AttachImageEmbeddingModel(base::File image_embedding_model) override;
  void ClearScorer() override;
  void SetTestObserver(
      mojo::PendingRemote<mojom::PhishingModelSetterTestObserver> observer,
      SetTestObserverCallback callback) override;

  void OnRendererAssociatedRequest(
      mojo::PendingAssociatedReceiver<mojom::PhishingModelSetter> receiver);

  mojo::Remote<mojom::PhishingModelSetterTestObserver> observer_for_testing_;

  mojo::AssociatedReceiver<mojom::PhishingModelSetter> receiver_{this};
};
}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_RENDERER_PHISHING_CLASSIFIER_PHISHING_MODEL_SETTER_IMPL_H_
