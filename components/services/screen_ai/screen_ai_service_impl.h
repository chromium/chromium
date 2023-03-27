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
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/services/screen_ai/public/mojom/screen_ai_service.mojom.h"
#include "components/services/screen_ai/screen_ai_library_wrapper.h"
#include "components/services/screen_ai/tasks_queue.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace ukm {
class UkmRecorder;
}

namespace screen_ai {

class ScreenAIServiceImplTest;

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

  // Calls `success_callback` function and tells it if `library` has value.
  // If `library` has value, sets the library and starts processing queued
  // tasks, otherwise kills the current process.
  void SetLibraryAndStartProcessingQueuedTasks(
      LoadAndInitializeLibraryCallback success_callback,
      std::unique_ptr<ScreenAILibraryWrapper> library);

  static void RecordMetrics(ukm::SourceId ukm_source_id,
                            ukm::UkmRecorder* ukm_recorder,
                            base::TimeDelta elapsed_time,
                            bool success);

 private:
  friend class ScreenAIServiceImplTest;

  // Posts a `ProcessNextTaskInQueue` if `library_` is loaded.
  void TriggerProcessingNextTaskInQueue();

  // If there are tasks in `tasks_queue_`, picks the first one, executes
  // it, and posts another `ProcessNextTaskInQueue` is there are more.
  void ProcessNextTaskInQueue();

  std::unique_ptr<ScreenAILibraryWrapper> library_;

  // mojom::ScreenAIAnnotator:
  void ExtractSemanticLayout(const SkBitmap& image,
                             const ui::AXTreeID& parent_tree_id,
                             PerformOcrCallback callback) override;

  // mojom::ScreenAIAnnotator:
  void PerformOcr(const SkBitmap& image,
                  const ui::AXTreeID& parent_tree_id,
                  PerformOcrCallback callback) override;

  // mojom::Screen2xMainContentExtractor:
  void ExtractMainContent(const ui::AXTreeUpdate& snapshot,
                          ukm::SourceId ukm_source_id,
                          ExtractMainContentCallback callback) override;

  // mojom::Screen2xMainContentExtractor:
  void CancelPendingMainContentExtractionTasks() override;

  // mojom::ScreenAIService:
  void LoadAndInitializeLibrary(
      base::File model_config,
      base::File model_tflite,
      const base::FilePath& library_path,
      LoadAndInitializeLibraryCallback callback) override;

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

  // Wrapper functions for queued tasks.
  void VisualAnnotationHelper(
      std::unique_ptr<TasksQueue::Task::VisualAnnotation> request);
  void VisualAnnotationReplier(PerformOcrCallback callback,
                               ui::AXTreeUpdate update);
  void ExtractMainContentHelper(
      std::unique_ptr<TasksQueue::Task::MainContentExtraction> request);
  void ExtractMainContentReplier(
      ExtractMainContentCallback callback,
      mojom::Screen2xMainContentExtractor::Status status,
      std::vector<int32_t> content_node_ids);

  // Returns the receiver id of the last main content extractor client.
  virtual mojo::ReceiverId GetMainContentExtractorReceiverId();

  // After library is loaded, this internal task scheduler picks queued
  // tasks from  `tasks_queue_` and processes them in the same order.
  // Owned by this class.
  const scoped_refptr<base::SequencedTaskRunner> helper_task_runner_;

  // Main service task runner that runs mojo messages.
  const scoped_refptr<base::SequencedTaskRunner> main_task_runner_;

  // Internal queue of received tasks. The tasks are in the same sequence as the
  // mojo remotes, except cancelling tasks which are immediately applied.
  TasksQueue tasks_queue_;

  mojo::Receiver<mojom::ScreenAIService> receiver_;

  // The set of receivers used to receive messages from annotators.
  mojo::ReceiverSet<mojom::ScreenAIAnnotator> screen_ai_annotators_;

  // The client that can receive annotator update messages.
  mojo::Remote<mojom::ScreenAIAnnotatorClient> screen_ai_annotator_client_;

  // The set of receivers used to receive messages from main content
  // extractors.
  mojo::ReceiverSet<mojom::Screen2xMainContentExtractor>
      screen_2x_main_content_extractors_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<ScreenAIService> weak_ptr_factory_{this};
};

}  // namespace screen_ai

#endif  // COMPONENTS_SERVICES_SCREEN_AI_SCREEN_AI_SERVICE_IMPL_H_
