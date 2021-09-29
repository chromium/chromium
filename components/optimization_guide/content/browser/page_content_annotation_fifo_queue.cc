// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/browser/page_content_annotation_fifo_queue.h"

#include "components/optimization_guide/content/browser/page_content_annotation_job.h"

namespace optimization_guide {

PageContentAnnotationFIFOQueue::PageContentAnnotationFIFOQueue() = default;
PageContentAnnotationFIFOQueue::~PageContentAnnotationFIFOQueue() = default;

void PageContentAnnotationFIFOQueue::AddJob(
    std::unique_ptr<PageContentAnnotationJob> job) {
  queue_.push_back(std::move(job));
}

std::unique_ptr<PageContentAnnotationJob>
PageContentAnnotationFIFOQueue::NextJob() {
  if (queue_.empty())
    return nullptr;

  // std::list::pop_front doesn't return the removed element, so it's hard to
  // take ownership of it. Instead, swap with a local var then erase.
  std::unique_ptr<PageContentAnnotationJob> job;
  queue_.front().swap(job);
  DCHECK(job);

  queue_.pop_front();
  return job;
}

PageContentAnnotationJob* PageContentAnnotationFIFOQueue::Peek() const {
  if (queue_.empty())
    return nullptr;
  return queue_.front().get();
}

size_t PageContentAnnotationFIFOQueue::size() const {
  return queue_.size();
}

bool PageContentAnnotationFIFOQueue::empty() const {
  return queue_.empty();
}

}  // namespace optimization_guide