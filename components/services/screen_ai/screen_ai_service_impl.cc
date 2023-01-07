// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/screen_ai/screen_ai_service_impl.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/process/process.h"
#include "base/strings/string_util.h"
#include "components/services/screen_ai/proto/proto_convertor.h"
#include "components/services/screen_ai/public/cpp/utilities.h"
#include "components/services/screen_ai/screen_ai_ax_tree_serializer.h"
#include "sandbox/policy/switches.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/gfx/geometry/rect_f.h"

namespace screen_ai {

namespace {

base::FilePath GetLibraryFilePath() {
  base::FilePath library_path = GetStoredComponentBinaryPath();

  if (!library_path.empty())
    return library_path;

  // Binary file path is set while setting the sandbox on Linux, or the first
  // time this function is called. So in all other cases we need to look for
  // the library binary in its component folder.
  library_path = GetLatestComponentBinaryPath();
  StoreComponentBinaryPath(library_path);

  return library_path;
}

std::string MakeString(const char* content, uint32_t length) {
  DCHECK(content);
  DCHECK(length);

  std::string output;
  memcpy(base::WriteInto(&output, length + 1), content, length);
  output[length] = 0;
  return output;
}

}  // namespace

ScreenAIService::ScreenAIService(
    mojo::PendingReceiver<mojom::ScreenAIService> receiver)
    : library_(GetLibraryFilePath()),
      init_function_(
          reinterpret_cast<InitFunction>(library_.GetFunctionPointer("Init"))),
      annotate_function_(reinterpret_cast<AnnotateFunction>(
          library_.GetFunctionPointer("Annotate"))),
      extract_main_content_function_(
          reinterpret_cast<ExtractMainContentFunction>(
              library_.GetFunctionPointer("ExtractMainContent"))),
      receiver_(this, std::move(receiver)) {
  DCHECK(init_function_ && annotate_function_ &&
         extract_main_content_function_);
  if (!CallLibraryInitFunction()) {
    // TODO(https://crbug.com/1278249): Add UMA metrics to monitor failures.
    VLOG(0) << "Screen AI library initialization failed.";
    base::Process::TerminateCurrentProcessImmediately(-1);
  }
}

NO_SANITIZE("cfi-icall")
bool ScreenAIService::CallLibraryInitFunction() {
  return init_function_(
      /*init_visual_annotations = */ features::
              IsScreenAIVisualAnnotationsEnabled() ||
          features::IsPdfOcrEnabled(),
      /*init_main_content_extraction = */
      features::IsReadAnythingWithScreen2xEnabled(),
      /*debug_mode = */ features::IsScreenAIDebugModeEnabled(),
      /*models_path = */
      GetLibraryFilePath().DirName().MaybeAsASCII().c_str());
}

ScreenAIService::~ScreenAIService() = default;

void ScreenAIService::BindAnnotator(
    mojo::PendingReceiver<mojom::ScreenAIAnnotator> annotator) {
  screen_ai_annotators_.Add(this, std::move(annotator));
}

void ScreenAIService::BindAnnotatorClient(
    mojo::PendingRemote<mojom::ScreenAIAnnotatorClient> annotator_client) {
  DCHECK(!screen_ai_annotator_client_.is_bound());
  screen_ai_annotator_client_.Bind(std::move(annotator_client));
}

void ScreenAIService::BindMainContentExtractor(
    mojo::PendingReceiver<mojom::Screen2xMainContentExtractor>
        main_content_extractor) {
  screen_2x_main_content_extractors_.Add(this,
                                         std::move(main_content_extractor));
}

void ScreenAIService::Annotate(const SkBitmap& image,
                               AnnotationCallback callback) {
  DCHECK(screen_ai_annotator_client_.is_bound());

  ui::AXTreeUpdate update;

  VLOG(2) << "Screen AI library starting to process " << image.width() << "x"
          << image.height() << " snapshot.";

  char* annotation_proto = nullptr;
  uint32_t annotation_proto_length = 0;
  // TODO(https://crbug.com/1278249): Consider adding a signature that
  // verifies the data integrity and source.
  if (!CallLibraryAnnotateFunction(image, annotation_proto,
                                   annotation_proto_length)) {
    std::move(callback).Run(ui::AXTreeID());
    VLOG(1) << "Screen AI library could not process snapshot.";
    return;
  }

  // TODO(https://crbug.com/1278249): Create an AXTreeSource and send the ID
  // back to the caller.
  std::move(callback).Run(ui::AXTreeID());

  std::string proto_as_string =
      MakeString(annotation_proto, annotation_proto_length);
  delete annotation_proto;

  gfx::Rect image_rect(image.width(), image.height());
  update = ScreenAIVisualAnnotationToAXTreeUpdate(proto_as_string, image_rect);
  // TODO(nektar): Get the parent tree ID from the calling process (i.e.
  // browser or renderer).
  ScreenAIAXTreeSerializer serializer(
      /* parent_tree_id */ ui::AXTreeID::CreateNewAXTreeID(),
      std::move(update.nodes));
  update = serializer.Serialize();

  // ScreenAI service is created per profile and all clients are in the same
  // browser process. As the updates are passed to global AXTreeManager, it is
  // enough to sent it to only one client.
  // TODO(https://crbug.com/1278249): Call [client]::HandleAXTreeUpdate(update);
}

NO_SANITIZE("cfi-icall")
bool ScreenAIService::CallLibraryAnnotateFunction(
    const SkBitmap& image,
    char*& annotation_proto,
    uint32_t& annotation_proto_length) {
  return annotate_function_(image, annotation_proto, annotation_proto_length);
}

void ScreenAIService::ExtractMainContent(const ui::AXTreeUpdate& snapshot,
                                         ContentExtractionCallback callback) {
  std::string serialized_snapshot = Screen2xSnapshotToViewHierarchy(snapshot);
  std::vector<int32_t> content_node_ids;

  int32_t* node_ids = nullptr;
  uint32_t nodes_count = 0;
  if (!CallLibraryExtractMainContentFunction(serialized_snapshot.c_str(),
                                             serialized_snapshot.length(),
                                             node_ids, nodes_count)) {
    VLOG(1) << "Screen2x did not return main content.";
  } else {
    content_node_ids.resize(nodes_count);
    memcpy(content_node_ids.data(), node_ids, nodes_count * sizeof(int32_t));
    delete[] node_ids;
    node_ids = nullptr;

    VLOG(2) << "Screen2x returned " << content_node_ids.size() << " node ids:";
    for (int32_t i : content_node_ids)
      VLOG(2) << i;
  }

  std::move(callback).Run(content_node_ids);
}

NO_SANITIZE("cfi-icall")
bool ScreenAIService::CallLibraryExtractMainContentFunction(
    const char* serialized_snapshot,
    const uint32_t serialized_snapshot_length,
    int32_t*& node_ids,
    uint32_t& nodes_count) {
  return extract_main_content_function_(
      serialized_snapshot, serialized_snapshot_length, node_ids, nodes_count);
}

}  // namespace screen_ai
