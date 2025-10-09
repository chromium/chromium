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

TEST(ContextualTaskTest, AddAndRemoveUrlResource) {
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  ContextualTask task(task_id);
  UrlResource url_resource(base::Uuid::GenerateRandomV4(),
                           GURL("https://www.google.com"));

  task.AddUrlResource(url_resource);
  task.AddUrlResource(url_resource);

  EXPECT_EQ(1u, task.GetUrlResources().size());

  task.RemoveUrl(url_resource.url);
  EXPECT_EQ(0u, task.GetUrlResources().size());
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

TEST(ContextualTaskTest, AddAndRemoveSessionId) {
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  ContextualTask task(task_id);
  SessionID session_id = SessionID::FromSerializedValue(1);

  task.AddSessionId(session_id);
  task.AddSessionId(session_id);

  EXPECT_EQ(1u, task.GetSessionIds().size());

  task.RemoveSessionId(session_id);
  EXPECT_EQ(0u, task.GetSessionIds().size());
}

TEST(ContextualTaskTest, SetAndGetTitle) {
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  ContextualTask task(task_id);
  std::string title = "test title";

  task.SetTitle(title);
  EXPECT_EQ(title, task.GetTitle());
}

}  // namespace contextual_tasks
