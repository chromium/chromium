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

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ScreenAILoadLibraryResult {
  kAllOk = 0,
  kDeprecatedVisualAnnotationFailed = 1,
  kMainContentExtractionFailed = 2,
  kLayoutExtractionFailed = 3,
  kOcrFailed = 4,
  kFunctionsLoadFailed = 5,
  kMaxValue = kFunctionsLoadFailed,
};

// Returns an empty result if load or initialization fail.
std::unique_ptr<ScreenAILibraryWrapper> LoadAndInitializeLibraryInternal(
    base::File model_config,
    base::File model_tflite,
    const base::FilePath& library_path) {
  DCHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  std::unique_ptr<ScreenAILibraryWrapper> library =
      std::make_unique<ScreenAILibraryWrapper>();

  bool init_ok = true;

  if (!library->Init(library_path)) {
    init_ok = false;
    base::UmaHistogramEnumeration(
        "Accessibility.ScreenAI.LoadLibraryResult",
        ScreenAILoadLibraryResult::kFunctionsLoadFailed);
  }

  if (init_ok) {
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
  }

  if (init_ok && features::IsPdfOcrEnabled()) {
    if (!library->InitOCR(library_path.DirName())) {
      init_ok = false;
      base::UmaHistogramEnumeration("Accessibility.ScreenAI.LoadLibraryResult",
                                    ScreenAILoadLibraryResult::kOcrFailed);
    }
  }

  if (init_ok && features::IsLayoutExtractionEnabled()) {
    if (!library->InitLayoutExtraction()) {
      init_ok = false;
      base::UmaHistogramEnumeration(
          "Accessibility.ScreenAI.LoadLibraryResult",
          ScreenAILoadLibraryResult::kLayoutExtractionFailed);
    }
  }

  if (init_ok && features::IsReadAnythingWithScreen2xEnabled()) {
    if (!library->InitMainContentExtraction(model_config, model_tflite)) {
      init_ok = false;
      base::UmaHistogramEnumeration(
          "Accessibility.ScreenAI.LoadLibraryResult",
          ScreenAILoadLibraryResult::kMainContentExtractionFailed);
    }
  }

  if (init_ok) {
    base::UmaHistogramEnumeration("Accessibility.ScreenAI.LoadLibraryResult",
                                  ScreenAILoadLibraryResult::kAllOk);
  } else {
    VLOG(0) << "Screen AI library initialization failed.";
    library.reset();
  }

  return library;
}

}  // namespace

ScreenAIService::ScreenAIService(
    mojo::PendingReceiver<mojom::ScreenAIService> receiver)
    : helper_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})),
      main_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
      receiver_(this, std::move(receiver)) {}

ScreenAIService::~ScreenAIService() = default;

void ScreenAIService::LoadAndInitializeLibrary(
    base::File model_config,
    base::File model_tflite,
    const base::FilePath& library_path,
    LoadAndInitializeLibraryCallback callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&LoadAndInitializeLibraryInternal, std::move(model_config),
                     std::move(model_tflite), library_path),
      base::BindOnce(&ScreenAIService::SetLibraryAndStartProcessingQueuedTasks,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ScreenAIService::SetLibraryAndStartProcessingQueuedTasks(
    LoadAndInitializeLibraryCallback success_callback,
    std::unique_ptr<ScreenAILibraryWrapper> library) {
  std::move(success_callback).Run((bool)library);

  if (library) {
    library_ = std::move(library);
    size_t queue_length = tasks_queue_.Size();
    for (size_t i = 0; i < queue_length; i++) {
      TriggerProcessingNextTaskInQueue();
    }
  } else {
    base::Process::TerminateCurrentProcessImmediately(-1);
  }
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

void ScreenAIService::ExtractSemanticLayout(const SkBitmap& image,
                                            const ui::AXTreeID& parent_tree_id,
                                            PerformOcrCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  tasks_queue_.PushTask(std::make_unique<TasksQueue::Task::VisualAnnotation>(
      std::move(image), parent_tree_id, std::move(callback), /*is_ocr=*/false));

  TriggerProcessingNextTaskInQueue();
}

void ScreenAIService::PerformOcr(const SkBitmap& image,
                                 const ui::AXTreeID& parent_tree_id,
                                 PerformOcrCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  tasks_queue_.PushTask(std::make_unique<TasksQueue::Task::VisualAnnotation>(
      std::move(image), parent_tree_id, std::move(callback), /*is_ocr=*/true));

  TriggerProcessingNextTaskInQueue();
}

void ScreenAIService::ExtractMainContent(const ui::AXTreeUpdate& snapshot,
                                         ukm::SourceId ukm_source_id,
                                         ExtractMainContentCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  tasks_queue_.PushTask(
      std::make_unique<TasksQueue::Task::MainContentExtraction>(
          std::move(snapshot), ukm_source_id, std::move(callback),
          GetMainContentExtractorReceiverId()));

  TriggerProcessingNextTaskInQueue();
}

void ScreenAIService::VisualAnnotationHelper(
    std::unique_ptr<TasksQueue::Task::VisualAnnotation> request) {
  DCHECK(screen_ai_annotator_client_.is_bound());

  std::string proto_as_string;
  // TODO(https://crbug.com/1278249): Consider adding a signature that
  // verifies the data integrity and source.
  bool result = request->is_ocr
                    ? library_->PerformOcr(request->image, proto_as_string)
                    : library_->ExtractLayout(request->image, proto_as_string);
  ui::AXTreeUpdate annotation;
  if (!result || proto_as_string.empty()) {
    DCHECK_EQ(annotation.tree_data.tree_id, ui::AXTreeIDUnknown());
    VLOG(1) << "Screen AI library could not process snapshot or no OCR data.";
  } else {
    gfx::Rect image_rect(request->image.width(), request->image.height());
    annotation = VisualAnnotationToAXTreeUpdate(proto_as_string, image_rect);
    ScreenAIAXTreeSerializer serializer(request->parent_tree_id,
                                        std::move(annotation.nodes));
    annotation = serializer.Serialize();

    // `ScreenAIAXTreeSerializer` should have assigned a new tree ID to
    // `update`. Thereby, it should never be an unknown tree ID, otherwise there
    // has been an unexpected serialization bug.
    DCHECK_NE(annotation.tree_data.tree_id, ui::AXTreeIDUnknown())
        << "Invalid serialization.\n"
        << annotation.ToString();
  }

  // The original caller is always replied to, and an AXTreeIDUnknown is sent to
  // tell it that the annotation function was not successful. However the client
  // is only contacted for successful runs and when we have an update.
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&ScreenAIService::VisualAnnotationReplier,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(request->callback), std::move(annotation)));
}

void ScreenAIService::VisualAnnotationReplier(PerformOcrCallback callback,
                                              ui::AXTreeUpdate update) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // The original caller is always replied to, and an AXTreeIDUnknown
  // is sent to tell it that the annotation function was not
  // successful. However the client is only contacted for successful
  // runs and when we have an update.
  std::move(callback).Run(update.tree_data.tree_id);
  if (update.tree_data.tree_id != ui::AXTreeIDUnknown()) {
    screen_ai_annotator_client_->HandleAXTreeUpdate(update);
  }
}

void ScreenAIService::ExtractMainContentHelper(
    std::unique_ptr<TasksQueue::Task::MainContentExtraction> request) {
  std::vector<int32_t> content_node_ids;
  mojom::Screen2xMainContentExtractor::Status status =
      mojom::Screen2xMainContentExtractor::Status::kOK;

  if (request->canceled) {
    status = mojom::Screen2xMainContentExtractor::Status::kCanceled;
  } else if (!request->snapshot.nodes.empty()) {
    std::string serialized_snapshot =
        SnapshotToViewHierarchy(request->snapshot);

    base::TimeTicks start_time = base::TimeTicks::Now();
    bool success =
        library_->ExtractMainContent(serialized_snapshot, content_node_ids);
    base::TimeDelta elapsed_time = base::TimeTicks::Now() - start_time;
    if (success) {
      VLOG(2) << "Screen2x returned " << content_node_ids.size()
              << " node ids.";
      RecordMetrics(request->ukm_source_id, ukm::UkmRecorder::Get(),
                    elapsed_time,
                    /* success= */ true);
      status = mojom::Screen2xMainContentExtractor::Status::kOK;
    } else {
      VLOG(1) << "Screen2x did not return main content.";
      RecordMetrics(request->ukm_source_id, ukm::UkmRecorder::Get(),
                    elapsed_time,
                    /* success= */ false);
      status = mojom::Screen2xMainContentExtractor::Status::kFailed;
    }
  }

  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&ScreenAIService::ExtractMainContentReplier,
                                weak_ptr_factory_.GetWeakPtr(),
                                std::move(request->callback), status,
                                std::move(content_node_ids)));
}

void ScreenAIService::ExtractMainContentReplier(
    ExtractMainContentCallback callback,
    mojom::Screen2xMainContentExtractor::Status status,
    std::vector<int32_t> content_node_ids) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(callback).Run(status, content_node_ids);
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

void ScreenAIService::CancelPendingMainContentExtractionTasks() {
  tasks_queue_.CancelMainContentExtractionTasks(
      GetMainContentExtractorReceiverId());
}

mojo::ReceiverId ScreenAIService::GetMainContentExtractorReceiverId() {
  DCHECK(!screen_2x_main_content_extractors_.empty());
  return screen_2x_main_content_extractors_.current_receiver();
}

void ScreenAIService::TriggerProcessingNextTaskInQueue() {
  if (!library_) {
    return;
  }
  // Sending `this` unretained is safe as `helper_task_runner_` belongs to
  // `this`.
  helper_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&ScreenAIService::ProcessNextTaskInQueue,
                                base::Unretained(this)));
}

void ScreenAIService::ProcessNextTaskInQueue() {
  std::unique_ptr<TasksQueue::Task> task = tasks_queue_.PopTask();
  if (!task) {
    return;
  }

  if (task->main_content_extraction) {
    ExtractMainContentHelper(std::move(task->main_content_extraction));
  } else if (task->visual_annotation) {
    VisualAnnotationHelper(std::move(task->visual_annotation));
  } else {
    NOTREACHED() << "Empty queued request.";
  }
}

}  // namespace screen_ai
