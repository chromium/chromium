// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/screen_ai/screen_ai_service_impl.h"

#include <utility>

#include "base/check.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/notreached.h"
#include "base/process/process.h"
#include "base/scoped_native_library.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "components/services/screen_ai/proto/proto_convertor.h"
#include "components/services/screen_ai/public/cpp/utilities.h"
#include "components/services/screen_ai/screen_ai_ax_tree_serializer.h"
#include "sandbox/policy/switches.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/gfx/geometry/rect_f.h"

namespace screen_ai {

namespace {

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
    : receiver_(this, std::move(receiver)) {}

void ScreenAIService::LoadLibrary(const base::FilePath& library_path) {
  library_ = base::ScopedNativeLibrary(library_path);
  init_function_ =
      reinterpret_cast<InitFunction>(library_.GetFunctionPointer("Init"));
  extract_main_content_function_ = reinterpret_cast<ExtractMainContentFunction>(
      library_.GetFunctionPointer("ExtractMainContent"));
  DCHECK(init_function_ && extract_main_content_function_);
// TODO(https://crbug.com/1278249): Enable when ScreenAI is supported on
// Windows.
#if !BUILDFLAG(IS_WIN)
  annotate_function_ = reinterpret_cast<AnnotateFunction>(
      library_.GetFunctionPointer("Annotate"));
  DCHECK(annotate_function_);
#endif

  if (!CallLibraryInitFunction(library_path.DirName())) {
    // TODO(https://crbug.com/1278249): Add UMA metrics to monitor failures.
    VLOG(0) << "Screen AI library initialization failed.";
    base::Process::TerminateCurrentProcessImmediately(-1);
  }
}

NO_SANITIZE("cfi-icall")
bool ScreenAIService::CallLibraryInitFunction(
    const base::FilePath& models_path) {
  bool init_main_content_extraction =
      features::IsReadAnythingWithScreen2xEnabled();
  bool init_visual_annotations;
#if BUILDFLAG(IS_WIN)
  init_visual_annotations = false;
#else
  init_visual_annotations = features::IsScreenAIVisualAnnotationsEnabled() ||
                            features::IsPdfOcrEnabled();
#endif
  return init_function_(init_visual_annotations, init_main_content_extraction,
                        features::IsScreenAIDebugModeEnabled(),
                        models_path.MaybeAsASCII().c_str());
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
                               const ui::AXTreeID& parent_tree_id,
                               AnnotationCallback callback) {
  DCHECK(screen_ai_annotator_client_.is_bound());
  VLOG(2) << "Screen AI library starting to process " << image.width() << "x"
          << image.height() << " snapshot.";

  char* annotation_proto = nullptr;
  uint32_t annotation_proto_length = 0;
  // TODO(https://crbug.com/1278249): Consider adding a signature that
  // verifies the data integrity and source.
  if (!CallLibraryAnnotateFunction(image, annotation_proto,
                                   annotation_proto_length)) {
    std::move(callback).Run(ui::AXTreeIDUnknown());
    VLOG(1) << "Screen AI library could not process snapshot.";
    return;
  }

  std::string proto_as_string =
      MakeString(annotation_proto, annotation_proto_length);
  delete annotation_proto;

  gfx::Rect image_rect(image.width(), image.height());
  ui::AXTreeUpdate update =
      ScreenAIVisualAnnotationToAXTreeUpdate(proto_as_string, image_rect);
  ScreenAIAXTreeSerializer serializer(parent_tree_id, std::move(update.nodes));
  update = serializer.Serialize();

  const ui::AXTreeID& child_tree_id = update.tree_data.tree_id;
  // `ScreenAIAXTreeSerializer` should have assigned a new tree ID to `update`.
  // Thereby, it should never be an unknown tree ID, otherwise there has been an
  // unexpected serialization bug.
  DCHECK_NE(child_tree_id, ui::AXTreeIDUnknown()) << "Invalid serialization.\n"
                                                  << update.ToString();
  std::move(callback).Run(child_tree_id);

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
#if BUILDFLAG(IS_WIN)
  NOTIMPLEMENTED();
  return false;
#else
  DCHECK(annotate_function_);
  return annotate_function_(image, annotation_proto, annotation_proto_length);
#endif
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
  DCHECK(extract_main_content_function_);
  return extract_main_content_function_(
      serialized_snapshot, serialized_snapshot_length, node_ids, nodes_count);
}

}  // namespace screen_ai
