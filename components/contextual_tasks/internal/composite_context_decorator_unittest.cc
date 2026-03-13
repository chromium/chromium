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
#include "base/unguessable_token.h"
#include "base/uuid.h"
#include "components/contextual_search/contextual_search_service.h"
#include "components/contextual_search/contextual_search_session_handle.h"
#include "components/contextual_search/mock_contextual_search_context_controller.h"
#include "components/contextual_tasks/internal/token_based_context_decorator.h"
#include "components/contextual_tasks/public/context_decoration_params.h"
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
               ContextDecorationParams* params,
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
                  arg2)),
          std::move(const_cast<
                    std::unique_ptr<contextual_tasks::ContextualTaskContext>&>(
              arg0))));
}

namespace {
class TestTokenBasedContextDecorator : public TokenBasedContextDecorator {
 public:
  TestTokenBasedContextDecorator() = default;

  MOCK_METHOD(std::vector<base::UnguessableToken>,
              GetTokensToDecorate,
              (contextual_search::ContextualSearchSessionHandle * handle),
              (const, override));
};

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
  EXPECT_CALL(*mock_decorator1_ptr,
              DecorateContext(testing::_, testing::_, testing::_))
      .WillOnce(RunCallbackAsync());
  EXPECT_CALL(*mock_decorator2_ptr,
              DecorateContext(testing::_, testing::_, testing::_))
      .WillOnce(RunCallbackAsync());

  base::RunLoop run_loop;
  composite_decorator.DecorateContext(
      std::move(context), {}, nullptr,
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

  EXPECT_CALL(*mock_decorator1_ptr,
              DecorateContext(testing::_, testing::_, testing::_))
      .Times(0);
  EXPECT_CALL(*mock_decorator2_ptr,
              DecorateContext(testing::_, testing::_, testing::_))
      .WillOnce(RunCallbackAsync());

  base::RunLoop run_loop;
  composite_decorator.DecorateContext(
      std::move(context), {ContextualTaskContextSource::kFaviconService},
      nullptr,
      base::BindOnce(
          [](base::OnceClosure quit_closure,
             std::unique_ptr<ContextualTaskContext> context) {
            std::move(quit_closure).Run();
          },
          run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(CompositeContextDecoratorTest,
       DecorateContext_SpecificSources_RespectsEarlyOrder) {
  std::map<ContextualTaskContextSource, std::unique_ptr<ContextDecorator>>
      decorators;

  // mock_decorator_other corresponds to kFallbackTitle (Enum 0)
  auto* mock_decorator_other = AddMockDecorator(
      &decorators, ContextualTaskContextSource::kFallbackTitle);

  // mock_decorator_early corresponds to kUploadedContextDecorator (Enum 4)
  auto* mock_decorator_early = AddMockDecorator(
      &decorators, ContextualTaskContextSource::kUploadedContextDecorator);

  auto* mock_decorator_early2 = AddMockDecorator(
      &decorators, ContextualTaskContextSource::kSubmittedContextDecorator);

  CompositeContextDecorator composite_decorator(std::move(decorators));

  ContextualTask task(base::Uuid::GenerateRandomV4());
  auto context = std::make_unique<ContextualTaskContext>(task);

  testing::InSequence s;

  // Crucial Check: Even though kFallbackTitle (other) has a lower Enum ID,
  // kUploadedContextDecorator (early) MUST run first.
  EXPECT_CALL(*mock_decorator_early,
              DecorateContext(testing::_, testing::_, testing::_))
      .WillOnce(RunCallbackAsync());

  EXPECT_CALL(*mock_decorator_early2,
              DecorateContext(testing::_, testing::_, testing::_))
      .WillOnce(RunCallbackAsync());

  EXPECT_CALL(*mock_decorator_other,
              DecorateContext(testing::_, testing::_, testing::_))
      .WillOnce(RunCallbackAsync());

  base::RunLoop run_loop;

  // We explicitly request both sources.
  // In a standard set, these would be sorted by ID (0 then 4).
  // We want to prove our logic reorders them.
  std::set<ContextualTaskContextSource> sources = {
      ContextualTaskContextSource::kUploadedContextDecorator,
      ContextualTaskContextSource::kSubmittedContextDecorator,
      ContextualTaskContextSource::kFallbackTitle};

  composite_decorator.DecorateContext(
      std::move(context), sources, {},
      base::BindOnce(
          [](base::OnceClosure quit_closure,
             std::unique_ptr<ContextualTaskContext> context) {
            std::move(quit_closure).Run();
          },
          run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(CompositeContextDecoratorTest,
       DecorateContext_AllSources_RunsEarlyDecoratorsFirst) {
  std::map<ContextualTaskContextSource, std::unique_ptr<ContextDecorator>>
      decorators;

  // kFallbackTitle has a lower Enum ID (0) than kUploadedContextDecorator (4).
  // Standard map iteration would run this FIRST.
  auto* mock_decorator_other = AddMockDecorator(
      &decorators, ContextualTaskContextSource::kFallbackTitle);

  // kUploadedContextDecorator has a higher Enum ID (4).
  // Standard map iteration would run this LAST.
  auto* mock_decorator_early = AddMockDecorator(
      &decorators, ContextualTaskContextSource::kUploadedContextDecorator);

  auto* mock_decorator_early2 = AddMockDecorator(
      &decorators, ContextualTaskContextSource::kSubmittedContextDecorator);

  CompositeContextDecorator composite_decorator(std::move(decorators));

  ContextualTask task(base::Uuid::GenerateRandomV4());
  auto context = std::make_unique<ContextualTaskContext>(task);

  testing::InSequence s;

  // We expect the "Early" decorator to run first, overriding the natural Enum
  // ID order.
  EXPECT_CALL(*mock_decorator_early,
              DecorateContext(testing::_, testing::_, testing::_))
      .WillOnce(RunCallbackAsync());

  EXPECT_CALL(*mock_decorator_early2,
              DecorateContext(testing::_, testing::_, testing::_))
      .WillOnce(RunCallbackAsync());

  EXPECT_CALL(*mock_decorator_other,
              DecorateContext(testing::_, testing::_, testing::_))
      .WillOnce(RunCallbackAsync());

  base::RunLoop run_loop;

  // Passing empty sources triggers the "Run All" code path.
  composite_decorator.DecorateContext(
      std::move(context), {}, {},
      base::BindOnce(
          [](base::OnceClosure quit_closure,
             std::unique_ptr<ContextualTaskContext> context) {
            std::move(quit_closure).Run();
          },
          run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(CompositeContextDecoratorTest, DecorateContext_DeduplicatesTokens) {
  // Set up mock controller and session handle
  contextual_search::ContextualSearchService service(
      nullptr, nullptr, nullptr, nullptr, version_info::Channel::UNKNOWN, "");
  auto mock_controller = std::make_unique<
      contextual_search::MockContextualSearchContextController>();
  auto* mock_controller_ptr = mock_controller.get();
  auto session_handle =
      service.CreateSessionForTesting(std::move(mock_controller), nullptr);

  base::UnguessableToken token = base::UnguessableToken::Create();

  contextual_search::FileInfo file_info;
  file_info.tab_url = GURL("https://example.com/");
  file_info.tab_title = "Test Title";
  file_info.tab_session_id = SessionID::FromSerializedValue(123);
  EXPECT_CALL(*mock_controller_ptr, GetFileInfo(token))
      .Times(1)
      .WillOnce(testing::Return(&file_info));

  auto params = std::make_unique<ContextDecorationParams>();
  params->contextual_search_session_handle = session_handle->AsWeakPtr();

  std::map<ContextualTaskContextSource, std::unique_ptr<ContextDecorator>>
      decorators;
  // Let the first decorator be a mock that just inserts the URL directly.
  auto mock_decorator1 =
      std::make_unique<testing::StrictMock<MockContextDecorator>>();
  EXPECT_CALL(*mock_decorator1,
              DecorateContext(testing::_, testing::_, testing::_))
      .WillOnce(
          [](std::unique_ptr<ContextualTaskContext> context,
             ContextDecorationParams* params,
             base::OnceCallback<void(std::unique_ptr<ContextualTaskContext>)>
                 context_callback) {
            // Add the same URL
            UrlAttachment attachment(GURL("https://example.com/"),
                                     ResourceType::kWebpage);
            auto& decorator_data =
                attachment.GetMutableDecoratorDataForTesting();
            decorator_data.contextual_search_context_data.title = u"Test Title";
            decorator_data.contextual_search_context_data.tab_session_id =
                SessionID::FromSerializedValue(123);
            context->GetMutableUrlAttachmentsForTesting().push_back(
                std::move(attachment));

            base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE, base::BindOnce(std::move(context_callback),
                                          std::move(context)));
          });

  decorators.emplace(ContextualTaskContextSource::kUploadedContextDecorator,
                     std::move(mock_decorator1));

  // The second decorator is the TestTokenBasedContextDecorator which tries to
  // add the same info
  auto test_decorator =
      std::make_unique<testing::StrictMock<TestTokenBasedContextDecorator>>();
  EXPECT_CALL(*test_decorator, GetTokensToDecorate(testing::_))
      .WillOnce(testing::Return(std::vector<base::UnguessableToken>{token}));

  decorators.emplace(ContextualTaskContextSource::kFallbackTitle,
                     std::move(test_decorator));

  CompositeContextDecorator composite_decorator(std::move(decorators));

  ContextualTask task(base::Uuid::GenerateRandomV4());
  auto context = std::make_unique<ContextualTaskContext>(task);

  base::RunLoop run_loop;
  composite_decorator.DecorateContext(
      std::move(context), {}, std::move(params),
      base::BindOnce(
          [](base::OnceClosure quit_closure,
             std::unique_ptr<ContextualTaskContext> context) {
            // There should only be 1 attachment, because the second decorator
            // deduplicated it!
            EXPECT_EQ(1u, context->GetUrlAttachments().size());
            std::move(quit_closure).Run();
          },
          run_loop.QuitClosure()));
  run_loop.Run();
}

}  // namespace contextual_tasks
