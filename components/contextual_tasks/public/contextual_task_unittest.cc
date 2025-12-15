// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_tasks/public/contextual_task.h"

#include <string>

#include "base/uuid.h"
#include "components/sessions/core/session_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace contextual_tasks {

TEST(ContextualTaskTest, GetTaskId) {
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  ContextualTask task(task_id);
  EXPECT_EQ(task_id, task.GetTaskId());
}

TEST(ContextualTaskTest, IsEphemeral) {
  base::Uuid task_id1 = base::Uuid::GenerateRandomV4();
  base::Uuid task_id2 = base::Uuid::GenerateRandomV4();
  ContextualTask task1(task_id1, /*is_ephemeral=*/true);
  ContextualTask task2(task_id2, /*is_ephemeral=*/false);
  EXPECT_TRUE(task1.IsEphemeral());
  EXPECT_FALSE(task2.IsEphemeral());
  EXPECT_EQ(task_id1, task1.GetTaskId());
  EXPECT_EQ(task_id2, task2.GetTaskId());
}

TEST(ContextualTaskTest, AddAndRemoveUrlResource) {
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  ContextualTask task(task_id);
  UrlResource url_resource(base::Uuid::GenerateRandomV4(),
                           GURL("https://www.google.com"));

  EXPECT_TRUE(task.AddUrlResource(url_resource));
  EXPECT_FALSE(task.AddUrlResource(url_resource));

  EXPECT_EQ(1u, task.GetUrlResources().size());

  EXPECT_EQ(url_resource.url_id, task.RemoveUrl(url_resource.url));
  EXPECT_EQ(0u, task.GetUrlResources().size());
  EXPECT_EQ(std::nullopt, task.RemoveUrl(url_resource.url));
}

TEST(ContextualTaskTest, AddThread) {
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  ContextualTask task(task_id);
  ThreadType type = ThreadType::kAiMode;
  std::string server_id = "server_id";
  std::string title = "foo";
  std::string conversation_turn_id = "conversation_turn_id";

  task.AddThread(Thread(type, server_id, title, conversation_turn_id));
  EXPECT_TRUE(task.GetThread().has_value());
  EXPECT_EQ(task.GetThread()->type, type);
  EXPECT_EQ(task.GetThread()->server_id, server_id);
  EXPECT_EQ(task.GetThread()->title, title);
  EXPECT_EQ(task.GetThread()->conversation_turn_id, conversation_turn_id);

  std::string server_id_2 = "server_id_2";
  std::string title_2 = "foo_2";
  std::string conversation_turn_id_2 = "conversation_turn_id_2";
  task.AddThread(Thread(type, server_id_2, title_2, conversation_turn_id_2));
  EXPECT_TRUE(task.GetThread().has_value());
  EXPECT_EQ(task.GetThread()->type, type);
  EXPECT_EQ(task.GetThread()->server_id, server_id_2);
  EXPECT_EQ(task.GetThread()->title, title_2);
  EXPECT_EQ(task.GetThread()->conversation_turn_id, conversation_turn_id_2);
}

TEST(ContextualTaskTest, RemoveThread) {
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  ContextualTask task(task_id);
  ThreadType type = ThreadType::kAiMode;
  std::string server_id = "server_id";
  std::string title = "foo";
  std::string conversation_turn_id = "conversation_turn_id";

  task.AddThread(Thread(type, server_id, title, conversation_turn_id));
  EXPECT_TRUE(task.GetThread().has_value());

  task.RemoveThread(type, "wrong_server_id");
  EXPECT_TRUE(task.GetThread().has_value());

  task.RemoveThread(type, server_id);
  EXPECT_FALSE(task.GetThread().has_value());
}

TEST(ContextualTaskTest, AddAndRemoveTabId) {
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  ContextualTask task(task_id);
  SessionID tab_id = SessionID::FromSerializedValue(1);

  task.AddTabId(tab_id);
  task.AddTabId(tab_id);

  EXPECT_EQ(1u, task.GetTabIds().size());

  task.RemoveTabId(tab_id);
  EXPECT_EQ(0u, task.GetTabIds().size());
}

TEST(ContextualTaskTest, ClearTabIds) {
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  ContextualTask task(task_id);
  task.AddTabId(SessionID::FromSerializedValue(1));
  task.AddTabId(SessionID::FromSerializedValue(2));

  ASSERT_EQ(2u, task.GetTabIds().size());

  task.ClearTabIds();
  EXPECT_EQ(0u, task.GetTabIds().size());
}

TEST(ContextualTaskTest, SetAndGetTitle) {
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  ContextualTask task(task_id);
  std::string title = "test title";

  task.SetTitle(title);
  EXPECT_EQ(title, task.GetTitle());
}

TEST(ContextualTaskTest, SetUrlResourcesFromServer) {
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  ContextualTask task(task_id);

  // Add some initial resources
  task.AddUrlResource(UrlResource(GURL("https://initial1.com")));
  task.AddUrlResource(UrlResource(GURL("https://initial2.com")));
  EXPECT_EQ(2u, task.GetUrlResources().size());

  // Set new resources
  std::vector<UrlResource> new_resources;
  new_resources.emplace_back(base::Uuid::GenerateRandomV4(),
                             GURL("https://new1.com"));
  new_resources.emplace_back(base::Uuid::GenerateRandomV4(),
                             GURL("https://new2.com"));
  new_resources.emplace_back(base::Uuid::GenerateRandomV4(),
                             GURL("https://new3.com"));

  task.SetUrlResourcesFromServer(new_resources);

  EXPECT_EQ(3u, task.GetUrlResources().size());
  EXPECT_EQ(task.GetUrlResources()[0].url, GURL("https://new1.com"));
  EXPECT_EQ(task.GetUrlResources()[1].url, GURL("https://new2.com"));
  EXPECT_EQ(task.GetUrlResources()[2].url, GURL("https://new3.com"));

  // Set with an empty vector
  std::vector<UrlResource> empty_resources;
  task.SetUrlResourcesFromServer(empty_resources);
  EXPECT_EQ(0u, task.GetUrlResources().size());
}

}  // namespace contextual_tasks
