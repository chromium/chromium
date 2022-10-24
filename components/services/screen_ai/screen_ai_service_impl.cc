// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/screen_ai/screen_ai_service_impl.h"

#include <utility>
#include <vector>

#include "base/check.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/notreached.h"
#include "base/process/process.h"
#include "base/scoped_native_library.h"
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

std::vector<char> LoadModelFile(base::File& model_file) {
  std::vector<char> buffer;
  int64_t length = model_file.GetLength();
  if (length < 0) {
    VLOG(0) << "Could not query Screen AI model file's length.";
    return buffer;
  }

  buffer.resize(length);
  if (model_file.Read(0, buffer.data(), length) != length) {
    buffer.clear();
    VLOG(0) << "Could not read Screen AI model file's content.";
  }

  return buffer;
}

}  // namespace

void LibraryFunctions::LoadFunctions(base::ScopedNativeLibrary& library) {
  // General functions.
  get_library_version_ = reinterpret_cast<GetLibraryVersion>(
      library.GetFunctionPointer("GetLibraryVersion"));
  DCHECK(get_library_version_);
  enable_debug_mode_ = reinterpret_cast<EnableDebugMode>(
      library.GetFunctionPointer("EnableDebugMode"));
  DCHECK(enable_debug_mode_);

  // Main Content Extraction functions.
  init_main_content_extraction_ = reinterpret_cast<InitMainContentExtraction>(
      library.GetFunctionPointer("InitMainContentExtraction"));
  DCHECK(init_main_content_extraction_);
  extract_main_content_ = reinterpret_cast<ExtractMainContent>(
      library.GetFunctionPointer("ExtractMainContent"));
  DCHECK(extract_main_content_);

// Visual Annotation functions.
// TODO(https://crbug.com/1278249): Enable when ScreenAI is supported on
// Windows.
#if !BUILDFLAG(IS_WIN)
  init_visual_annotation_ = reinterpret_cast<InitVisualAnnotations>(
      library.GetFunctionPointer("InitVisualAnnotations"));
  DCHECK(init_visual_annotation_);
  annotate_ =
      reinterpret_cast<Annotate>(library.GetFunctionPointer("Annotate"));
  DCHECK(annotate_);
#endif  // !BUILDFLAG(IS_WIN)
}

ScreenAIService::ScreenAIService(
    mojo::PendingReceiver<mojom::ScreenAIService> receiver)
    : receiver_(this, std::move(receiver)) {}

void ScreenAIService::LoadLibrary(base::File model_config,
                                  base::File model_tflite,
                                  const base::FilePath& library_path) {
  library_ = base::ScopedNativeLibrary(library_path);
  library_functions_.LoadFunctions(library_);
  if (features::IsScreenAIDebugModeEnabled())
    CallEnableDebugMode();
  bool init_ok = true;
#if !BUILDFLAG(IS_WIN)
  if (features::IsPdfOcrEnabled() ||
      features::IsScreenAIVisualAnnotationsEnabled()) {
    if (!CallInitVisualAnnotationsFunction(library_path.DirName())) {
      init_ok = false;
      // TODO(https://crbug.com/1278249): Add UMA metrics to monitor failures.
    }
  }
#endif

  if (init_ok && features::IsReadAnythingWithScreen2xEnabled()) {
    if (!CallInitMainContentExtractionFunction(model_config, model_tflite)) {
      // TODO(https://crbug.com/1278249): Add UMA metrics to monitor failures.
      init_ok = false;
    }
  }

  if (!init_ok) {
    VLOG(0) << "Screen AI library initialization failed.";
    base::Process::TerminateCurrentProcessImmediately(-1);
  }
}

ScreenAIService::~ScreenAIService() = default;

NO_SANITIZE("cfi-icall")
bool ScreenAIService::CallInitVisualAnnotationsFunction(
    const base::FilePath& models_folder) {
#if BUILDFLAG(IS_WIN)
  return false;
#else
  return library_functions_.init_visual_annotation_(
      models_folder.MaybeAsASCII().c_str());
#endif
}

NO_SANITIZE("cfi-icall")
bool ScreenAIService::CallInitMainContentExtractionFunction(
    base::File& model_config_file,
    base::File& model_tflite_file) {
  std::vector<char> model_config = LoadModelFile(model_config_file);
  std::vector<char> model_tflite = LoadModelFile(model_tflite_file);
  if (model_config.empty() || model_tflite.empty())
    return false;

  return library_functions_.init_main_content_extraction_(
      model_config.data(), model_config.size(), model_tflite.data(),
      model_tflite.size());
}

NO_SANITIZE("cfi-icall")
void ScreenAIService::CallEnableDebugMode() {
  library_functions_.enable_debug_mode_();
}

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

  std::string proto_as_string;
  proto_as_string.assign(annotation_proto, annotation_proto_length);
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
  DCHECK(library_functions_.annotate_);
  return library_functions_.annotate_(image, annotation_proto,
                                      annotation_proto_length);
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
  DCHECK(library_functions_.extract_main_content_);
  return library_functions_.extract_main_content_(
      serialized_snapshot, serialized_snapshot_length, node_ids, nodes_count);
}

}  // namespace screen_ai
