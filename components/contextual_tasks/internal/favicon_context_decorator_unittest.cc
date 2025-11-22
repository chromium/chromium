// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_tasks/internal/favicon_context_decorator.h"

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/contextual_tasks/public/contextual_task_context.h"
#include "components/favicon/core/test/mock_favicon_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "url/gurl.h"

using testing::_;
using testing::Return;

namespace contextual_tasks {

class FaviconContextDecoratorTest : public testing::Test {
 public:
  FaviconContextDecoratorTest() = default;
  ~FaviconContextDecoratorTest() override = default;

 protected:
  base::test::TaskEnvironment task_environment_;
  favicon::MockFaviconService mock_favicon_service_;
};

TEST_F(FaviconContextDecoratorTest, DecorateContextWithFavicons) {
  const GURL kUrl1("https://www.google.com");
  const UrlResource url_resource1(base::Uuid::GenerateRandomV4(), kUrl1);
  const GURL kUrl2("https://www.chromium.org");
  const UrlResource url_resource2(base::Uuid::GenerateRandomV4(), kUrl2);
  const GURL kIconUrl("https://www.google.com/favicon.ico");
  const gfx::Image kTestImage = gfx::test::CreateImage(16, 16);

  ContextualTask task(base::Uuid::GenerateRandomV4());
  task.AddUrlResource(url_resource1);
  task.AddUrlResource(url_resource2);
  auto context = std::make_unique<ContextualTaskContext>(task);

  EXPECT_CALL(mock_favicon_service_, GetFaviconImageForPageURL(kUrl1, _, _))
      .WillOnce([&](const GURL& page_url,
                    favicon_base::FaviconImageCallback callback,
                    base::CancelableTaskTracker* tracker) {
        favicon_base::FaviconImageResult result;
        result.image = kTestImage;
        result.icon_url = kIconUrl;
        std::move(callback).Run(result);
        return base::CancelableTaskTracker::kBadTaskId;
      });
  EXPECT_CALL(mock_favicon_service_, GetFaviconImageForPageURL(kUrl2, _, _))
      .WillOnce([&](const GURL& page_url,
                    favicon_base::FaviconImageCallback callback,
                    base::CancelableTaskTracker* tracker) {
        // No favicon for this URL.
        std::move(callback).Run(favicon_base::FaviconImageResult());
        return base::CancelableTaskTracker::kBadTaskId;
      });

  FaviconContextDecorator decorator(&mock_favicon_service_);
  base::test::TestFuture<std::unique_ptr<ContextualTaskContext>> future;
  decorator.DecorateContext(std::move(context), nullptr, future.GetCallback());

  auto decorated_context = future.Take();
  ASSERT_TRUE(decorated_context);

  auto& attachments = decorated_context->GetMutableUrlAttachmentsForTesting();
  ASSERT_EQ(attachments.size(), 2u);

  auto& attachment1_data =
      attachments[0].GetMutableDecoratorDataForTesting().favicon_data;
  EXPECT_FALSE(attachment1_data.image.IsEmpty());
  EXPECT_EQ(attachment1_data.icon_url, kIconUrl);
  EXPECT_TRUE(gfx::test::AreImagesEqual(attachment1_data.image, kTestImage));

  auto& attachment2_data =
      attachments[1].GetMutableDecoratorDataForTesting().favicon_data;
  EXPECT_TRUE(attachment2_data.image.IsEmpty());
  EXPECT_TRUE(attachment2_data.icon_url.is_empty());
}

TEST_F(FaviconContextDecoratorTest, NoFaviconService) {
  const GURL kUrl("https://www.google.com");
  const UrlResource url_resource(base::Uuid::GenerateRandomV4(), kUrl);
  ContextualTask task(base::Uuid::GenerateRandomV4());
  task.AddUrlResource(url_resource);
  auto context = std::make_unique<ContextualTaskContext>(task);

  FaviconContextDecorator decorator(nullptr);
  base::test::TestFuture<std::unique_ptr<ContextualTaskContext>> future;
  decorator.DecorateContext(std::move(context), nullptr, future.GetCallback());

  auto decorated_context = future.Take();
  ASSERT_TRUE(decorated_context);

  auto& attachments = decorated_context->GetMutableUrlAttachmentsForTesting();
  ASSERT_EQ(attachments.size(), 1u);
  const auto& attachment_data =
      attachments[0].GetMutableDecoratorDataForTesting().favicon_data;
  EXPECT_TRUE(attachment_data.image.IsEmpty());
  EXPECT_TRUE(attachment_data.icon_url.is_empty());
}

TEST_F(FaviconContextDecoratorTest, NoAttachments) {
  ContextualTask task(base::Uuid::GenerateRandomV4());
  auto context = std::make_unique<ContextualTaskContext>(task);

  FaviconContextDecorator decorator(&mock_favicon_service_);
  base::test::TestFuture<std::unique_ptr<ContextualTaskContext>> future;
  decorator.DecorateContext(std::move(context), nullptr, future.GetCallback());

  auto decorated_context = future.Take();
  ASSERT_TRUE(decorated_context);
  EXPECT_TRUE(decorated_context->GetUrlAttachments().empty());
}

}  // namespace contextual_tasks
