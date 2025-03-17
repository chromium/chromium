// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COLLABORATION_INTERNAL_MESSAGING_STORAGE_MESSAGING_BACKEND_DATABASE_H_
#define COMPONENTS_COLLABORATION_INTERNAL_MESSAGING_STORAGE_MESSAGING_BACKEND_DATABASE_H_

#include <map>
#include <vector>

#include "base/functional/callback.h"
#include "components/collaboration/internal/messaging/storage/protocol/message.pb.h"

namespace collaboration::messaging {

using DBLoadedCallback = base::OnceCallback<
    void(bool, const std::map<std::string, collaboration_pb::Message>&)>;

// Interface for messaging backend database.
class MessagingBackendDatabase {
 public:
  virtual ~MessagingBackendDatabase();

  // Initialize the database and invoke the callback when loaded.
  virtual void Initialize(DBLoadedCallback db_loaded_callback) = 0;

  // Insert or update a message.
  virtual void Update(const collaboration_pb::Message& message) = 0;

  // Delete messages with uuids.
  virtual void Delete(const std::vector<std::string>& message_uuids) = 0;

  // Delete all messages from the database.
  virtual void DeleteAllData() = 0;
};

}  // namespace collaboration::messaging

#endif  // COMPONENTS_COLLABORATION_INTERNAL_MESSAGING_STORAGE_MESSAGING_BACKEND_DATABASE_H_
