// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_SCREEN_AI_SCREEN_AI_SERVICE_IMPL_H_
#define COMPONENTS_SERVICES_SCREEN_AI_SCREEN_AI_SERVICE_IMPL_H_

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/deferred_sequenced_task_runner.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/services/screen_ai/public/mojom/screen_ai_service.mojom.h"
#include "components/services/screen_ai/screen_ai_library_wrapper.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace ukm {
class UkmRecorder;
}

namespace screen_ai {

using AnnotationCallback = base::OnceCallback<void(const ui::AXTreeID&)>;
using ContentExtractionCallback =
    base::OnceCallback<void(const std::vector<int32_t>&)>;

// Uses a local machine intelligence library to augment the accessibility
// tree. Functionalities include extracting layout and running OCR on passed
// snapshots and extracting the main content of a page.
// See more in: google3/chrome/chromeos/accessibility/machine_intelligence/
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

  void SetLibraryAndStartTaskRunner(
      std::unique_ptr<ScreenAILibraryWrapper> library);

  static void RecordMetrics(ukm::SourceId ukm_source_id,
                            ukm::UkmRecorder* ukm_recorder,
                            base::TimeDelta elapsed_time,
                            bool success);

 private:
  std::unique_ptr<ScreenAILibraryWrapper> library_;

  // mojom::ScreenAIAnnotator:
  void ExtractSemanticLayout(const SkBitmap& image,
                             const ui::AXTreeID& parent_tree_id,
                             AnnotationCallback callback) override;

  // mojom::ScreenAIAnnotator:
  void PerformOcr(const SkBitmap& image,
                  const ui::AXTreeID& parent_tree_id,
                  AnnotationCallback callback) override;

  // mojom::Screen2xMainContentExtractor:
  void ExtractMainContent(const ui::AXTreeUpdate& snapshot,
                          ukm::SourceId ukm_source_id,
                          ContentExtractionCallback callback) override;

  // mojom::ScreenAIService:
  void LoadAndInitializeLibrary(base::File model_config,
                                base::File model_tflite,
                                const base::FilePath& library_path) override;

  // mojom::ScreenAIService:
  void BindAnnotator(
      mojo::PendingReceiver<mojom::ScreenAIAnnotator> annotator) override;

  // mojom::ScreenAIService:
  void BindAnnotatorClient(mojo::PendingRemote<mojom::ScreenAIAnnotatorClient>
                               annotator_client) override;

  // mojom::ScreenAIService:
  void BindMainContentExtractor(
      mojo::PendingReceiver<mojom::Screen2xMainContentExtractor>
          main_content_extractor) override;

  // Common section of PerformOcr and ExtractSemanticLayout functions.
  void PerformVisualAnnotation(const SkBitmap& image,
                               const ui::AXTreeID& parent_tree_id,
                               AnnotationCallback callback,
                               bool run_ocr,
                               bool run_layout_extraction);

  // Wrapper functions for task scheduler.
  void VisualAnnotationInternal(const SkBitmap& image,
                                const ui::AXTreeID& parent_tree_id,
                                bool run_ocr,
                                bool run_layout_extraction,
                                ui::AXTreeUpdate* annotation);
  void ExtractMainContentInternal(const ui::AXTreeUpdate& snapshot,
                                  const ukm::SourceId& ukm_source_id,
                                  std::vector<int32_t>* content_node_ids);

  // Internal task scheduler that starts after library load is completed.
  scoped_refptr<base::DeferredSequencedTaskRunner> task_runner_;

  mojo::Receiver<mojom::ScreenAIService> receiver_;

  // The set of receivers used to receive messages from annotators.
  mojo::ReceiverSet<mojom::ScreenAIAnnotator> screen_ai_annotators_;

  // The client that can receive annotator update messages.
  mojo::Remote<mojom::ScreenAIAnnotatorClient> screen_ai_annotator_client_;

  // The set of receivers used to receive messages from main content
  // extractors.
  mojo::ReceiverSet<mojom::Screen2xMainContentExtractor>
      screen_2x_main_content_extractors_;

  base::WeakPtrFactory<ScreenAIService> weak_ptr_factory_{this};
};

}  // namespace screen_ai

#endif  // COMPONENTS_SERVICES_SCREEN_AI_SCREEN_AI_SERVICE_IMPL_H_
