// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_tasks/internal/composite_context_decorator.h"

#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/uuid.h"
#include "components/contextual_tasks/public/context_decorator.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/contextual_tasks/public/contextual_task_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace contextual_tasks {

// Mocks the ContextDecorator for testing.
class MockContextDecorator : public ContextDecorator {
 public:
  MOCK_METHOD(void,
              DecorateContext,
              (std::unique_ptr<ContextualTaskContext> context,
               base::OnceCallback<void(std::unique_ptr<ContextualTaskContext>)>
                   context_callback),
              (override));
};

// A gmock action to asynchronously run the callback passed to DecorateContext.
ACTION(RunCallbackAsync) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          std::move(
              const_cast<base::OnceCallback<void(
                  std::unique_ptr<contextual_tasks::ContextualTaskContext>)>&>(
                  arg1)),
          std::move(const_cast<
                    std::unique_ptr<contextual_tasks::ContextualTaskContext>&>(
              arg0))));
}

namespace {
// Helper function to create a mock decorator, add it to a vector, and return
// a raw pointer to it for setting expectations.
MockContextDecorator* AddMockDecorator(
    std::map<ContextualTaskContextSource, std::unique_ptr<ContextDecorator>>*
        decorators,
    ContextualTaskContextSource source) {
  auto mock_decorator =
      std::make_unique<testing::StrictMock<MockContextDecorator>>();
  MockContextDecorator* ptr = mock_decorator.get();
  decorators->emplace(source, std::move(mock_decorator));
  return ptr;
}
}  // namespace

class CompositeContextDecoratorTest : public testing::Test {
 public:
  CompositeContextDecoratorTest() = default;
  ~CompositeContextDecoratorTest() override = default;

 protected:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(CompositeContextDecoratorTest, DecorateContext_EmptySources) {
  std::map<ContextualTaskContextSource, std::unique_ptr<ContextDecorator>>
      decorators;
  auto* mock_decorator1_ptr = AddMockDecorator(
      &decorators, ContextualTaskContextSource::kFallbackTitle);
  auto* mock_decorator2_ptr = AddMockDecorator(
      &decorators, ContextualTaskContextSource::kFaviconService);

  CompositeContextDecorator composite_decorator(std::move(decorators));

  ContextualTask task(base::Uuid::GenerateRandomV4());
  auto context = std::make_unique<ContextualTaskContext>(task);

  // InSequence is used to ensure that the decorators are called in the
  // correct order.
  testing::InSequence s;
  EXPECT_CALL(*mock_decorator1_ptr, DecorateContext(testing::_, testing::_))
      .WillOnce(RunCallbackAsync());
  EXPECT_CALL(*mock_decorator2_ptr, DecorateContext(testing::_, testing::_))
      .WillOnce(RunCallbackAsync());

  base::RunLoop run_loop;
  composite_decorator.DecorateContext(
      std::move(context), {},
      base::BindOnce(
          [](base::OnceClosure quit_closure,
             std::unique_ptr<ContextualTaskContext> context) {
            std::move(quit_closure).Run();
          },
          run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(CompositeContextDecoratorTest, DecorateContext_SingleSource) {
  std::map<ContextualTaskContextSource, std::unique_ptr<ContextDecorator>>
      decorators;
  auto* mock_decorator1_ptr = AddMockDecorator(
      &decorators, ContextualTaskContextSource::kFallbackTitle);
  auto* mock_decorator2_ptr = AddMockDecorator(
      &decorators, ContextualTaskContextSource::kFaviconService);

  CompositeContextDecorator composite_decorator(std::move(decorators));

  ContextualTask task(base::Uuid::GenerateRandomV4());
  auto context = std::make_unique<ContextualTaskContext>(task);

  EXPECT_CALL(*mock_decorator1_ptr, DecorateContext(testing::_, testing::_))
      .Times(0);
  EXPECT_CALL(*mock_decorator2_ptr, DecorateContext(testing::_, testing::_))
      .WillOnce(RunCallbackAsync());

  base::RunLoop run_loop;
  composite_decorator.DecorateContext(
      std::move(context), {ContextualTaskContextSource::kFaviconService},
      base::BindOnce(
          [](base::OnceClosure quit_closure,
             std::unique_ptr<ContextualTaskContext> context) {
            std::move(quit_closure).Run();
          },
          run_loop.QuitClosure()));
  run_loop.Run();
}

}  // namespace contextual_tasks
