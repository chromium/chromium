// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_SCREEN_AI_TASKS_QUEUE_H_
#define COMPONENTS_SERVICES_SCREEN_AI_TASKS_QUEUE_H_

#include <list>

#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "components/services/screen_ai/public/mojom/screen_ai_service.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace screen_ai {

// Thread-safe task queue for Screen AI service.
class TasksQueue {
 public:
  TasksQueue();
  TasksQueue(const TasksQueue&) = delete;
  TasksQueue& operator=(const TasksQueue&) = delete;
  ~TasksQueue();

  struct Task {
    // This task structure is used both by Semantic Layout Extraction and
    // OCR functions. The former is still in research phase and once it
    // matures they will be separated.
    struct VisualAnnotation {
      VisualAnnotation(const SkBitmap& image,
                       const ui::AXTreeID& parent_tree_id,
                       mojom::ScreenAIAnnotator::PerformOcrCallback callback,
                       bool is_ocr);
      ~VisualAnnotation();

      const SkBitmap image;
      const ui::AXTreeID parent_tree_id;
      mojom::ScreenAIAnnotator::PerformOcrCallback callback;
      bool is_ocr;
    };

    struct MainContentExtraction {
      MainContentExtraction(
          const ui::AXTreeUpdate& snapshot,
          ukm::SourceId ukm_source_id,
          mojom::Screen2xMainContentExtractor::ExtractMainContentCallback
              callback,
          mojo::ReceiverId receiver_id);
      ~MainContentExtraction();

      const ui::AXTreeUpdate snapshot;
      ukm::SourceId ukm_source_id;
      mojom::Screen2xMainContentExtractor::ExtractMainContentCallback callback;
      mojo::ReceiverId receiver_id;
      bool canceled = false;
    };

    explicit Task(std::unique_ptr<VisualAnnotation> task);
    explicit Task(std::unique_ptr<MainContentExtraction> task);
    ~Task();

    // Only one of the following pointers will be non-empty.
    std::unique_ptr<VisualAnnotation> visual_annotation;
    std::unique_ptr<MainContentExtraction> main_content_extraction;
  };

  void PushTask(std::unique_ptr<Task::VisualAnnotation> task);
  void PushTask(std::unique_ptr<Task::MainContentExtraction> task);
  std::unique_ptr<Task> PopTask();
  bool Empty();
  size_t Size();

  void CancelMainContentExtractionTasks(mojo::ReceiverId receiver_id);

 private:
  base::Lock lock_;
  std::list<std::unique_ptr<Task>> GUARDED_BY(lock_) queue_;
};

}  // namespace screen_ai

#endif  // COMPONENTS_SERVICES_SCREEN_AI_TASKS_QUEUE_H_
