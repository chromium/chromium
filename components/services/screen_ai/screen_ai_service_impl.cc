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

// The only instance of `PreloadedModelData`, valid through the call to any of
// the library initialization functions.
PreloadedModelData* g_preloaded_model_data_instance = nullptr;

// Keeps the content of model files, and replies to calls for copying them.
class PreloadedModelData {
 public:
  PreloadedModelData(const PreloadedModelData&) = delete;
  PreloadedModelData& operator=(const PreloadedModelData&) = delete;
  ~PreloadedModelData() {
    CHECK_NE(g_preloaded_model_data_instance, nullptr);
    g_preloaded_model_data_instance = nullptr;
  }

  static std::unique_ptr<PreloadedModelData> Create(
      base::flat_map<base::FilePath, base::File> model_files) {
    return base::WrapUnique<PreloadedModelData>(
        new PreloadedModelData(std::move(model_files)));
  }

  // Returns 0 if file is not found.
  static uint32_t GetDataSize(const char* relative_file_path) {
    CHECK(g_preloaded_model_data_instance);
    return base::Contains(g_preloaded_model_data_instance->data_,
                          relative_file_path)
               ? g_preloaded_model_data_instance->data_[relative_file_path]
                     .size()
               : 0;
  }

  // Assumes that `buffer` has enough size.
  static void CopyData(const char* relative_file_path,
                       uint32_t buffer_size,
                       char* buffer) {
    CHECK(g_preloaded_model_data_instance);
    CHECK(base::Contains(g_preloaded_model_data_instance->data_,
                         relative_file_path));
    const std::vector<char>& data =
        g_preloaded_model_data_instance->data_[relative_file_path];
    CHECK_GE(buffer_size, data.size());
    memcpy(buffer, data.data(), data.size());
  }

  std::map<std::string, std::vector<char>> data_;

 private:
  explicit PreloadedModelData(
      base::flat_map<base::FilePath, base::File> model_files) {
    CHECK_EQ(g_preloaded_model_data_instance, nullptr);
    g_preloaded_model_data_instance = this;

    for (auto& model_file : model_files) {
      std::vector<char> buffer;
      int64_t length = model_file.second.GetLength();
      if (length < 0) {
        VLOG(0) << "Could not query Screen AI model file's length: "
                << model_file.first;
        continue;
      }

      buffer.resize(length);
      if (model_file.second.Read(0, buffer.data(), length) != length) {
        VLOG(0) << "Could not read Screen AI model file's content: "
                << model_file.first;
        continue;
      }
      data_[model_file.first.MaybeAsASCII()] = std::move(buffer);
    }
  }
};

ScreenAIService::ScreenAIService(
    mojo::PendingReceiver<mojom::ScreenAIServiceFactory> receiver)
    : factory_receiver_(this, std::move(receiver)),
      ocr_receiver_(this),
      main_content_extraction_receiver_(this) {}

ScreenAIService::~ScreenAIService() = default;

void ScreenAIService::LoadLibrary(const base::FilePath& library_path) {
  DCHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  library_ = std::make_unique<ScreenAILibraryWrapper>();

  bool load_sucessful = library_->Load(library_path);
  base::UmaHistogramBoolean("Accessibility.ScreenAI.Library.Initialized",
                            load_sucessful);

  if (!load_sucessful) {
    library_.reset();
    return;
  }

  uint32_t version_major;
  uint32_t version_minor;
  library_->GetLibraryVersion(version_major, version_minor);
  VLOG(2) << "Screen AI library version: " << version_major << "."
          << version_minor;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  library_->SetLogger();
#endif

  if (features::IsScreenAIDebugModeEnabled()) {
    library_->EnableDebugMode();
  }

  library_->SetFileContentFunctions(&PreloadedModelData::GetDataSize,
                                    &PreloadedModelData::CopyData);
}

void ScreenAIService::InitializeMainContentExtraction(
    const base::FilePath& library_path,
    base::flat_map<base::FilePath, base::File> model_files,
    mojo::PendingReceiver<mojom::MainContentExtractionService>
        main_content_extractor_service_receiver,
    InitializeMainContentExtractionCallback callback) {
  if (!library_) {
    LoadLibrary(library_path);
  }

  if (!library_) {
    std::move(callback).Run(false);
    base::Process::TerminateCurrentProcessImmediately(-1);
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&PreloadedModelData::Create, std::move(model_files)),
      base::BindOnce(&ScreenAIService::InitializeMainContentExtractionInternal,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(main_content_extractor_service_receiver),
                     std::move(callback)));
}

void ScreenAIService::InitializeMainContentExtractionInternal(
    mojo::PendingReceiver<mojom::MainContentExtractionService>
        main_content_extractor_service_receiver,
    InitializeMainContentExtractionCallback callback,
    std::unique_ptr<PreloadedModelData> model_data) {
  // `model_data` contains the content of the model files and its accessors are
  // passed to the library. It should be kept in memory until after library
  // initialization.
  bool init_successful = library_->InitMainContentExtraction();
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
    base::flat_map<base::FilePath, base::File> model_files,
    mojo::PendingReceiver<mojom::OCRService> ocr_service_receiver,
    InitializeOCRCallback callback) {
  if (!library_) {
    LoadLibrary(library_path);
  }

  if (!library_) {
    std::move(callback).Run(false);
    base::Process::TerminateCurrentProcessImmediately(-1);
  }
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&PreloadedModelData::Create, std::move(model_files)),
      base::BindOnce(&ScreenAIService::InitializeOCRInternal,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(ocr_service_receiver), std::move(callback)));
}

void ScreenAIService::InitializeOCRInternal(
    mojo::PendingReceiver<mojom::OCRService> ocr_service_receiver,
    InitializeMainContentExtractionCallback callback,
    std::unique_ptr<PreloadedModelData> model_data) {
  // `model_data` contains the content of the model files and its accessors are
  // passed to the library. It should be kept in memory until after library
  // initialization.
  bool init_successful = library_->InitOCR();
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
ScreenAIService::PerformOcrAndRecordMetrics(const SkBitmap& image,
                                            bool a11y_tree_request) {
  base::TimeTicks start_time = base::TimeTicks::Now();
  auto result = library_->PerformOcr(image);
  base::TimeDelta elapsed_time = base::TimeTicks::Now() - start_time;
  int lines_count = result ? result->lines_size() : 0;
  VLOG(1) << "OCR returned " << lines_count << " lines in " << elapsed_time;

  base::UmaHistogramCounts100("Accessibility.ScreenAI.OCR.LinesCount",
                              lines_count);
  base::UmaHistogramTimes("Accessibility.ScreenAI.OCR.Time", elapsed_time);
  base::UmaHistogramCounts10M("Accessibility.ScreenAI.OCR.ImageSize10M",
                              image.width() * image.height());

  // If needed to extend to more clients, an identifier can be passed from the
  // client to introduce itself and these metrics can be collected based on it.
  if (a11y_tree_request) {
    base::UmaHistogramCounts100("Accessibility.ScreenAI.OCR.LinesCount.PDF",
                                lines_count);
    base::UmaHistogramTimes("Accessibility.ScreenAI.OCR.Time.PDF",
                            elapsed_time);
    base::UmaHistogramCounts10M("Accessibility.ScreenAI.OCR.ImageSize.PDF",
                                image.width() * image.height());
  }

  return result;
}

void ScreenAIService::PerformOcrAndReturnAnnotation(
    const SkBitmap& image,
    PerformOcrAndReturnAnnotationCallback callback) {
  absl::optional<chrome_screen_ai::VisualAnnotation> annotation_proto =
      PerformOcrAndRecordMetrics(image, /*a11y_tree_request=*/false);

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
      PerformOcrAndRecordMetrics(image, /*a11y_tree_request=*/true);
  ui::AXTreeUpdate update = ConvertVisualAnnotationToTreeUpdate(
      annotation_proto, gfx::Rect(image.width(), image.height()));

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
