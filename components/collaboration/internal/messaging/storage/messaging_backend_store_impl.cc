// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/collaboration/internal/messaging/storage/messaging_backend_store_impl.h"

#include <algorithm>

#include "base/containers/contains.h"
#include "base/task/single_thread_task_runner.h"
#include "components/collaboration/internal/messaging/storage/collaboration_message_util.h"
#include "components/collaboration/internal/messaging/storage/messaging_backend_database_impl.h"

namespace collaboration::messaging {

namespace {

// Interval of the timer to clean up old messages.
constexpr base::TimeDelta kMessageCleanUpDuration = base::Days(1);

// TTL of the database messages. Expired messages will be deleted from the
// database.
constexpr base::TimeDelta kMessageExpireDuration = base::Days(31);

// Return if the message is more recent than other_message.
bool IsMessageMoreRecent(const collaboration_pb::Message& message,
                         const collaboration_pb::Message& other_message) {
  return message.event_timestamp() >= other_message.event_timestamp();
}

// Return if the message is dirty for dirty_type
bool IsDirty(const collaboration_pb::Message& message, DirtyType dirty_type) {
  return message.dirty() & static_cast<int>(dirty_type);
}

// Return the value of dirty bits after dirty_type is clear.
int ClearDirty(const collaboration_pb::Message& message, DirtyType dirty_type) {
  return message.dirty() & ~static_cast<int>(dirty_type);
}

bool IsMessageExpired(const collaboration_pb::Message& message,
                      const base::Time& now) {
  const time_t expiration_time = (now - kMessageExpireDuration).ToTimeT();
  return message.event_timestamp() < expiration_time;
}

bool IsMemberAddRemoveMessage(const collaboration_pb::Message& message) {
  return message.event_type() == collaboration_pb::COLLABORATION_MEMBER_ADDED ||
         message.event_type() == collaboration_pb::COLLABORATION_MEMBER_REMOVED;
}

std::pair<std::optional<std::string>, std::optional<collaboration_pb::Message>>
AddOrReplaceCollaborationMessage(
    std::vector<collaboration_pb::Message>& collaboration_messages,
    const collaboration_pb::Message& new_message) {
  std::optional<std::string> message_id_to_remove;
  std::optional<collaboration_pb::Message> message_to_update;

  // Loop through the messages and find if there is a message for the same
  // affected user. If yes, erase the older of the two message in favor of
  // keeping the new one.
  bool found_matching_message = false;
  for (auto it = collaboration_messages.begin();
       it != collaboration_messages.end(); it++) {
    auto message = *it;
    if (IsMemberAddRemoveMessage(message) &&
        IsMemberAddRemoveMessage(new_message) &&
        message.affected_user_gaia_id() ==
            new_message.affected_user_gaia_id()) {
      found_matching_message = true;
      if (IsMessageMoreRecent(new_message, message)) {
        message_id_to_remove = message.uuid();
        message_to_update = new_message;
        collaboration_messages.erase(it);
        collaboration_messages.emplace_back(new_message);
      }
      break;
    }
  }

  // If we didn't find a matching message for the same user, just add the
  // message.
  if (!found_matching_message) {
    message_to_update = new_message;
    collaboration_messages.emplace_back(new_message);
  }

  return std::make_pair<>(message_id_to_remove, message_to_update);
}

}  // namespace

MessagesPerGroup::MessagesPerGroup() = default;

MessagesPerGroup::~MessagesPerGroup() = default;

MessagingBackendStoreImpl::MessagingBackendStoreImpl(
    std::unique_ptr<MessagingBackendDatabase> database)
    : database_(std::move(database)) {}
MessagingBackendStoreImpl::~MessagingBackendStoreImpl() = default;

void MessagingBackendStoreImpl::Initialize(
    base::OnceCallback<void(bool)> on_initialized_callback) {
  database_->Initialize(base::BindOnce(
      &MessagingBackendStoreImpl::OnDatabaseLoaded,
      weak_ptr_factory_.GetWeakPtr(), std::move(on_initialized_callback)));
}

bool MessagingBackendStoreImpl::HasAnyDirtyMessages(DirtyType dirty_type) {
  bool dirty = false;
  TraverseMessages(base::BindRepeating(
      [](bool* dirty, DirtyType dirty_type,
         collaboration_pb::Message& message) {
        if (IsDirty(message, dirty_type)) {
          *dirty = true;
          return false;
        }
        return true;
      },
      &dirty, dirty_type));
  return dirty;
}

void MessagingBackendStoreImpl::ClearDirtyMessageForTab(
    const data_sharing::GroupId& collaboration_id,
    const base::Uuid& tab_id,
    DirtyType dirty_type) {
  std::optional<MessagesPerGroup*> messages_per_group =
      GetMessagesPerGroup(collaboration_id);
  if (!messages_per_group) {
    return;
  }

  auto& tab_messages = messages_per_group.value()->tab_messages;

  auto it = tab_messages.find(tab_id);
  if (it == tab_messages.end()) {
    return;
  } else {
    it->second.set_dirty(ClearDirty(it->second, dirty_type));
    database_->Update(it->second);
  }
}

void MessagingBackendStoreImpl::ClearDirtyMessage(const base::Uuid uuid,
                                                  DirtyType dirty_type) {
  TraverseMessages(base::BindRepeating(
      [](const base::Uuid uuid, DirtyType dirty_type,
         MessagingBackendDatabase* database,
         collaboration_pb::Message& message) {
        if (message.uuid() == uuid.AsLowercaseString()) {
          message.set_dirty(ClearDirty(message, dirty_type));
          database->Update(message);
          return false;
        }
        return true;
      },
      uuid, dirty_type, database_.get()));
}

std::vector<collaboration_pb::Message>
MessagingBackendStoreImpl::ClearDirtyTabMessagesForGroup(
    const data_sharing::GroupId& collaboration_id) {
  std::vector<collaboration_pb::Message> cleared_messages;
  std::optional<MessagesPerGroup*> messages_per_group =
      GetMessagesPerGroup(collaboration_id);
  if (!messages_per_group) {
    return cleared_messages;
  }

  // Clear dirty bit for all tab messages.
  for (auto& [message_id, message] : messages_per_group.value()->tab_messages) {
    bool was_dirty = false;
    if (IsDirty(message, DirtyType::kDotAndChip)) {
      message.set_dirty(ClearDirty(message, DirtyType::kDotAndChip));
      was_dirty = true;
    }
    if (IsDirty(message, DirtyType::kTombstoned)) {
      message.set_dirty(ClearDirty(message, DirtyType::kTombstoned));
      was_dirty = true;
    }
    if (was_dirty) {
      cleared_messages.emplace_back(message);
      database_->Update(message);
    }
  }

  return cleared_messages;
}

std::vector<collaboration_pb::Message>
MessagingBackendStoreImpl::GetDirtyMessages(DirtyType dirty_type) {
  std::vector<collaboration_pb::Message> result;
  TraverseMessages(base::BindRepeating(
      [](std::vector<collaboration_pb::Message>* result, DirtyType dirty_type,
         collaboration_pb::Message& message) {
        if (IsDirty(message, dirty_type)) {
          result->push_back(message);
        }
        return true;
      },
      &result, dirty_type));
  return result;
}

std::vector<collaboration_pb::Message>
MessagingBackendStoreImpl::GetDirtyMessagesForGroup(
    const data_sharing::GroupId& collaboration_id,
    DirtyType dirty_type) {
  std::optional<MessagesPerGroup*> messages_per_group =
      GetMessagesPerGroup(collaboration_id);
  if (!messages_per_group) {
    return std::vector<collaboration_pb::Message>();
  }

  std::vector<collaboration_pb::Message> result;

  TraverseMessagesForGroup(
      messages_per_group.value(),
      base::BindRepeating(
          [](std::vector<collaboration_pb::Message>* result,
             DirtyType dirty_type, collaboration_pb::Message& message) {
            if (IsDirty(message, dirty_type)) {
              result->push_back(message);
            }
            return true;
          },
          &result, dirty_type));

  return result;
}

std::optional<collaboration_pb::Message>
MessagingBackendStoreImpl::GetDirtyMessageForTab(
    const data_sharing::GroupId& collaboration_id,
    const base::Uuid& tab_id,
    DirtyType dirty_type) {
  std::optional<MessagesPerGroup*> messages_per_group =
      GetMessagesPerGroup(collaboration_id);
  if (!messages_per_group) {
    return std::nullopt;
  }

  auto& tab_messages = messages_per_group.value()->tab_messages;

  auto it = tab_messages.find(tab_id);
  if (it == tab_messages.end()) {
    return std::nullopt;
  }

  collaboration_pb::Message& message = it->second;
  if (IsDirty(message, dirty_type)) {
    return message;
  } else {
    return std::nullopt;
  }
}

base::TimeDelta MessagingBackendStoreImpl::GetRecentMessageCutoffDuration() {
  return recent_message_cutoff_duration_;
}

void MessagingBackendStoreImpl::SetRecentMessageCutoffDuration(
    base::TimeDelta time_delta) {
  recent_message_cutoff_duration_ = time_delta;
}

std::vector<collaboration_pb::Message>
MessagingBackendStoreImpl::GetRecentMessagesForGroup(
    const data_sharing::GroupId& collaboration_id) {
  std::optional<MessagesPerGroup*> messages_per_group =
      GetMessagesPerGroup(collaboration_id);
  if (!messages_per_group) {
    return std::vector<collaboration_pb::Message>();
  }

  std::vector<collaboration_pb::Message> result;

  const time_t cutoff_time =
      (base::Time::Now() - recent_message_cutoff_duration_).ToTimeT();

  TraverseMessagesForGroup(
      messages_per_group.value(),
      base::BindRepeating(
          [](std::vector<collaboration_pb::Message>* result, time_t cutoff_time,
             collaboration_pb::Message& message) {
            if (message.event_timestamp() >= cutoff_time) {
              result->push_back(message);
            }
            return true;
          },
          &result, cutoff_time));

  std::sort(result.begin(), result.end(),
            [](const collaboration_pb::Message& a,
               const collaboration_pb::Message& b) {
              return a.event_timestamp() > b.event_timestamp();
            });

  return result;
}

void MessagingBackendStoreImpl::AddMessage(
    const collaboration_pb::Message& message) {
  last_added_message_for_testing_ = message;

  data_sharing::GroupId collaboration_id =
      data_sharing::GroupId(message.collaboration_id());
  CHECK(!collaboration_id->empty());

  if (!base::Contains(messages_, collaboration_id)) {
    messages_.insert({collaboration_id, std::make_unique<MessagesPerGroup>()});
  }

  MessagesPerGroup* messages_per_group = messages_.at(collaboration_id).get();

  MessageCategory category = GetMessageCategory(message);
  std::optional<collaboration_pb::Message> message_to_update = std::nullopt;
  std::optional<std::string> message_id_to_delete = std::nullopt;
  if (category == MessageCategory::kTab) {
    CHECK(message.has_tab_data());
    // For tab messages, keep the latest per tab.
    base::Uuid sync_tab_id =
        base::Uuid::ParseLowercase(message.tab_data().sync_tab_id());

    auto it = messages_per_group->tab_messages.find(sync_tab_id);
    if (it == messages_per_group->tab_messages.end()) {
      message_to_update = message;
      messages_per_group->tab_messages[sync_tab_id] = message;
    } else {
      if (IsMessageMoreRecent(message, it->second)) {
        message_id_to_delete = it->second.uuid();
        message_to_update = message;
        messages_per_group->tab_messages[sync_tab_id] = message;
      }
    }
  } else if (category == MessageCategory::kTabGroup) {
    // For tab group messages, keep the latest per event type.
    collaboration_pb::EventType event_type = message.event_type();
    auto it = messages_per_group->tab_group_messages.find(event_type);
    if (it == messages_per_group->tab_group_messages.end()) {
      message_to_update = message;
      messages_per_group->tab_group_messages[event_type] = message;
    } else {
      if (IsMessageMoreRecent(message, it->second)) {
        message_id_to_delete = it->second.uuid();
        message_to_update = message;
        messages_per_group->tab_group_messages[event_type] = message;
      }
    }
  } else if (category == MessageCategory::kCollaboration) {
    // For collaboration messages, keep the latest per user.
    std::tie(message_id_to_delete, message_to_update) =
        AddOrReplaceCollaborationMessage(
            messages_per_group->collaboration_messages, message);
  }

  if (message_id_to_delete) {
    database_->Delete({*message_id_to_delete});
  }
  if (message_to_update) {
    database_->Update(*message_to_update);
  }
}

void MessagingBackendStoreImpl::RemoveMessages(
    const std::set<std::string>& message_ids) {
  for (auto& [key, messages_per_group] : messages_) {
    auto& tab_messages = messages_per_group->tab_messages;
    for (auto it = tab_messages.begin(); it != tab_messages.end();) {
      std::string message_uuid = it->second.uuid();
      if (base::Contains(message_ids, message_uuid)) {
        it = tab_messages.erase(it);
      } else {
        ++it;
      }
    }

    auto& tab_group_messages = messages_per_group->tab_group_messages;
    for (auto it = tab_group_messages.begin();
         it != tab_group_messages.end();) {
      std::string message_uuid = it->second.uuid();
      if (base::Contains(message_ids, message_uuid)) {
        it = tab_group_messages.erase(it);
      } else {
        ++it;
      }
    }

    auto& collab_messages = messages_per_group->collaboration_messages;
    for (auto it = collab_messages.begin(); it != collab_messages.end();) {
      std::string message_uuid = it->uuid();
      if (base::Contains(message_ids, message_uuid)) {
        it = collab_messages.erase(it);
      } else {
        ++it;
      }
    }
  }

  database_->Delete(
      std::vector<std::string>(message_ids.begin(), message_ids.end()));
}

void MessagingBackendStoreImpl::RemoveAllMessages() {
  messages_.clear();
  database_->DeleteAllData();
}

std::optional<MessagesPerGroup*>
MessagingBackendStoreImpl::GetMessagesPerGroupForTesting(
    const data_sharing::GroupId& collaboration_id) {
  return GetMessagesPerGroup(collaboration_id);
}

std::optional<collaboration_pb::Message>
MessagingBackendStoreImpl::GetLastMessageForTesting() {
  return last_added_message_for_testing_;
}

std::optional<MessagesPerGroup*> MessagingBackendStoreImpl::GetMessagesPerGroup(
    const data_sharing::GroupId& collaboration_id) {
  auto it = messages_.find(collaboration_id);
  if (it == messages_.end()) {
    return std::nullopt;
  }
  return it->second.get();
}

void MessagingBackendStoreImpl::TraverseMessages(
    base::RepeatingCallback<bool(collaboration_pb::Message& message)>
        message_callback) {
  for (auto& [key, messages_per_group] : messages_) {
    for (auto& [tab_key, tab_message] : messages_per_group->tab_messages) {
      if (!message_callback.Run(tab_message)) {
        return;
      }
    }

    for (auto& [event_type, tab_group_message] :
         messages_per_group->tab_group_messages) {
      if (!message_callback.Run(tab_group_message)) {
        return;
      }
    }

    for (auto& collaboration_message :
         messages_per_group->collaboration_messages) {
      if (!message_callback.Run(collaboration_message)) {
        return;
      }
    }
  }
}

void MessagingBackendStoreImpl::TraverseMessagesForGroup(
    MessagesPerGroup* messages_per_group,
    base::RepeatingCallback<bool(collaboration_pb::Message& message)>
        message_callback) {
  for (auto& [tab_key, tab_message] : messages_per_group->tab_messages) {
    if (!message_callback.Run(tab_message)) {
      return;
    }
  }

  for (auto& [event_type, tab_group_message] :
       messages_per_group->tab_group_messages) {
    if (!message_callback.Run(tab_group_message)) {
      return;
    }
  }

  for (auto& collaboration_message :
       messages_per_group->collaboration_messages) {
    if (!message_callback.Run(collaboration_message)) {
      return;
    }
  }
}

void MessagingBackendStoreImpl::OnDatabaseLoaded(
    OnLoadCallback on_load_callback,
    bool success,
    const std::map<std::string, collaboration_pb::Message>& data) {
  if (success) {
    for (const auto& [key, message] : data) {
      AddMessage(message);
    }

    DeleteExpiredMessages();
    delete_expired_messages_timer_ = std::make_unique<base::RepeatingTimer>();
    delete_expired_messages_timer_->Start(
        FROM_HERE, kMessageCleanUpDuration,
        base::BindRepeating(&MessagingBackendStoreImpl::DeleteExpiredMessages,
                            weak_ptr_factory_.GetWeakPtr()));
  }

  std::move(on_load_callback).Run(success);
}

void MessagingBackendStoreImpl::DeleteExpiredMessages() {
  std::vector<std::string> ids_to_remove;

  // Loop through every message in the store, remove them if they are expired.
  base::Time now = base::Time::Now();
  for (auto& [key, messages_per_group] : messages_) {
    auto& tab_messages = messages_per_group->tab_messages;
    for (auto it = tab_messages.begin(); it != tab_messages.end();) {
      if (IsMessageExpired(it->second, now)) {
        ids_to_remove.push_back(it->second.uuid());
        it = tab_messages.erase(it);
      } else {
        ++it;
      }
    }

    auto& tab_group_messages = messages_per_group->tab_group_messages;
    for (auto it = tab_group_messages.begin();
         it != tab_group_messages.end();) {
      if (IsMessageExpired(it->second, now)) {
        ids_to_remove.push_back(it->second.uuid());
        it = tab_group_messages.erase(it);
      } else {
        ++it;
      }
    }

    auto& collab_messages = messages_per_group->collaboration_messages;
    for (auto it = collab_messages.begin(); it != collab_messages.end();) {
      if (IsMessageExpired(*it, now)) {
        ids_to_remove.push_back(it->uuid());
        it = collab_messages.erase(it);
      } else {
        ++it;
      }
    }

    if (!ids_to_remove.empty()) {
      database_->Delete(ids_to_remove);
    }
  }
}

}  // namespace collaboration::messaging
