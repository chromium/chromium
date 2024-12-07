// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/collaboration/internal/messaging/storage/messaging_backend_store_impl.h"

#include "base/containers/contains.h"
#include "base/task/single_thread_task_runner.h"
#include "components/collaboration/internal/messaging/storage/collaboration_message_util.h"

namespace collaboration::messaging {

namespace {
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
}  // namespace

MessagesPerGroup::MessagesPerGroup() = default;
MessagesPerGroup::~MessagesPerGroup() = default;

MessagingBackendStoreImpl::MessagingBackendStoreImpl() = default;
MessagingBackendStoreImpl::~MessagingBackendStoreImpl() = default;

void MessagingBackendStoreImpl::Initialize(
    base::OnceCallback<void(bool)> on_initialized_callback) {
  // TODO(crbug.com/379870772): Initialize database and load messages.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(on_initialized_callback), true));
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
  }
}

void MessagingBackendStoreImpl::ClearDirtyMessage(const base::Uuid uuid,
                                                  DirtyType dirty_type) {
  TraverseMessages(base::BindRepeating(
      [](const base::Uuid uuid, DirtyType dirty_type,
         collaboration_pb::Message& message) {
        if (message.uuid() == uuid.AsLowercaseString()) {
          message.set_dirty(ClearDirty(message, dirty_type));
          return false;
        }
        return true;
      },
      uuid, dirty_type));
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

  return result;
}

void MessagingBackendStoreImpl::AddMessage(
    const collaboration_pb::Message& message) {
  data_sharing::GroupId collaboration_id =
      data_sharing::GroupId(message.collaboration_id());
  CHECK(!collaboration_id->empty());

  if (!base::Contains(messages_, collaboration_id)) {
    messages_.insert({collaboration_id, std::make_unique<MessagesPerGroup>()});
  }

  MessagesPerGroup* messages_per_group = messages_.at(collaboration_id).get();

  MessageCategory category = GetMessageCategory(message);
  if (category == MessageCategory::kTab) {
    CHECK(message.has_tab_data());
    // For tab messages, keep the latest per tab.
    base::Uuid sync_tab_id =
        base::Uuid::ParseLowercase(message.tab_data().sync_tab_id());

    auto it = messages_per_group->tab_messages.find(sync_tab_id);
    if (it == messages_per_group->tab_messages.end()) {
      messages_per_group->tab_messages[sync_tab_id] = message;
    } else {
      if (IsMessageMoreRecent(message, it->second)) {
        messages_per_group->tab_messages[sync_tab_id] = message;
      }
    }
  } else if (category == MessageCategory::kTabGroup) {
    // For tab group messages, keep the latest per event type.
    collaboration_pb::EventType event_type = message.event_type();
    auto it = messages_per_group->tab_group_messages.find(event_type);
    if (it == messages_per_group->tab_group_messages.end()) {
      messages_per_group->tab_group_messages[event_type] = message;
    } else {
      if (IsMessageMoreRecent(message, it->second)) {
        messages_per_group->tab_group_messages[event_type] = message;
      }
    }
  } else if (category == MessageCategory::kCollaboration) {
    // For collaboration messages, keep all the messages.
    messages_per_group->collaboration_messages.push_back(message);
  }
}

std::optional<MessagesPerGroup*>
MessagingBackendStoreImpl::GetMessagesPerGroupForTesting(
    const data_sharing::GroupId& collaboration_id) {
  return GetMessagesPerGroup(collaboration_id);
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

}  // namespace collaboration::messaging
