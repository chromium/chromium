// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COLLABORATION_INTERNAL_MESSAGING_STORAGE_MESSAGING_BACKEND_STORE_IMPL_H_
#define COMPONENTS_COLLABORATION_INTERNAL_MESSAGING_STORAGE_MESSAGING_BACKEND_STORE_IMPL_H_

#include <map>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "components/collaboration/internal/messaging/storage/messaging_backend_database.h"
#include "components/collaboration/internal/messaging/storage/messaging_backend_store.h"

namespace collaboration::messaging {

// TODO(crbug.com/379870772): store message uuid instead of message to avoid
// data duplication. Query database with message uuid.
struct MessagesPerGroup {
  MessagesPerGroup();
  ~MessagesPerGroup();

  // Store the latest tab messages, key by sync_tab_id.
  std::map<base::Uuid, collaboration_pb::Message> tab_messages;

  // Store the latest tab group messages, key by event type.
  std::map<collaboration_pb::EventType, collaboration_pb::Message>
      tab_group_messages;

  // Store the collaboration messages.
  std::vector<collaboration_pb::Message> collaboration_messages;
};

class MessagingBackendStoreImpl : public MessagingBackendStore {
 public:
  using OnLoadCallback = base::OnceCallback<void(bool)>;

  MessagingBackendStoreImpl(std::unique_ptr<MessagingBackendDatabase> database);
  ~MessagingBackendStoreImpl() override;

  // MessagingBackendStore:
  void Initialize(
      base::OnceCallback<void(bool)> on_initialized_callback) override;
  bool HasAnyDirtyMessages(DirtyType dirty_type) override;
  void ClearDirtyMessageForTab(const data_sharing::GroupId& collaboration_id,
                               const base::Uuid& tab_id,
                               DirtyType dirty_type) override;
  std::vector<collaboration_pb::Message> ClearDirtyTabMessagesForGroup(
      const data_sharing::GroupId& collaboration_id) override;
  void ClearDirtyMessage(const base::Uuid uuid, DirtyType dirty_type) override;
  std::vector<collaboration_pb::Message> GetDirtyMessages(
      std::optional<DirtyType> dirty_type) override;
  std::vector<collaboration_pb::Message> GetDirtyMessagesForGroup(
      const data_sharing::GroupId& collaboration_id,
      DirtyType dirty_type) override;
  std::optional<collaboration_pb::Message> GetDirtyMessageForTab(
      const data_sharing::GroupId& collaboration_id,
      const base::Uuid& tab_id,
      DirtyType dirty_type) override;
  base::TimeDelta GetRecentMessageCutoffDuration() override;
  void SetRecentMessageCutoffDuration(base::TimeDelta time_delta) override;

  std::vector<collaboration_pb::Message> GetRecentMessagesForGroup(
      const data_sharing::GroupId& collaboration_id) override;
  void AddMessage(const collaboration_pb::Message& message) override;
  void RemoveMessages(const std::set<std::string>& message_ids) override;
  void RemoveAllMessages() override;

  std::optional<MessagesPerGroup*> GetMessagesPerGroupForTesting(
      const data_sharing::GroupId& collaboration_id);

  std::optional<collaboration_pb::Message> GetLastMessageForTesting();

 private:
  std::optional<MessagesPerGroup*> GetMessagesPerGroup(
      const data_sharing::GroupId& collaboration_id);

  // Traverse all the messages and execute the callback for each message.
  // If the callback returns false then stop the traversal and return early.
  void TraverseMessages(
      base::RepeatingCallback<bool(collaboration_pb::Message& message)>
          message_callback);

  // Traverse all the messages in a collaboration group and execute the callback
  // for each message. If the callback returns false then stop the traversal and
  // return early.
  void TraverseMessagesForGroup(
      MessagesPerGroup* messages_per_group,
      base::RepeatingCallback<bool(collaboration_pb::Message& message)>
          message_callback);

  void OnDatabaseLoaded(
      OnLoadCallback on_load_callback,
      bool success,
      const std::map<std::string, collaboration_pb::Message>& data);

  void DeleteExpiredMessages();

  // Store all the messages group by collaboration group.
  std::map<data_sharing::GroupId, std::unique_ptr<MessagesPerGroup>> messages_;

  // Stores special one-off messages that have no relation to a collaboration.
  std::vector<collaboration_pb::Message> ungrouped_messages_;

  std::unique_ptr<MessagingBackendDatabase> database_;

  std::unique_ptr<base::RepeatingTimer> delete_expired_messages_timer_;

  // Max age of GetRecentMessages should return.
  base::TimeDelta recent_message_cutoff_duration_ = base::Days(31);

  // Return the last message from AddMessage() for testing purpose.
  std::optional<collaboration_pb::Message> last_added_message_for_testing_;

  base::WeakPtrFactory<MessagingBackendStoreImpl> weak_ptr_factory_{this};
};

}  // namespace collaboration::messaging

#endif  // COMPONENTS_COLLABORATION_INTERNAL_MESSAGING_STORAGE_MESSAGING_BACKEND_STORE_IMPL_H_
