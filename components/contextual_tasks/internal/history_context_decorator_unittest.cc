// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_tasks/internal/history_context_decorator.h"

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/contextual_tasks/public/contextual_task_context.h"
#include "components/history/core/browser/history_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::_;

namespace contextual_tasks {

class MockHistoryService : public history::HistoryService {
 public:
  MockHistoryService() = default;
  ~MockHistoryService() override = default;

  MOCK_METHOD(base::CancelableTaskTracker::TaskId,
              QueryURL,
              (const GURL&,
               history::HistoryService::QueryURLCallback,
               base::CancelableTaskTracker*),
              (override));
};

class HistoryContextDecoratorTest : public testing::Test {
 public:
  HistoryContextDecoratorTest() = default;
  ~HistoryContextDecoratorTest() override = default;

 protected:
  base::test::TaskEnvironment task_environment_;
  MockHistoryService mock_history_service_;
};

TEST_F(HistoryContextDecoratorTest, DecorateContextWithHistory) {
  const GURL kUrl1("https://www.google.com");
  const UrlResource url_resource1(base::Uuid::GenerateRandomV4(), kUrl1);
  const GURL kUrl2("https://www.chromium.org");
  const UrlResource url_resource2(base::Uuid::GenerateRandomV4(), kUrl2);
  const std::u16string kTitle1 = u"Google";

  ContextualTask task(base::Uuid::GenerateRandomV4());
  task.AddUrlResource(url_resource1);
  task.AddUrlResource(url_resource2);
  auto context = std::make_unique<ContextualTaskContext>(task);

  EXPECT_CALL(mock_history_service_, QueryURL(kUrl1, _, _))
      .WillOnce([&](const GURL& url,
                    history::HistoryService::QueryURLCallback callback,
                    base::CancelableTaskTracker* tracker) {
        history::QueryURLResult result;
        result.success = true;
        result.row.set_title(kTitle1);
        std::move(callback).Run(std::move(result));
        return base::CancelableTaskTracker::kBadTaskId;
      });
  EXPECT_CALL(mock_history_service_, QueryURL(kUrl2, _, _))
      .WillOnce([&](const GURL& url,
                    history::HistoryService::QueryURLCallback callback,
                    base::CancelableTaskTracker* tracker) {
        // No title for this URL.
        std::move(callback).Run(history::QueryURLResult());
        return base::CancelableTaskTracker::kBadTaskId;
      });

  HistoryContextDecorator decorator(&mock_history_service_);
  base::test::TestFuture<std::unique_ptr<ContextualTaskContext>> future;
  decorator.DecorateContext(std::move(context), nullptr, future.GetCallback());

  auto decorated_context = future.Take();
  ASSERT_TRUE(decorated_context);

  auto& attachments = decorated_context->GetMutableUrlAttachmentsForTesting();
  ASSERT_EQ(attachments.size(), 2u);

  auto& attachment1_data =
      attachments[0].GetMutableDecoratorDataForTesting().history_data;
  EXPECT_EQ(attachment1_data.title, kTitle1);

  auto& attachment2_data =
      attachments[1].GetMutableDecoratorDataForTesting().history_data;
  EXPECT_TRUE(attachment2_data.title.empty());
}

TEST_F(HistoryContextDecoratorTest, NoHistoryService) {
  const GURL kUrl("https://www.google.com");
  const UrlResource url_resource(base::Uuid::GenerateRandomV4(), kUrl);
  ContextualTask task(base::Uuid::GenerateRandomV4());
  task.AddUrlResource(url_resource);
  auto context = std::make_unique<ContextualTaskContext>(task);

  HistoryContextDecorator decorator(nullptr);
  base::test::TestFuture<std::unique_ptr<ContextualTaskContext>> future;
  decorator.DecorateContext(std::move(context), nullptr, future.GetCallback());

  auto decorated_context = future.Take();
  ASSERT_TRUE(decorated_context);

  auto& attachments = decorated_context->GetMutableUrlAttachmentsForTesting();
  ASSERT_EQ(attachments.size(), 1u);
  const auto& attachment_data =
      attachments[0].GetMutableDecoratorDataForTesting().history_data;
  EXPECT_TRUE(attachment_data.title.empty());
}

TEST_F(HistoryContextDecoratorTest, NoAttachments) {
  ContextualTask task(base::Uuid::GenerateRandomV4());
  auto context = std::make_unique<ContextualTaskContext>(task);

  HistoryContextDecorator decorator(&mock_history_service_);
  base::test::TestFuture<std::unique_ptr<ContextualTaskContext>> future;
  decorator.DecorateContext(std::move(context), nullptr, future.GetCallback());

  auto decorated_context = future.Take();
  ASSERT_TRUE(decorated_context);
  EXPECT_TRUE(decorated_context->GetUrlAttachments().empty());
}

}  // namespace contextual_tasks
