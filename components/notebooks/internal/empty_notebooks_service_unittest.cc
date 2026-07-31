// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/notebooks/internal/empty_notebooks_service.h"

#include <memory>

#include "base/test/task_environment.h"
#include "base/uuid.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace notebooks {

class EmptyNotebooksServiceTest : public testing::Test {
 public:
  EmptyNotebooksServiceTest() = default;

  ~EmptyNotebooksServiceTest() override = default;

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(EmptyNotebooksServiceTest, ConstructionAndInitialization) {
  auto service = std::make_unique<EmptyNotebooksService>();
  EXPECT_TRUE(service->IsEmptyForTesting());
}

TEST_F(EmptyNotebooksServiceTest, GetNotebookReturnsNullopt) {
  auto service = std::make_unique<EmptyNotebooksService>();
  EXPECT_EQ(service->GetNotebook(NotebookId(base::Uuid::GenerateRandomV4())),
            std::nullopt);
}

TEST_F(EmptyNotebooksServiceTest, GetAllNotebooksReturnsEmpty) {
  auto service = std::make_unique<EmptyNotebooksService>();
  EXPECT_TRUE(service->GetAllNotebooks().empty());
}

}  // namespace notebooks
