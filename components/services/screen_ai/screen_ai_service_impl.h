// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_SCREEN_AI_SCREEN_AI_SERVICE_IMPL_H_
#define COMPONENTS_SERVICES_SCREEN_AI_SCREEN_AI_SERVICE_IMPL_H_

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/scoped_native_library.h"
#include "components/services/screen_ai/public/mojom/screen_ai_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace screen_ai {

using AnnotationCallback = base::OnceCallback<void(const ui::AXTreeID&)>;
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

  // mojom::ScreenAIAnnotator:
  void Annotate(const SkBitmap& image, AnnotationCallback callback) override;

  // mojom::Screen2xMainContentExtractor:
  void ExtractMainContent(const ui::AXTreeUpdate& snapshot,
                          ContentExtractionCallback callback) override;

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

  // Calls |init_function_| and returns the result.
  // Library function calls are isloated to have specific compiler directives.
  bool CallLibraryInitFunction();

  // Calls |annotate_function_| and returns the result.
  // Library function calls are isloated to have specific compiler directives.
  bool CallLibraryAnnotateFunction(const SkBitmap& image,
                                   char*& annotation_proto,
                                   uint32_t& annotation_proto_length);

  // Calls |extract_main_content_function_| and returns the result.
  // Library function calls are isloated to have specific compiler directives.
  bool CallLibraryExtractMainContentFunction(
      const char* serialized_snapshot,
      const uint32_t serialized_snapshot_length,
      int32_t*& node_ids,
      uint32_t& nodes_count);

  // Initializes the pipelines.
  // `models_folder` is a null terminated string pointing to the folder that
  // includes model files.
  typedef bool (*InitFunction)(bool /*init_visual_annotations*/,
                               bool /*init_main_content_extraction*/,
                               bool /*debug_mode*/,
                               const char* /*models_path*/);
  InitFunction init_function_;

  // Sends the given bitmap to ScreenAI pipeline and returns visual annotations.
  // The annotations will be returned as a serialized VisualAnnotation proto.
  // `serialized_visual_annotation` will be allocated for the output and the
  // ownership of the buffer will be passed to the caller function.
  typedef bool (*AnnotateFunction)(
      const SkBitmap& /*bitmap*/,
      char*& /*serialized_visual_annotation*/,
      uint32_t& /*serialized_visual_annotation_length*/);
  AnnotateFunction annotate_function_;

  // Passes the given accessibility tree proto to Screen2x pipeline and returns
  // the main content ids.
  // The input is in form of a serialized ViewHierarchy proto.
  // `content_node_ids` will be allocated for the outputs and the ownership of
  // the array will be passed to the caller function.
  typedef bool (*ExtractMainContentFunction)(
      const char* /*serialized_view_hierarchy*/,
      uint32_t /*serialized_view_hierarchy_length*/,
      int32_t*& /*&content_node_ids*/,
      uint32_t& /*content_node_ids_length*/);
  ExtractMainContentFunction extract_main_content_function_;

  mojo::Receiver<mojom::ScreenAIService> receiver_;

  // The set of receivers used to receive messages from annotators.
  mojo::ReceiverSet<mojom::ScreenAIAnnotator> screen_ai_annotators_;

  // The client that can receive annotator update messages.
  mojo::Remote<mojom::ScreenAIAnnotatorClient> screen_ai_annotator_client_;

  // The set of receivers used to receive messages from main content extractors.
  mojo::ReceiverSet<mojom::Screen2xMainContentExtractor>
      screen_2x_main_content_extractors_;
};

}  // namespace screen_ai

#endif  // COMPONENTS_SERVICES_SCREEN_AI_SCREEN_AI_SERVICE_IMPL_H_
