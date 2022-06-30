// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_SCREEN_AI_SCREEN_AI_SERVICE_IMPL_H_
#define COMPONENTS_SERVICES_SCREEN_AI_SCREEN_AI_SERVICE_IMPL_H_

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/scoped_native_library.h"
#include "components/services/screen_ai/public/mojom/screen_ai_service.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace screen_ai {

using AnnotationCallback = base::OnceCallback<void(const ui::AXTreeUpdate&)>;
using ContentExtractionCallback =
    base::OnceCallback<void(const std::vector<int32_t>&)>;

// Sends the snapshot to a local machine learning library to get annotations
// that can help in updating the accessibility tree. See more in:
// google3/chrome/chromeos/accessibility/machine_intelligence/
// chrome_screen_ai/README.md
class ScreenAIService : public mojom::ScreenAIService,
                        public mojom::ScreenAIAnnotator,
                        public mojom::Screen2xMainContentExtractor {
 public:
  explicit ScreenAIService(
      mojo::PendingReceiver<mojom::ScreenAIService> receiver);
  ScreenAIService(const ScreenAIService&) = delete;
  ScreenAIService& operator=(const ScreenAIService&) = delete;
  ~ScreenAIService() override;

 private:
  base::ScopedNativeLibrary library_;

  // mojom::ScreenAIAnnotator
  void Annotate(const SkBitmap& image, AnnotationCallback callback) override;

  // mojom::Screen2xMainContentExtractor
  void ExtractMainContent(const ui::AXTreeUpdate& snapshot,
                          ContentExtractionCallback callback) override;

  // mojom::ScreenAIService
  void BindAnnotator(
      mojo::PendingReceiver<mojom::ScreenAIAnnotator> annotator) override;

  // mojom::ScreenAIService
  void BindMainContentExtractor(
      mojo::PendingReceiver<mojom::Screen2xMainContentExtractor>
          main_content_extractor) override;

  typedef bool (*InitFunction)(bool /*init_visual_annotations*/,
                               bool /*init_main_content_extraction*/,
                               bool /*debug_mode*/,
                               const std::string& /*models_path*/);
  InitFunction init_function_;

  typedef bool (*AnnotateFunction)(const SkBitmap& /*image*/,
                                   std::string& /*annotation_text*/);
  AnnotateFunction annotate_function_;

  typedef bool (*ExtractMainContentFunction)(
      const std::string& /*serialized_snapshot*/,
      std::vector<int32_t>& /*content_node_ids*/);
  ExtractMainContentFunction extract_main_content_function_;

  mojo::Receiver<mojom::ScreenAIService> receiver_;

  // The set of receivers used to receive messages from annotators.
  mojo::ReceiverSet<mojom::ScreenAIAnnotator> screen_ai_annotators_;

  // The set of receivers used to receive messages from main content extractors.
  mojo::ReceiverSet<mojom::Screen2xMainContentExtractor>
      screen_2x_main_content_extractors_;
};

}  // namespace screen_ai

#endif  // COMPONENTS_SERVICES_SCREEN_AI_SCREEN_AI_SERVICE_IMPL_H_
