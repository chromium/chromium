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
#include "base/scoped_native_library.h"
#include "base/task/deferred_sequenced_task_runner.h"
#include "build/build_config.h"
#include "components/services/screen_ai/public/mojom/screen_ai_service.mojom.h"
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

// Defines and keeps pointers to ScreenAI library functions.
class LibraryFunctions {
 public:
  explicit LibraryFunctions(const base::FilePath& library_path);
  LibraryFunctions(const LibraryFunctions&) = delete;
  LibraryFunctions& operator=(const LibraryFunctions&) = delete;
  ~LibraryFunctions() = default;

  // Initializes the pipeline for layout extraction.
  typedef bool (*InitLayoutExtractionFn)();
  InitLayoutExtractionFn init_layout_extraction_ = nullptr;

  // Sends the given bitmap to layout extraction pipeline and returns visual
  // annotations. The annotations will be returned as a serialized
  // VisualAnnotation proto.
  // `serialized_visual_annotation_length` will return the size of required
  // buffer to read the output serialized proto. The buffered results are
  // guaranteed to be kept until the next call to any function of the library.
  // The caller should then allocate the required buffer and call
  // `ReadBufferedCharArray` to get the results. Note that the results can be
  // read only once.
  typedef bool (*ExtractLayoutFn)(
      const SkBitmap& /*bitmap*/,
      uint32_t& /*serialized_visual_annotation_length*/);
  ExtractLayoutFn extract_layout_ = nullptr;

  // Initializes the pipeline for OCR.
  // |models_folder| is a null terminated string pointing to the
  // folder that includes model files for OCR.
  // TODO(http://crbug.com/1278249): Replace |models_folder| with file
  // handle(s).
  typedef bool (*InitOCRFn)(const char* /*models_folder*/);
  InitOCRFn init_ocr_ = nullptr;

  // Sends the given bitmap to the OCR pipeline and returns visual
  // annotations. The annotations will be returned as a serialized
  // VisualAnnotation proto.
  // `serialized_visual_annotation_length` will return the size of required
  // buffer to read the output serialized proto. The buffered results are
  // guaranteed to be kept until the next call to any function of the library.
  // The caller should then allocate the required buffer and call
  // `ReadBufferedCharArray` to get the results. Note that the results can be
  // read only once.
  typedef bool (*PerformOcrFn)(
      const SkBitmap& /*bitmap*/,
      uint32_t& /*serialized_visual_annotation_length*/);
  PerformOcrFn perform_ocr_ = nullptr;

  // Initializes the pipeline for main content extraction.
  // |model_config| and |model_tflite| pass content of the required files to
  // initialize Screen2x engine.
  typedef bool (*InitMainContentExtractionFn)(const char* model_config,
                                              uint32_t model_config_length,
                                              const char* model_tflite,
                                              uint32_t model_tflite_length);
  InitMainContentExtractionFn init_main_content_extraction_ = nullptr;

  // Passes the given accessibility tree proto to Screen2x pipeline and returns
  // the main content ids. The input is in form of a serialized ViewHierarchy
  // proto.
  // `content_node_ids_length` returns the size of required buffer to read the
  // output content node ids. The buffered results are guaranteed to be kept
  // until the next call to any function of the library.
  // The caller should then allocate the required buffer and call
  // `ReadBufferedInt32Array` to get the results. Note that the results can be
  // read only once.
  typedef bool (*ExtractMainContentFn)(
      const char* /*serialized_view_hierarchy*/,
      uint32_t /*serialized_view_hierarchy_length*/,
      uint32_t& /*content_node_ids_length*/);
  ExtractMainContentFn extract_main_content_ = nullptr;

  // Enables the debug mode which stores all i/o protos in the temp folder.
  typedef void (*EnableDebugModeFn)();
  EnableDebugModeFn enable_debug_mode_ = nullptr;

  // Sets a function to receive library logs and add them to Chrome logs.
  typedef void (*SetLoggerFn)(void (*logger_func)(int /*severity*/,
                                                  const char* /*message*/));
  SetLoggerFn set_logger_ = nullptr;

  // Gets the library version number.
  typedef void (*GetLibraryVersionFn)(uint32_t& major, uint32_t& minor);
  GetLibraryVersionFn get_library_version_ = nullptr;

  // Reads buffered int32 array. This function is used to read the int32*
  // results that are computed by other functions and kept in library's memory.
  // The read operation can be done only once and the results are cleared after
  // reading.
  typedef bool (*ReadBufferedInt32ArrayFn)(int32_t* results,
                                           uint32_t max_count);
  ReadBufferedInt32ArrayFn read_buffered_int32_array_;

  // Reads buffered char array. This function is used to read the char*
  // results that are computed by other functions and kept in library's memory.
  // The read operation can be done only once and the results are cleared after
  // reading.
  typedef bool (*ReadBufferedCharArrayFn)(char* results, uint32_t max_count);
  ReadBufferedCharArrayFn read_buffered_char_array_;

 private:
  base::ScopedNativeLibrary library_;
};

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

  void SetLibraryFunctions(std::unique_ptr<LibraryFunctions> library_functions);

  static void RecordMetrics(ukm::SourceId ukm_source_id,
                            ukm::UkmRecorder* ukm_recorder,
                            base::TimeDelta elapsed_time,
                            bool success);

 private:
  std::unique_ptr<LibraryFunctions> library_functions_;

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
  void LoadLibrary(base::File model_config,
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

  // Library function calls are isolated to have specific compiler directives.
  bool CallLibraryLayoutExtractionFunction(const SkBitmap& image,
                                           std::string& annotation_proto);
  bool CallLibraryOcrFunction(const SkBitmap& image,
                              std::string& annotation_proto);
  bool CallLibraryExtractMainContentFunction(
      const std::string& serialized_view_hierarchy,
      std::vector<int32_t>& node_ids);

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
