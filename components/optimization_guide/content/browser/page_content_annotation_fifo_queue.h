// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTENT_ANNOTATION_FIFO_QUEUE_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTENT_ANNOTATION_FIFO_QUEUE_H_

#include <deque>

#include "components/optimization_guide/content/browser/page_content_annotation_job_queue.h"

namespace optimization_guide {

class PageContentAnnotationJob;

// A Job Queue that obeys FIFO (first in, first out) ordering.
class PageContentAnnotationFIFOQueue : public PageContentAnnotationJobQueue {
 public:
  PageContentAnnotationFIFOQueue();
  ~PageContentAnnotationFIFOQueue() override;

  // PageContentAnnotationJobQueue:
  void AddJob(std::unique_ptr<PageContentAnnotationJob> job) override;
  std::unique_ptr<PageContentAnnotationJob> NextJob() override;
  PageContentAnnotationJob* Peek() const override;
  size_t size() const override;
  bool empty() const override;

 private:
  // The backing data structure. Only the front and back of this container are
  // ever modified, so a linked list impl is preferable to an array list since
  // removal from the front is a common operation.
  std::deque<std::unique_ptr<PageContentAnnotationJob>> queue_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTENT_ANNOTATION_FIFO_QUEUE_H_
