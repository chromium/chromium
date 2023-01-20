// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/screen_ai/screen_ai_service_impl.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/process/process.h"
#include "base/scoped_native_library.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "components/services/screen_ai/proto/main_content_extractor_proto_convertor.h"
#include "components/services/screen_ai/proto/visual_annotator_proto_convertor.h"
#include "components/services/screen_ai/public/cpp/utilities.h"
#include "components/services/screen_ai/screen_ai_ax_tree_serializer.h"
#include "content/public/browser/browser_thread.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/gfx/geometry/rect_f.h"

namespace screen_ai {

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ScreenAILoadLibraryResult {
  kAllOk = 0,
  kDeprecatedVisualAnnotationFailed = 1,
  kMainContentExtractionFailed = 2,
  kLayoutExtractionFailed = 3,
  kOcrFailed = 4,
  kMaxValue = kOcrFailed,
};

#if BUILDFLAG(IS_CHROMEOS_ASH)
void HandleLibraryLogging(int severity, const char* message) {
  switch (severity) {
    case logging::LOG_VERBOSE:
    case logging::LOG_INFO:
      VLOG(2) << message;
      break;
    case logging::LOG_WARNING:
      VLOG(1) << message;
      break;
    case logging::LOG_ERROR:
    case logging::LOG_FATAL:
      VLOG(0) << message;
      break;
  }
}
#endif

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

NO_SANITIZE("cfi-icall")
void CallGetLibraryVersionFunction(LibraryFunctions* library_functions,
                                   uint32_t& major,
                                   uint32_t& minor) {
  DCHECK(library_functions);
  DCHECK(library_functions->get_library_version_);

  library_functions->get_library_version_(major, minor);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
NO_SANITIZE("cfi-icall")
void CallSetLoggerFunction(LibraryFunctions* library_functions) {
  DCHECK(library_functions);
  DCHECK(library_functions->set_logger_);
  library_functions->set_logger_(&HandleLibraryLogging);
}
#endif

NO_SANITIZE("cfi-icall")
bool CallInitLayoutExtractionFunction(LibraryFunctions* library_functions,
                                      const base::FilePath& models_folder) {
  DCHECK(library_functions);
  DCHECK(library_functions->init_layout_extraction_);
  return library_functions->init_layout_extraction_(
      models_folder.MaybeAsASCII().c_str());
}

NO_SANITIZE("cfi-icall")
bool CallInitOCRFunction(LibraryFunctions* library_functions,
                         const base::FilePath& models_folder) {
  DCHECK(library_functions);
  DCHECK(library_functions->init_ocr_);
  return library_functions->init_ocr_(models_folder.MaybeAsASCII().c_str());
}

NO_SANITIZE("cfi-icall")
bool CallInitMainContentExtractionFunction(LibraryFunctions* library_functions,
                                           base::File& model_config_file,
                                           base::File& model_tflite_file) {
  DCHECK(library_functions);
  DCHECK(library_functions->init_main_content_extraction_);
  std::vector<char> model_config = LoadModelFile(model_config_file);
  std::vector<char> model_tflite = LoadModelFile(model_tflite_file);
  if (model_config.empty() || model_tflite.empty())
    return false;

  return library_functions->init_main_content_extraction_(
      model_config.data(), model_config.size(), model_tflite.data(),
      model_tflite.size());
}

NO_SANITIZE("cfi-icall")
void CallEnableDebugMode(LibraryFunctions* library_functions) {
  library_functions->enable_debug_mode_();
}

std::unique_ptr<LibraryFunctions> LoadAndInitializeLibrary(
    base::File model_config,
    base::File model_tflite,
    const base::FilePath& library_path) {
  DCHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  std::unique_ptr<LibraryFunctions> library_functions =
      std::make_unique<LibraryFunctions>(library_path);

  uint32_t version_major;
  uint32_t version_minor;
  CallGetLibraryVersionFunction(library_functions.get(), version_major,
                                version_minor);
  VLOG(2) << "Screen AI library version: " << version_major << "."
          << version_minor;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  CallSetLoggerFunction(library_functions.get());
#endif

  if (features::IsScreenAIDebugModeEnabled())
    CallEnableDebugMode(library_functions.get());

  bool init_ok = true;
  if (features::IsPdfOcrEnabled()) {
    if (!CallInitOCRFunction(library_functions.get(), library_path.DirName())) {
      init_ok = false;
      base::UmaHistogramEnumeration("Accessibility.ScreenAI.LoadLibraryResult",
                                    ScreenAILoadLibraryResult::kOcrFailed);
    }
  }

  if (init_ok && features::IsLayoutExtractionEnabled()) {
    if (!CallInitLayoutExtractionFunction(library_functions.get(),
                                          library_path.DirName())) {
      init_ok = false;
      base::UmaHistogramEnumeration(
          "Accessibility.ScreenAI.LoadLibraryResult",
          ScreenAILoadLibraryResult::kLayoutExtractionFailed);
    }
  }

  if (init_ok && features::IsReadAnythingWithScreen2xEnabled()) {
    if (!CallInitMainContentExtractionFunction(library_functions.get(),
                                               model_config, model_tflite)) {
      init_ok = false;
      base::UmaHistogramEnumeration(
          "Accessibility.ScreenAI.LoadLibraryResult",
          ScreenAILoadLibraryResult::kMainContentExtractionFailed);
    }
  }

  if (!init_ok) {
    VLOG(0) << "Screen AI library initialization failed.";
    base::Process::TerminateCurrentProcessImmediately(-1);
  }

  base::UmaHistogramEnumeration("Accessibility.ScreenAI.LoadLibraryResult",
                                ScreenAILoadLibraryResult::kAllOk);

  return library_functions;
}

}  // namespace

ScreenAIService::ScreenAIService(
    mojo::PendingReceiver<mojom::ScreenAIService> receiver)
    : task_runner_(new base::DeferredSequencedTaskRunner(
          base::SingleThreadTaskRunner::GetCurrentDefault())),
      receiver_(this, std::move(receiver)) {}

ScreenAIService::~ScreenAIService() = default;

LibraryFunctions::LibraryFunctions(const base::FilePath& library_path) {
  library_ = base::ScopedNativeLibrary(library_path);
  DCHECK(library_.GetError());
#if BUILDFLAG(IS_WIN)
  DCHECK_EQ(0u, library_.GetError()->code)
      << "Library load error: " << library_.GetError()->code;
#else
  DCHECK(library_.GetError()->message.empty())
      << "Library load error: " << library_.GetError()->message;
#endif

  // General functions.
  get_library_version_ = reinterpret_cast<GetLibraryVersionFn>(
      library_.GetFunctionPointer("GetLibraryVersion"));
  DCHECK(get_library_version_);
  enable_debug_mode_ = reinterpret_cast<EnableDebugModeFn>(
      library_.GetFunctionPointer("EnableDebugMode"));
  DCHECK(enable_debug_mode_);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  set_logger_ =
      reinterpret_cast<SetLoggerFn>(library_.GetFunctionPointer("SetLogger"));
  DCHECK(set_logger_);
#endif

  // Main Content Extraction functions.
  if (features::IsReadAnythingWithScreen2xEnabled()) {
    init_main_content_extraction_ =
        reinterpret_cast<InitMainContentExtractionFn>(
            library_.GetFunctionPointer("InitMainContentExtraction"));
    DCHECK(init_main_content_extraction_);
    extract_main_content_ = reinterpret_cast<ExtractMainContentFn>(
        library_.GetFunctionPointer("ExtractMainContent"));
    DCHECK(extract_main_content_);
  }

  // Layout Extraction.
  if (features::IsLayoutExtractionEnabled()) {
    init_layout_extraction_ = reinterpret_cast<InitLayoutExtractionFn>(
        library_.GetFunctionPointer("InitLayoutExtraction"));
    DCHECK(init_layout_extraction_);
    extract_layout_ = reinterpret_cast<ExtractLayoutFn>(
        library_.GetFunctionPointer("ExtractLayout"));
    DCHECK(extract_layout_);
  }

  // OCR.
  if (features::IsPdfOcrEnabled()) {
    init_ocr_ =
        reinterpret_cast<InitOCRFn>(library_.GetFunctionPointer("InitOCR"));
    DCHECK(init_ocr_);
    perform_ocr_ = reinterpret_cast<PerformOcrFn>(
        library_.GetFunctionPointer("PerformOCR"));
    DCHECK(perform_ocr_);
  }
}

void ScreenAIService::LoadLibrary(base::File model_config,
                                  base::File model_tflite,
                                  const base::FilePath& library_path) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&LoadAndInitializeLibrary, std::move(model_config),
                     std::move(model_tflite), library_path),
      base::BindOnce(&ScreenAIService::SetLibraryFunctions,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ScreenAIService::SetLibraryFunctions(
    std::unique_ptr<LibraryFunctions> library_functions) {
  library_functions_ = std::move(library_functions);
  task_runner_->Start();
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

void ScreenAIService::PerformVisualAnnotation(
    const SkBitmap& image,
    const ui::AXTreeID& parent_tree_id,
    AnnotationCallback callback,
    bool run_ocr,
    bool run_layout_extraction) {
  std::unique_ptr<ui::AXTreeUpdate> annotation =
      std::make_unique<ui::AXTreeUpdate>();
  ui::AXTreeUpdate* annotation_ptr = annotation.get();

  // Ownership of |annotation| is passed to the reply function, and hence it is
  // destroyed after the task function is called, or both functions get
  // cancelled, so it's safe to pass |annotation_ptr| unretained.
  // We need to get the pointer beforehand since compiler optimizations may
  // result in binding the reply function (and moving |annotation|) before
  // binding the task.
  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&ScreenAIService::VisualAnnotationInternal,
                     weak_ptr_factory_.GetWeakPtr(), std::move(image),
                     std::move(parent_tree_id), run_ocr, run_layout_extraction,
                     base::Unretained(annotation_ptr)),
      base::BindOnce(
          [](mojo::Remote<mojom::ScreenAIAnnotatorClient>* client,
             AnnotationCallback callback,
             std::unique_ptr<ui::AXTreeUpdate> update) {
            // The original caller is always replied to, and an AXTreeIDUnknown
            // is sent to tell it that the annotation function was not
            // successful. However the client is only contacted for successful
            // runs and when we have an update.
            std::move(callback).Run(update->tree_data.tree_id);
            if (update->tree_data.tree_id != ui::AXTreeIDUnknown())
              (*client)->HandleAXTreeUpdate(*update);
          },
          &screen_ai_annotator_client_, std::move(callback),
          std::move(annotation)));
}

void ScreenAIService::ExtractSemanticLayout(const SkBitmap& image,
                                            const ui::AXTreeID& parent_tree_id,
                                            AnnotationCallback callback) {
  PerformVisualAnnotation(std::move(image), parent_tree_id, std::move(callback),
                          /*run_ocr=*/false,
                          /*run_layout_extraction=*/true);
}

void ScreenAIService::PerformOcr(const SkBitmap& image,
                                 const ui::AXTreeID& parent_tree_id,
                                 AnnotationCallback callback) {
  PerformVisualAnnotation(std::move(image), parent_tree_id, std::move(callback),
                          /*run_ocr=*/true,
                          /*run_layout_extraction=*/false);
}

void ScreenAIService::VisualAnnotationInternal(
    const SkBitmap& image,
    const ui::AXTreeID& parent_tree_id,
    bool run_ocr,
    bool run_layout_extraction,
    ui::AXTreeUpdate* annotation) {
  // Currently we only support either of OCR or LayoutExtraction features.
  DCHECK_NE(run_ocr, run_layout_extraction);
  DCHECK(screen_ai_annotator_client_.is_bound());

  char* annotation_proto = nullptr;
  uint32_t annotation_proto_length = 0;
  // TODO(https://crbug.com/1278249): Consider adding a signature that
  // verifies the data integrity and source.
  bool result = false;
  if (run_ocr) {
    result = CallLibraryOcrFunction(image, annotation_proto,
                                    annotation_proto_length);
  } else /* if (run_layout_extraction) */ {
    result = CallLibraryLayoutExtractionFunction(image, annotation_proto,
                                                 annotation_proto_length);
  }
  if (!result || !annotation_proto_length) {
    DCHECK_EQ(annotation->tree_data.tree_id, ui::AXTreeIDUnknown());
    VLOG(1) << "Screen AI library could not process snapshot or no OCR data.";
    return;
  }

  std::string proto_as_string;
  proto_as_string.assign(annotation_proto, annotation_proto_length);
  delete annotation_proto;

  gfx::Rect image_rect(image.width(), image.height());
  *annotation = VisualAnnotationToAXTreeUpdate(proto_as_string, image_rect);
  ScreenAIAXTreeSerializer serializer(parent_tree_id,
                                      std::move(annotation->nodes));
  *annotation = serializer.Serialize();

  // `ScreenAIAXTreeSerializer` should have assigned a new tree ID to `update`.
  // Thereby, it should never be an unknown tree ID, otherwise there has been an
  // unexpected serialization bug.
  DCHECK_NE(annotation->tree_data.tree_id, ui::AXTreeIDUnknown())
      << "Invalid serialization.\n"
      << annotation->ToString();
}

NO_SANITIZE("cfi-icall")
bool ScreenAIService::CallLibraryOcrFunction(
    const SkBitmap& image,
    char*& annotation_proto,
    uint32_t& annotation_proto_length) {
  DCHECK(library_functions_);
  DCHECK(library_functions_->perform_ocr_);
  return library_functions_->perform_ocr_(image, annotation_proto,
                                          annotation_proto_length);
}

NO_SANITIZE("cfi-icall")
bool ScreenAIService::CallLibraryLayoutExtractionFunction(
    const SkBitmap& image,
    char*& annotation_proto,
    uint32_t& annotation_proto_length) {
  DCHECK(library_functions_);
  DCHECK(library_functions_->extract_layout_);
  return library_functions_->extract_layout_(image, annotation_proto,
                                             annotation_proto_length);
}

void ScreenAIService::ExtractMainContent(const ui::AXTreeUpdate& snapshot,
                                         ContentExtractionCallback callback) {
  std::unique_ptr<std::vector<int32_t>> content_node_ids =
      std::make_unique<std::vector<int32_t>>();
  std::vector<int32_t>* node_ids_ptr = content_node_ids.get();

  // Ownership of |content_node_ids| is passed to the reply function, so it's
  // safe to pass an unretained pointer to the task function.
  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&ScreenAIService::ExtractMainContentInternal,
                     weak_ptr_factory_.GetWeakPtr(), std::move(snapshot),
                     base::Unretained(node_ids_ptr)),
      base::BindOnce(
          [](ContentExtractionCallback callback,
             std::unique_ptr<std::vector<int32_t>> content_node_ids) {
            std::move(callback).Run(*content_node_ids);
          },
          std::move(callback), std::move(content_node_ids)));
}

void ScreenAIService::ExtractMainContentInternal(
    const ui::AXTreeUpdate& snapshot,
    std::vector<int32_t>* content_node_ids) {
  DCHECK(content_node_ids);
  std::string serialized_snapshot = SnapshotToViewHierarchy(snapshot);

  int32_t* node_ids = nullptr;
  uint32_t nodes_count = 0;
  base::TimeTicks start_time = base::TimeTicks::Now();
  bool success = CallLibraryExtractMainContentFunction(
      serialized_snapshot.c_str(), serialized_snapshot.length(), node_ids,
      nodes_count);
  base::TimeDelta elapsed_time = base::TimeTicks::Now() - start_time;
  if (!success) {
    VLOG(1) << "Screen2x did not return main content.";
    DCHECK(content_node_ids->empty());
    base::UmaHistogramTimes(
        "Accessibility.ScreenAI.Screen2xDistillationTime.Failure",
        elapsed_time);
    return;
  }

  content_node_ids->resize(nodes_count);
  memcpy(content_node_ids->data(), node_ids, nodes_count * sizeof(int32_t));
  delete[] node_ids;
  node_ids = nullptr;

  VLOG(2) << "Screen2x returned " << content_node_ids->size() << " node ids:";
  for (int32_t i : *content_node_ids)
    VLOG(2) << i;
  base::UmaHistogramTimes(
      "Accessibility.ScreenAI.Screen2xDistillationTime.Success", elapsed_time);
}

NO_SANITIZE("cfi-icall")
bool ScreenAIService::CallLibraryExtractMainContentFunction(
    const char* serialized_snapshot,
    const uint32_t serialized_snapshot_length,
    int32_t*& node_ids,
    uint32_t& nodes_count) {
  DCHECK(library_functions_);
  DCHECK(library_functions_->extract_main_content_);
  return library_functions_->extract_main_content_(
      serialized_snapshot, serialized_snapshot_length, node_ids, nodes_count);
}

}  // namespace screen_ai
