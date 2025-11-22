// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_tasks/internal/pending_context_decorator.h"

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/uuid.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/contextual_tasks/public/contextual_task_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace contextual_tasks {

class PendingContextDecoratorTest : public testing::Test {
 public:
  PendingContextDecoratorTest() = default;
  ~PendingContextDecoratorTest() override = default;

 protected:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(PendingContextDecoratorTest, Construction) {
  PendingContextDecorator decorator;
  ContextualTask task(base::Uuid::GenerateRandomV4());
  auto context = std::make_unique<ContextualTaskContext>(task);
  auto* context_ptr = context.get();

  base::RunLoop run_loop;
  decorator.DecorateContext(
      std::move(context), nullptr,
      base::BindOnce(
          [](ContextualTaskContext* expected_context,
             base::OnceClosure quit_closure,
             std::unique_ptr<ContextualTaskContext> context) {
            EXPECT_EQ(context.get(), expected_context);
            std::move(quit_closure).Run();
          },
          context_ptr, run_loop.QuitClosure()));
  run_loop.Run();
}

}  // namespace contextual_tasks
