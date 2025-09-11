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

TEST(ContextualTaskTest, AddAndRemoveUrl) {
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  ContextualTask task(task_id);
  GURL url("https://www.google.com");

  task.AddUrl(url);
  task.AddUrl(url);

  EXPECT_EQ(1u, task.GetUrls().size());

  task.RemoveUrl(url);
  EXPECT_EQ(0u, task.GetUrls().size());
}

TEST(ContextualTaskTest, AddChat) {
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  ContextualTask task(task_id);
  ChatType type = ChatType::kAiMode;
  std::string server_id = "server_id";
  std::string title = "foo";

  task.AddChat(type, server_id, title);
  EXPECT_TRUE(task.GetChat().has_value());
  EXPECT_EQ(task.GetChat()->type, type);
  EXPECT_EQ(task.GetChat()->server_id, server_id);
  EXPECT_EQ(task.GetChat()->title, title);

  std::string server_id_2 = "server_id_2";
  std::string title_2 = "foo_2";
  task.AddChat(type, server_id_2, title_2);
  EXPECT_TRUE(task.GetChat().has_value());
  EXPECT_EQ(task.GetChat()->type, type);
  EXPECT_EQ(task.GetChat()->server_id, server_id_2);
  EXPECT_EQ(task.GetChat()->title, title_2);
}

TEST(ContextualTaskTest, RemoveChat) {
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  ContextualTask task(task_id);
  ChatType type = ChatType::kAiMode;
  std::string server_id = "server_id";

  task.AddChat(type, server_id, "foo");
  EXPECT_TRUE(task.GetChat().has_value());

  task.RemoveChat(type, "wrong_server_id");
  EXPECT_TRUE(task.GetChat().has_value());

  task.RemoveChat(type, server_id);
  EXPECT_FALSE(task.GetChat().has_value());
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

}  // namespace contextual_tasks
