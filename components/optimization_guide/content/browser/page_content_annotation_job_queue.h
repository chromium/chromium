// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTENT_ANNOTATION_JOB_QUEUE_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTENT_ANNOTATION_JOB_QUEUE_H_

#include <memory>

namespace optimization_guide {

class PageContentAnnotationJob;

// An interface definition of a job queue for Page Content Annotations.
class PageContentAnnotationJobQueue {
 public:
  virtual ~PageContentAnnotationJobQueue() = default;

  // Adds a |job| to the queue.
  virtual void AddJob(std::unique_ptr<PageContentAnnotationJob> job) = 0;

  // Takes the next job in the queue, passing its ownership and removing
  // it from the queue. nullptr if the queue is empty.
  virtual std::unique_ptr<PageContentAnnotationJob> NextJob() = 0;

  // Peeks at the next job in the queue without taking ownership or removing the
  // job from the queue. nullptr if the queue is empty.
  virtual PageContentAnnotationJob* Peek() const = 0;

  // Returns the current size of the queue.
  virtual size_t size() const = 0;

  // Returns if the queue is empty.
  virtual bool empty() const = 0;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTENT_ANNOTATION_JOB_QUEUE_H_