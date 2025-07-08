// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/collaboration/internal/comments/comments_service_impl.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"

namespace collaboration::comments {

class CommentsServiceImplTest : public testing::Test {
 public:
  void SetUp() override { service_ = std::make_unique<CommentsServiceImpl>(); }

 protected:
  std::unique_ptr<CommentsServiceImpl> service_;
};

TEST_F(CommentsServiceImplTest, TestServiceConstruction) {
  EXPECT_FALSE(service_->IsEmptyService());
  EXPECT_FALSE(service_->IsInitialized());
}

}  // namespace collaboration::comments
