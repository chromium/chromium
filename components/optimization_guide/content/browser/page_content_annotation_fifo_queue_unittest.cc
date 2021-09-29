// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/browser/page_content_annotation_fifo_queue.h"

#include "components/optimization_guide/content/browser/page_content_annotation_job.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

class PageContentAnnotationFIFOQueueTest : public testing::Test {
 public:
  std::unique_ptr<PageContentAnnotationJob> NewJob(const std::string& input) {
    return std::make_unique<PageContentAnnotationJob>(
        input, AnnotationType::kPageTopics);
  }
};

TEST_F(PageContentAnnotationFIFOQueueTest, StartsEmpty) {
  PageContentAnnotationFIFOQueue queue;
  EXPECT_EQ(0U, queue.size());
  EXPECT_EQ(nullptr, queue.Peek());
  EXPECT_EQ(nullptr, queue.NextJob());
}

TEST_F(PageContentAnnotationFIFOQueueTest, KeepsOrdering) {
  PageContentAnnotationFIFOQueue queue;
  queue.AddJob(NewJob("A"));
  queue.AddJob(NewJob("B"));
  queue.AddJob(NewJob("C"));
  EXPECT_EQ(3U, queue.size());

  EXPECT_EQ("A", queue.NextJob()->input());
  EXPECT_EQ("B", queue.NextJob()->input());
  EXPECT_EQ("C", queue.NextJob()->input());

  EXPECT_EQ(0U, queue.size());
}

TEST_F(PageContentAnnotationFIFOQueueTest, KeepsDuplicates) {
  PageContentAnnotationFIFOQueue queue;
  queue.AddJob(NewJob("A"));
  queue.AddJob(NewJob("A"));
  queue.AddJob(NewJob("A"));
  EXPECT_EQ(3U, queue.size());

  EXPECT_EQ("A", queue.NextJob()->input());
  EXPECT_EQ("A", queue.NextJob()->input());
  EXPECT_EQ("A", queue.NextJob()->input());

  EXPECT_EQ(0U, queue.size());
}

TEST_F(PageContentAnnotationFIFOQueueTest, PeekABoo) {
  PageContentAnnotationFIFOQueue queue;
  queue.AddJob(NewJob("A"));
  queue.AddJob(NewJob("B"));
  queue.AddJob(NewJob("C"));
  EXPECT_EQ(3U, queue.size());

  EXPECT_EQ("A", queue.Peek()->input());
  EXPECT_EQ("A", queue.NextJob()->input());

  EXPECT_EQ("B", queue.Peek()->input());
  EXPECT_EQ("B", queue.NextJob()->input());

  EXPECT_EQ("C", queue.Peek()->input());
  EXPECT_EQ("C", queue.NextJob()->input());

  EXPECT_EQ(0U, queue.size());
}

}  // namespace optimization_guide