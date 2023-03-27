// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/screen_ai/tasks_queue.h"

namespace screen_ai {

TasksQueue::TasksQueue() = default;
TasksQueue::~TasksQueue() = default;

TasksQueue::Task::VisualAnnotation::VisualAnnotation(
    const SkBitmap& image,
    const ui::AXTreeID& parent_tree_id,
    mojom::ScreenAIAnnotator::PerformOcrCallback callback,
    bool is_ocr)
    : image(image),
      parent_tree_id(parent_tree_id),
      callback(std::move(callback)),
      is_ocr(is_ocr) {}

TasksQueue::Task::VisualAnnotation::~VisualAnnotation() = default;

TasksQueue::Task::MainContentExtraction::MainContentExtraction(
    const ui::AXTreeUpdate& snapshot,
    ukm::SourceId ukm_source_id,
    mojom::Screen2xMainContentExtractor::ExtractMainContentCallback callback,
    mojo::ReceiverId receiver_id)
    : snapshot(std::move(snapshot)),
      ukm_source_id(ukm_source_id),
      callback(std::move(callback)),
      receiver_id(receiver_id) {}

TasksQueue::Task::MainContentExtraction::~MainContentExtraction() = default;

TasksQueue::Task::Task(
    std::unique_ptr<TasksQueue::Task::VisualAnnotation> request)
    : visual_annotation(std::move(request)) {}

TasksQueue::Task::Task(
    std::unique_ptr<TasksQueue::Task::MainContentExtraction> request)
    : main_content_extraction(std::move(request)) {}

TasksQueue::Task::~Task() = default;

bool TasksQueue::Empty() {
  base::AutoLock lock(lock_);
  return queue_.empty();
}
size_t TasksQueue::Size() {
  base::AutoLock lock(lock_);
  return queue_.size();
}

void TasksQueue::PushTask(std::unique_ptr<Task::VisualAnnotation> task) {
  base::AutoLock lock(lock_);
  queue_.emplace_back(std::make_unique<Task>(std::move(task)));
}

void TasksQueue::PushTask(std::unique_ptr<Task::MainContentExtraction> task) {
  base::AutoLock lock(lock_);
  queue_.emplace_back(std::make_unique<Task>(std::move(task)));
}

std::unique_ptr<TasksQueue::Task> TasksQueue::PopTask() {
  base::AutoLock lock(lock_);
  std::unique_ptr<TasksQueue::Task> result;
  if (!queue_.empty()) {
    result.swap(queue_.front());
    queue_.pop_front();
  }
  return result;
}

void TasksQueue::CancelMainContentExtractionTasks(
    mojo::ReceiverId receiver_id) {
  base::AutoLock lock(lock_);
  for (auto& task : queue_) {
    if (task->main_content_extraction &&
        task->main_content_extraction->receiver_id == receiver_id) {
      task->main_content_extraction->canceled = true;
    }
  }
}

}  // namespace screen_ai
