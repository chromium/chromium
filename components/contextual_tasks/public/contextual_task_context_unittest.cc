// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_tasks/public/contextual_task_context.h"

#include "base/uuid.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/contextual_tasks/public/utils.h"
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
  GURL url2("https://www.youtube.com/watch?v=123");
  GURL url3 = GURL();  // Invalid URL
  task.AddUrlResource(UrlResource(base::Uuid::GenerateRandomV4(), url1));
  task.AddUrlResource(UrlResource(base::Uuid::GenerateRandomV4(), url2));
  task.AddUrlResource(UrlResource(base::Uuid::GenerateRandomV4(), url3));

  ContextualTaskContext context(task);
  auto deduplication_helper = CreateURLDeduplicationHelperForContextualTask();

  // Exact match.
  EXPECT_TRUE(context.ContainsURL(url1, deduplication_helper.get()));
  EXPECT_TRUE(context.ContainsURL(url2, deduplication_helper.get()));

  // Variations in scheme.
  EXPECT_TRUE(context.ContainsURL(GURL("https://google.com/"),
                                  deduplication_helper.get()));
  EXPECT_TRUE(context.ContainsURL(GURL("https://www.google.com"),
                                  deduplication_helper.get()));

  // Variations in port.
  EXPECT_TRUE(context.ContainsURL(GURL("http://google.com:8080"),
                                  deduplication_helper.get()));

  // Variations in username/password.
  EXPECT_TRUE(context.ContainsURL(GURL("https://user:pass@google.com:443"),
                                  deduplication_helper.get()));
  // Variations in ref.
  EXPECT_TRUE(
      context.ContainsURL(GURL("https://www.youtube.com/watch?v=123#t=30s"),
                          deduplication_helper.get()));

  // Non-matching URLs.
  EXPECT_FALSE(context.ContainsURL(GURL("https://example.com"),
                                   deduplication_helper.get()));
  // Different path.
  EXPECT_FALSE(context.ContainsURL(GURL("https://google.com/maps"),
                                   deduplication_helper.get()));
  EXPECT_FALSE(context.ContainsURL(GURL("https://www.youtube.com"),
                                   deduplication_helper.get()));

  // Different query param.
  EXPECT_FALSE(context.ContainsURL(GURL("https://www.youtube.com/watch?v=234"),
                                   deduplication_helper.get()));
  EXPECT_FALSE(context.ContainsURL(GURL("https://www.youtube.com/watch"),
                                   deduplication_helper.get()));

  // Invalid URLs.
  EXPECT_FALSE(context.ContainsURL(GURL(), deduplication_helper.get()));
  EXPECT_FALSE(
      context.ContainsURL(GURL("invalid-url"), deduplication_helper.get()));
}

TEST(ContextualTaskContextTest, GetMatchingUrlAttachments) {
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  ContextualTask task(task_id);
  GURL url1("https://google.com");
  GURL url2("https://www.youtube.com/watch?v=123");
  GURL url3("https://google.com/maps");
  GURL url4 = GURL();  // Invalid URL
  task.AddUrlResource(UrlResource(base::Uuid::GenerateRandomV4(), url1));
  task.AddUrlResource(UrlResource(base::Uuid::GenerateRandomV4(), url2));
  task.AddUrlResource(UrlResource(base::Uuid::GenerateRandomV4(), url3));
  task.AddUrlResource(UrlResource(base::Uuid::GenerateRandomV4(), url4));

  ContextualTaskContext context(task);
  auto deduplication_helper = CreateURLDeduplicationHelperForContextualTask();

  // Exact match.
  std::vector<const UrlAttachment*> matches1 =
      context.GetMatchingUrlAttachments(url1, deduplication_helper.get());
  ASSERT_EQ(matches1.size(), 1u);
  EXPECT_EQ(matches1[0]->GetURL(), url1);

  // Match with variations of URL.
  std::vector<const UrlAttachment*> matches2 =
      context.GetMatchingUrlAttachments(GURL("http://google.com:8080"),
                                        deduplication_helper.get());
  ASSERT_EQ(matches2.size(), 1u);
  EXPECT_EQ(matches2[0]->GetURL(), url1);

  // Non-matching domain.
  std::vector<const UrlAttachment*> matches3 =
      context.GetMatchingUrlAttachments(GURL("https://example.com"),
                                        deduplication_helper.get());
  EXPECT_TRUE(matches3.empty());

  // Non-matching path.
  std::vector<const UrlAttachment*> matches4 =
      context.GetMatchingUrlAttachments(GURL("https://google.com/webhp"),
                                        deduplication_helper.get());
  EXPECT_TRUE(matches4.empty());

  // Empty URL input.
  std::vector<const UrlAttachment*> matches5 =
      context.GetMatchingUrlAttachments(GURL(), deduplication_helper.get());
  EXPECT_TRUE(matches5.empty());

  // Invalid URL input.
  std::vector<const UrlAttachment*> matches6 =
      context.GetMatchingUrlAttachments(GURL("invalid-url"),
                                        deduplication_helper.get());
  EXPECT_TRUE(matches6.empty());
}

}  // namespace contextual_tasks
