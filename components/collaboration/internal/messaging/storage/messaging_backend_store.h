// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COLLABORATION_INTERNAL_MESSAGING_STORAGE_MESSAGING_BACKEND_STORE_H_
#define COMPONENTS_COLLABORATION_INTERNAL_MESSAGING_STORAGE_MESSAGING_BACKEND_STORE_H_

#include <set>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/collaboration/internal/messaging/storage/protocol/message.pb.h"
#include "components/data_sharing/public/group_data.h"

namespace collaboration::messaging {

enum class DirtyType {
  kNone = 0,
  kMessageOnly = 1 << 0,  // Used for showing instant message.
  kDot = 1 << 1,          // Used for showing dot indicator.
  kChip = 1 << 2,         // Used for showing chip indicator (Desktop only).
  kTombstoned = 1 << 3,  // Used for showing messages for removed entities (tabs
                         // and tab groups).
  kDotAndChip = kDot | kChip,  // Used for showing both dot and chip indicator.
  kTombstonedAndInstantMessage =
      kTombstoned | kMessageOnly,  // Used for tracking both persistent and
                                   // instant message for tombstoned entities.
  kAll =
      kMessageOnly | kDot | kChip | kTombstoned,  // Used for all dirty types.
};

class MessagingBackendStore {
 public:
  MessagingBackendStore() = default;
  virtual ~MessagingBackendStore() = default;

  // Load the database and provide a callback to indicate whether it's
  // successful.
  virtual void Initialize(
      base::OnceCallback<void(bool)> on_initialized_callback) = 0;

  // Return if there are any dirty messages in any groups.
  virtual bool HasAnyDirtyMessages(DirtyType dirty_type) = 0;

  // Clear the dirty message given for a single tab.
  virtual void ClearDirtyMessageForTab(
      const data_sharing::GroupId& collaboration_id,
      const base::Uuid& tab_id,
      DirtyType dirty_type) = 0;

  // Clear all dirty tab messages for a group.
  virtual std::vector<collaboration_pb::Message> ClearDirtyTabMessagesForGroup(
      const data_sharing::GroupId& collaboration_id) = 0;

  // Clear the dirty message for a single message.
  virtual void ClearDirtyMessage(const base::Uuid uuid,
                                 DirtyType dirty_type) = 0;

  // Get all dirty messages for dirty_type. If no dirty_type is supplied, all
  // the messages will be returned.
  virtual std::vector<collaboration_pb::Message> GetDirtyMessages(
      std::optional<DirtyType> dirty_type) = 0;

  // Get the dirty messages for a group.
  virtual std::vector<collaboration_pb::Message> GetDirtyMessagesForGroup(
      const data_sharing::GroupId& collaboration_id,
      DirtyType dirty_type) = 0;

  // Get the dirty message
  virtual std::optional<collaboration_pb::Message> GetDirtyMessageForTab(
      const data_sharing::GroupId& collaboration_id,
      const base::Uuid& tab_id,
      DirtyType dirty_type) = 0;

  // Get cutoff duration for recent message.
  virtual base::TimeDelta GetRecentMessageCutoffDuration() = 0;

  // Set cutoff duration for recent message.
  virtual void SetRecentMessageCutoffDuration(base::TimeDelta time_delta) = 0;

  // Get the recent messages for a group. The newest message is first in the
  // result.
  virtual std::vector<collaboration_pb::Message> GetRecentMessagesForGroup(
      const data_sharing::GroupId& collaboration_id) = 0;

  // Add a message to the store.
  virtual void AddMessage(const collaboration_pb::Message& message) = 0;

  // Remove a message from the store.
  virtual void RemoveMessages(const std::set<std::string>& message_ids) = 0;

  // Remove all messages from the store.
  virtual void RemoveAllMessages() = 0;
};

}  // namespace collaboration::messaging

#endif  // COMPONENTS_COLLABORATION_INTERNAL_MESSAGING_STORAGE_MESSAGING_BACKEND_STORE_H_
