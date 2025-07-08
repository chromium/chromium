// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/collaboration/internal/comments/empty_comments_service.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"

namespace collaboration::comments {

class EmptyCommentsServiceTest : public testing::Test {
 public:
  void SetUp() override { service_ = std::make_unique<EmptyCommentsService>(); }

 protected:
  std::unique_ptr<EmptyCommentsService> service_;
};

TEST_F(EmptyCommentsServiceTest, TestServiceConstruction) {
  EXPECT_TRUE(service_->IsEmptyService());
  EXPECT_TRUE(service_->IsInitialized());
}

}  // namespace collaboration::comments
