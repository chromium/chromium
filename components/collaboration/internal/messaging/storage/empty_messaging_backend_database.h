// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COLLABORATION_INTERNAL_MESSAGING_STORAGE_EMPTY_MESSAGING_BACKEND_DATABASE_H_
#define COMPONENTS_COLLABORATION_INTERNAL_MESSAGING_STORAGE_EMPTY_MESSAGING_BACKEND_DATABASE_H_

#include "components/collaboration/internal/messaging/storage/messaging_backend_database.h"

namespace collaboration::messaging {

class EmptyMessagingBackendDatabase : public MessagingBackendDatabase {
 public:
  EmptyMessagingBackendDatabase();
  ~EmptyMessagingBackendDatabase() override;

  void Initialize(DBLoadedCallback db_loaded_callback) override;

  void Update(const collaboration_pb::Message& message) override;

  void Delete(const std::vector<std::string>& message_uuids) override;

  void DeleteAllData() override;
};

}  // namespace collaboration::messaging

#endif  // COMPONENTS_COLLABORATION_INTERNAL_MESSAGING_STORAGE_EMPTY_MESSAGING_BACKEND_DATABASE_H_
