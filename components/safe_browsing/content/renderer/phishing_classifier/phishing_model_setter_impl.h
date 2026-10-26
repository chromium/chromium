// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_RENDERER_PHISHING_CLASSIFIER_PHISHING_MODEL_SETTER_IMPL_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_RENDERER_PHISHING_CLASSIFIER_PHISHING_MODEL_SETTER_IMPL_H_

#include "components/safe_browsing/content/common/safe_browsing.mojom.h"
#include "content/public/renderer/render_thread_observer.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/rust/cxx/v1/cxx.h"

namespace safe_browsing {

namespace rust_bridge {
// Defined in rust/phishing_model_setter_bridge.rs. Forward-declared so that
// files including this header don't also need the generated bridge header.
struct PhishingModelSetterState;
}  // namespace rust_bridge

// TODO(crbug.com/561593066): Once `kRustPhishingModelSetter` has fully rolled
// out, delete the flag and the mojom::PhishingModelSetter implementation here,
// leaving only the RenderThreadObserver that owns the Rust state and forwards
// receivers to it.
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
  void SetImageEmbeddingAndPhishingTfLiteModel(
      int classification_input_width,
      int classification_input_height,
      base::File tflite_visual_model,
      int image_embedding_input_width,
      int image_embedding_input_height,
      base::File image_embedding_model) override;
  void SetPhishingTfLiteModel(int classification_input_width,
                              int classification_input_height,
                              base::File tflite_visual_model) override;
  void AttachImageEmbeddingModelAndDimensions(
      int image_embedding_input_width,
      int image_embedding_input_height,
      base::File image_embedding_model) override;
  void ClearScorer() override;
  void SetTestObserver(
      mojo::PendingRemote<mojom::PhishingModelSetterTestObserver> observer,
      SetTestObserverCallback callback) override;

  void OnRendererAssociatedRequest(
      mojo::PendingAssociatedReceiver<mojom::PhishingModelSetter> receiver);

  mojo::Remote<mojom::PhishingModelSetterTestObserver> observer_for_testing_;

  mojo::AssociatedReceiver<mojom::PhishingModelSetter> receiver_{this};

  // State shared by every receiver bound to the Rust implementation, which is
  // used instead of the C++ one when `kRustPhishingModelSetter` is enabled.
  rust::Box<rust_bridge::PhishingModelSetterState> rust_state_;
};
}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_RENDERER_PHISHING_CLASSIFIER_PHISHING_MODEL_SETTER_IMPL_H_
