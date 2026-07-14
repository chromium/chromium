// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/notebooks/internal/notebooks_service_impl.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"

namespace notebooks {

class NotebooksServiceImplTest : public testing::Test {
 public:
  NotebooksServiceImplTest() = default;

  ~NotebooksServiceImplTest() override = default;
};

TEST_F(NotebooksServiceImplTest, ConstructionAndInitialization) {
  auto service = std::make_unique<NotebooksServiceImpl>();
  EXPECT_FALSE(service->IsEmptyForTesting());
}

}  // namespace notebooks
