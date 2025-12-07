// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_tasks/internal/fallback_title_context_decorator.h"

#include "base/i18n/icu_util.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/contextual_tasks/public/contextual_task_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace contextual_tasks {

class FallbackTitleContextDecoratorTest : public testing::Test {
 public:
  FallbackTitleContextDecoratorTest() { base::i18n::InitializeICU(); }
  ~FallbackTitleContextDecoratorTest() override = default;

 protected:
  base::test::TaskEnvironment task_environment_;
  FallbackTitleContextDecorator decorator_;
};

TEST_F(FallbackTitleContextDecoratorTest, DecorateContextWithTitle) {
  ContextualTask task(base::Uuid::GenerateRandomV4());
  GURL url("https://www.google.com");
  task.AddUrlResource(UrlResource(base::Uuid::GenerateRandomV4(), url));
  auto context = std::make_unique<ContextualTaskContext>(task);

  base::RunLoop run_loop;
  decorator_.DecorateContext(
      std::move(context), nullptr,
      base::BindOnce(
          [](base::OnceClosure quit_closure,
             std::unique_ptr<ContextualTaskContext> context) {
            auto attachments = context->GetUrlAttachments();
            ASSERT_EQ(1u, attachments.size());
            EXPECT_EQ(GURL("https://www.google.com"), attachments[0].GetURL());
            EXPECT_FALSE(attachments[0].GetTitle().empty());
            EXPECT_EQ(u"google.com", attachments[0].GetTitle());
            std::move(quit_closure).Run();
          },
          run_loop.QuitClosure()));
  run_loop.Run();
}

}  // namespace contextual_tasks
