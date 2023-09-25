// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_SCREEN_AI_SCREEN_AI_SERVICE_IMPL_H_
#define COMPONENTS_SERVICES_SCREEN_AI_SCREEN_AI_SERVICE_IMPL_H_

#include <string>

#include "base/containers/flat_map.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
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

class PreloadedModelData;

// Uses a local machine intelligence library to augment the accessibility
// tree. Functionalities include extracting layout and running OCR on passed
// snapshots and extracting the main content of a page.
// See more in: google3/chrome/chromeos/accessibility/machine_intelligence/
// chrome_screen_ai/README.md
class ScreenAIService : public mojom::ScreenAIServiceFactory,
                        public mojom::OCRService,
                        public mojom::MainContentExtractionService,
                        public mojom::ScreenAIAnnotator,
                        public mojom::Screen2xMainContentExtractor {
 public:
  explicit ScreenAIService(
      mojo::PendingReceiver<mojom::ScreenAIServiceFactory> receiver);
  ScreenAIService(const ScreenAIService&) = delete;
  ScreenAIService& operator=(const ScreenAIService&) = delete;
  ~ScreenAIService() override;

  static void RecordMetrics(ukm::SourceId ukm_source_id,
                            ukm::UkmRecorder* ukm_recorder,
                            base::TimeDelta elapsed_time,
                            bool success);

 private:
  std::unique_ptr<ScreenAILibraryWrapper> library_;

  void LoadLibrary(const base::FilePath& library_path);

  // mojom::ScreenAIAnnotator:
  void ExtractSemanticLayout(const SkBitmap& image,
                             const ui::AXTreeID& parent_tree_id,
                             ExtractSemanticLayoutCallback callback) override;

  // mojom::ScreenAIAnnotator:
  void PerformOcrAndReturnAXTreeUpdate(
      const SkBitmap& image,
      PerformOcrAndReturnAXTreeUpdateCallback callback) override;

  // mojom::ScreenAIAnnotator:
  void PerformOcrAndReturnAnnotation(
      const SkBitmap& image,
      PerformOcrAndReturnAnnotationCallback callback) override;

  // mojom::Screen2xMainContentExtractor:
  void ExtractMainContent(const ui::AXTreeUpdate& snapshot,
                          ukm::SourceId ukm_source_id,
                          ExtractMainContentCallback callback) override;

  // mojom::ScreenAIServiceFactory:
  void InitializeMainContentExtraction(
      const base::FilePath& library_path,
      base::flat_map<std::string, base::File> model_files,
      mojo::PendingReceiver<mojom::MainContentExtractionService>
          main_content_extractor_service_receiver,
      InitializeMainContentExtractionCallback callback) override;

  // mojom::ScreenAIServiceFactory:
  void InitializeOCR(
      const base::FilePath& library_path,
      mojo::PendingReceiver<mojom::OCRService> ocr_service_receiver,
      InitializeOCRCallback callback) override;

  // mojom::OCRService:
  void BindAnnotator(
      mojo::PendingReceiver<mojom::ScreenAIAnnotator> annotator) override;

  // mojom::OCRService:
  void BindAnnotatorClient(mojo::PendingRemote<mojom::ScreenAIAnnotatorClient>
                               annotator_client) override;

  // mojom::MainContentExtractionService:
  void BindMainContentExtractor(
      mojo::PendingReceiver<mojom::Screen2xMainContentExtractor>
          main_content_extractor) override;

  void InitializeMainContentExtractionInternal(
      mojo::PendingReceiver<mojom::MainContentExtractionService>
          main_content_extractor_service_receiver,
      InitializeMainContentExtractionCallback callback,
      std::unique_ptr<PreloadedModelData> model_data);

  // Wrapper to call `PerformOcr` library function and record metrics.
  absl::optional<chrome_screen_ai::VisualAnnotation> PerformOcrAndRecordMetrics(
      const SkBitmap& image);

  mojo::Receiver<mojom::ScreenAIServiceFactory> factory_receiver_;
  mojo::Receiver<mojom::OCRService> ocr_receiver_;
  mojo::Receiver<mojom::MainContentExtractionService>
      main_content_extraction_receiver_;

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
