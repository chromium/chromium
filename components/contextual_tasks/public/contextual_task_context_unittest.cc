// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_tasks/public/contextual_task_context.h"

#include "base/uuid.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/url_deduplication/url_deduplication_helper.h"
#include "components/visited_url_ranking/public/url_visit_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace contextual_tasks {

TEST(ContextualTaskContextTest, ConstructFromContextualTask) {
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  ContextualTask task(task_id);
  GURL url1("https://google.com");
  GURL url2("https://youtube.com");
  task.AddUrlResource(UrlResource(base::Uuid::GenerateRandomV4(), url1));
  task.AddUrlResource(UrlResource(base::Uuid::GenerateRandomV4(), url2));

  ContextualTaskContext context(task);

  EXPECT_EQ(context.GetTaskId(), task_id);
  auto& attachments = context.GetUrlAttachments();
  ASSERT_EQ(attachments.size(), 2u);
  EXPECT_EQ(attachments[0].GetURL(), url1);
  EXPECT_EQ(attachments[1].GetURL(), url2);
}

TEST(ContextualTaskContextTest, ConstructFromContextualTask_WithMetadata) {
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  ContextualTask task(task_id);
  GURL url("https://google.com");
  UrlResource resource(base::Uuid::GenerateRandomV4(), url);
  resource.title = "Google";
  resource.tab_id = SessionID::FromSerializedValue(123);
  task.AddUrlResource(resource);

  ContextualTaskContext context(task);

  EXPECT_EQ(context.GetTaskId(), task_id);
  auto& attachments = context.GetUrlAttachments();
  ASSERT_EQ(attachments.size(), 1u);
  EXPECT_EQ(attachments[0].GetURL(), url);
  EXPECT_EQ(attachments[0].GetTitle(), u"Google");
  EXPECT_EQ(attachments[0].GetTabSessionId(),
            SessionID::FromSerializedValue(123));
}

TEST(ContextualTaskContextTest, ContainsURL) {
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  ContextualTask task(task_id);
  GURL url1("https://google.com");
  GURL url2("https://youtube.com");
  task.AddUrlResource(UrlResource(base::Uuid::GenerateRandomV4(), url1));
  task.AddUrlResource(UrlResource(base::Uuid::GenerateRandomV4(), url2));

  ContextualTaskContext context(task);
  auto deduplication_helper =
      visited_url_ranking::CreateDefaultURLDeduplicationHelper();

  EXPECT_TRUE(context.ContainsURL(GURL("https://google.com/"),
                                  deduplication_helper.get()));
  EXPECT_TRUE(context.ContainsURL(GURL("https://www.google.com"),
                                  deduplication_helper.get()));
  EXPECT_TRUE(context.ContainsURL(GURL("http://google.com"),
                                  deduplication_helper.get()));
  EXPECT_FALSE(context.ContainsURL(GURL("https://example.com"),
                                   deduplication_helper.get()));
}

}  // namespace contextual_tasks
