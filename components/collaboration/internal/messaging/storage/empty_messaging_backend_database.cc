// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/collaboration/internal/messaging/storage/empty_messaging_backend_database.h"

#include "base/task/single_thread_task_runner.h"

namespace collaboration::messaging {

EmptyMessagingBackendDatabase::EmptyMessagingBackendDatabase() = default;
EmptyMessagingBackendDatabase::~EmptyMessagingBackendDatabase() = default;

void EmptyMessagingBackendDatabase::Initialize(
    DBLoadedCallback db_loaded_callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(db_loaded_callback), true,
                     std::map<std::string, collaboration_pb::Message>()));
}

void EmptyMessagingBackendDatabase::Update(
    const collaboration_pb::Message& message) {}

void EmptyMessagingBackendDatabase::Delete(
    const std::vector<std::string>& message_uuids) {}

void EmptyMessagingBackendDatabase::DeleteAllData() {}

}  // namespace collaboration::messaging
