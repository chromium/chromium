// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CONTENT_RENDERER_COMMERCE_WEB_EXTRACTOR_H_
#define COMPONENTS_COMMERCE_CONTENT_RENDERER_COMMERCE_WEB_EXTRACTOR_H_

#include "components/commerce/core/mojom/commerce_web_extractor.mojom.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_frame_observer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/service_manager/public/cpp/binder_registry.h"

namespace commerce {
class CommerceWebExtractor
    : public content::RenderFrameObserver,
      public commerce_web_extractor::mojom::CommerceWebExtractor {
 public:
  CommerceWebExtractor(const CommerceWebExtractor&) = delete;
  CommerceWebExtractor& operator=(const CommerceWebExtractor&) = delete;
  CommerceWebExtractor(content::RenderFrame* render_frame,
                       service_manager::BinderRegistry* registry);
  ~CommerceWebExtractor() override;

  // commerce_web_extractor::mojom::CommerceWebExtractor:
  void ExtractMetaInfo(ExtractMetaInfoCallback callback) override;

  // RenderFrameObserver:
  void OnDestruct() override;

 private:
  void BindReceiver(
      mojo::PendingReceiver<commerce_web_extractor::mojom::CommerceWebExtractor>
          receiver);

  raw_ptr<content::RenderFrame> render_frame_;
  mojo::Receiver<commerce_web_extractor::mojom::CommerceWebExtractor> receiver_{
      this};
};
}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CONTENT_RENDERER_COMMERCE_WEB_EXTRACTOR_H_
