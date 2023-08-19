// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/screen_ai/screen_ai_service_impl.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/process/process.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "components/services/screen_ai/proto/main_content_extractor_proto_convertor.h"
#include "components/services/screen_ai/proto/visual_annotator_proto_convertor.h"
#include "components/services/screen_ai/public/cpp/utilities.h"
#include "components/services/screen_ai/screen_ai_ax_tree_serializer.h"
#include "content/public/browser/browser_thread.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/gfx/geometry/rect_f.h"

namespace screen_ai {

namespace {

// Returns an empty result if load fails.
std::unique_ptr<ScreenAILibraryWrapper> LoadLibrary(
    const base::FilePath& library_path) {
  DCHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  std::unique_ptr<ScreenAILibraryWrapper> library =
      std::make_unique<ScreenAILibraryWrapper>();

  bool load_sucessful = library->Load(library_path);
  base::UmaHistogramBoolean("Accessibility.ScreenAI.Library.Initialized",
                            load_sucessful);

  if (!load_sucessful) {
    library.reset();
    return library;
  }

  uint32_t version_major;
  uint32_t version_minor;
  library->GetLibraryVersion(version_major, version_minor);
  VLOG(2) << "Screen AI library version: " << version_major << "."
          << version_minor;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  library->SetLogger();
#endif

  if (features::IsScreenAIDebugModeEnabled()) {
    library->EnableDebugMode();
  }

  return library;
}

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

std::unique_ptr<
    screen_ai::ScreenAILibraryWrapper::MainContentExtractionModelData>
LoadMainContentExtractionModelFiles(base::File& model_config_file,
                                    base::File& model_tflite_file) {
  auto model_data = std::make_unique<
      screen_ai::ScreenAILibraryWrapper::MainContentExtractionModelData>(
      LoadModelFile(model_config_file), LoadModelFile(model_tflite_file));
  return model_data;
}

ui::AXTreeUpdate ConvertVisualAnnotationToTreeUpdate(
    const absl::optional<chrome_screen_ai::VisualAnnotation>& annotation_proto,
    const gfx::Rect& image_rect) {
  if (!annotation_proto) {
    VLOG(0) << "Screen AI library could not process snapshot or no OCR data.";
    return ui::AXTreeUpdate();
  }

  return VisualAnnotationToAXTreeUpdate(*annotation_proto, image_rect);
}

}  // namespace

ScreenAIService::ScreenAIService(
    mojo::PendingReceiver<mojom::ScreenAIServiceFactory> receiver)
    : factory_receiver_(this, std::move(receiver)),
      ocr_receiver_(this),
      main_content_extraction_receiver_(this) {}

ScreenAIService::~ScreenAIService() = default;

void ScreenAIService::InitializeMainContentExtraction(
    base::File model_config,
    base::File model_tflite,
    const base::FilePath& library_path,
    mojo::PendingReceiver<mojom::MainContentExtractionService>
        main_content_extractor_service_receiver,
    InitializeMainContentExtractionCallback callback) {
  if (!library_) {
    library_ = LoadLibrary(library_path);
  }

  if (!library_) {
    std::move(callback).Run(false);
    base::Process::TerminateCurrentProcessImmediately(-1);
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&LoadMainContentExtractionModelFiles,
                     base::OwnedRef(std::move(model_config)),
                     base::OwnedRef(std::move(model_tflite))),
      base::BindOnce(&ScreenAIService::InitializeMainContentExtractionInternal,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(main_content_extractor_service_receiver),
                     std::move(callback)));
}

void ScreenAIService::InitializeMainContentExtractionInternal(
    mojo::PendingReceiver<mojom::MainContentExtractionService>
        main_content_extractor_service_receiver,
    InitializeMainContentExtractionCallback callback,
    std::unique_ptr<ScreenAILibraryWrapper::MainContentExtractionModelData>
        model_data) {
  bool init_successful = library_->InitMainContentExtraction(*model_data);
  base::UmaHistogramBoolean(
      "Accessibility.ScreenAI.MainContentExtraction.Initialized",
      init_successful);
  if (!init_successful) {
    std::move(callback).Run(false);
    return;
  }

  // This interface should be created only once.
  CHECK(!main_content_extraction_receiver_.is_bound());

  main_content_extraction_receiver_.Bind(
      std::move(main_content_extractor_service_receiver));

  std::move(callback).Run(true);
}

void ScreenAIService::InitializeOCR(
    const base::FilePath& library_path,
    mojo::PendingReceiver<mojom::OCRService> ocr_service_receiver,
    InitializeOCRCallback callback) {
  if (!library_) {
    library_ = LoadLibrary(library_path);
  }

  if (!library_) {
    std::move(callback).Run(false);
    base::Process::TerminateCurrentProcessImmediately(-1);
  }

  bool init_successful = library_->InitOCR(library_path.DirName());
  base::UmaHistogramBoolean("Accessibility.ScreenAI.OCR.Initialized",
                            init_successful);

  // TODO(crbug.com/1443349): Add a separate initialization interface for
  // layout extraction.
  if (features::IsLayoutExtractionEnabled()) {
    if (!library_->InitLayoutExtraction()) {
      VLOG(0) << "Could not initialize layout extraction.";
    }
  }

  if (!init_successful) {
    std::move(callback).Run(false);
    return;
  }

  // This interface should be created only once.
  CHECK(!ocr_receiver_.is_bound());

  ocr_receiver_.Bind(std::move(ocr_service_receiver));

  std::move(callback).Run(true);
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

void ScreenAIService::ExtractSemanticLayout(
    const SkBitmap& image,
    const ui::AXTreeID& parent_tree_id,
    ExtractSemanticLayoutCallback callback) {
  DCHECK(screen_ai_annotator_client_.is_bound());

  absl::optional<chrome_screen_ai::VisualAnnotation> annotation_proto =
      library_->ExtractLayout(image);

  // The original caller is always replied to, and an AXTreeIDUnknown is sent to
  // tell it that the annotation function was not successful. However the client
  // is only contacted for successful runs and when we have an update.
  if (!annotation_proto) {
    VLOG(0) << "Layout Extraction failed. ";
    std::move(callback).Run(ui::AXTreeIDUnknown());
    return;
  }

  ui::AXTreeUpdate update = ConvertVisualAnnotationToTreeUpdate(
      annotation_proto, gfx::Rect(image.width(), image.height()));
  VLOG(1) << "Layout Extraction returned " << update.nodes.size() << " nodes.";

  // Convert `update` to a properly serialized `AXTreeUpdate`.
  ScreenAIAXTreeSerializer serializer(parent_tree_id, std::move(update.nodes));
  update = serializer.Serialize();

  // `ScreenAIAXTreeSerializer` should have assigned a new tree ID to `update`.
  // Thereby, it should never be an unknown tree ID, otherwise there has been an
  // unexpected serialization bug.
  DCHECK_NE(update.tree_data.tree_id, ui::AXTreeIDUnknown())
      << "Invalid serialization.\n"
      << update.ToString();
  std::move(callback).Run(update.tree_data.tree_id);
  screen_ai_annotator_client_->HandleAXTreeUpdate(update);
}

absl::optional<chrome_screen_ai::VisualAnnotation>
ScreenAIService::PerformOcrAndRecordMetrics(const SkBitmap& image) {
  base::TimeTicks start_time = base::TimeTicks::Now();
  auto result = library_->PerformOcr(image);
  base::TimeDelta elapsed_time = base::TimeTicks::Now() - start_time;

  base::UmaHistogramTimes("Accessibility.ScreenAI.OCR.Time", elapsed_time);
  base::UmaHistogramCounts10M("Accessibility.ScreenAI.OCR.ImageSize10M",
                              image.width() * image.height());
  return result;
}

void ScreenAIService::PerformOcrAndReturnAnnotation(
    const SkBitmap& image,
    PerformOcrAndReturnAnnotationCallback callback) {
  absl::optional<chrome_screen_ai::VisualAnnotation> annotation_proto =
      PerformOcrAndRecordMetrics(image);

  if (annotation_proto) {
    std::move(callback).Run(ConvertProtoToVisualAnnotation(*annotation_proto));
    return;
  }

  std::move(callback).Run(mojom::VisualAnnotation::New());
}

void ScreenAIService::PerformOcrAndReturnAXTreeUpdate(
    const SkBitmap& image,
    PerformOcrAndReturnAXTreeUpdateCallback callback) {
  absl::optional<chrome_screen_ai::VisualAnnotation> annotation_proto =
      PerformOcrAndRecordMetrics(image);
  ui::AXTreeUpdate update = ConvertVisualAnnotationToTreeUpdate(
      annotation_proto, gfx::Rect(image.width(), image.height()));
  VLOG(1) << "OCR returned " << update.nodes.size() << " nodes.";

  // The original caller is always replied to, and an empty AXTreeUpdate tells
  // that the annotation function was not successful.
  std::move(callback).Run(update);

  // TODO(crbug.com/1434701): Send the AXTreeUpdate to the browser
  // side client for Backlight.
}

void ScreenAIService::ExtractMainContent(const ui::AXTreeUpdate& snapshot,
                                         ukm::SourceId ukm_source_id,
                                         ExtractMainContentCallback callback) {
  // Early return if input is empty.
  if (snapshot.nodes.empty()) {
    std::move(callback).Run(std::vector<int32_t>());
    return;
  }

  std::string serialized_snapshot = SnapshotToViewHierarchy(snapshot);

  base::TimeTicks start_time = base::TimeTicks::Now();
  absl::optional<std::vector<int32_t>> content_node_ids =
      library_->ExtractMainContent(serialized_snapshot);
  base::TimeDelta elapsed_time = base::TimeTicks::Now() - start_time;

  RecordMetrics(ukm_source_id, ukm::UkmRecorder::Get(), elapsed_time,
                /* success= */ content_node_ids.has_value());

  if (content_node_ids.has_value()) {
    VLOG(2) << "Screen2x returned " << content_node_ids->size() << " node ids.";
    std::move(callback).Run(*content_node_ids);
    return;
  }

  VLOG(0) << "Screen2x returned no results.";
  std::move(callback).Run(std::vector<int32_t>());
}

// static
void ScreenAIService::RecordMetrics(ukm::SourceId ukm_source_id,
                                    ukm::UkmRecorder* ukm_recorder,
                                    base::TimeDelta elapsed_time,
                                    bool success) {
  if (success) {
    base::UmaHistogramTimes(
        "Accessibility.ScreenAI.Screen2xDistillationTime.Success",
        elapsed_time);
    if (ukm_source_id != ukm::kInvalidSourceId) {
      ukm::builders::Accessibility_ScreenAI(ukm_source_id)
          .SetScreen2xDistillationTime_Success(elapsed_time.InMilliseconds())
          .Record(ukm_recorder);
    }
  } else {
    base::UmaHistogramTimes(
        "Accessibility.ScreenAI.Screen2xDistillationTime.Failure",
        elapsed_time);
    if (ukm_source_id != ukm::kInvalidSourceId) {
      ukm::builders::Accessibility_ScreenAI(ukm_source_id)
          .SetScreen2xDistillationTime_Failure(elapsed_time.InMilliseconds())
          .Record(ukm_recorder);
    }
  }
}

}  // namespace screen_ai
