// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/collaboration/internal/messaging/instant_message_processor_impl.h"

#include <optional>
#include <set>
#include <unordered_map>
#include <vector>

#include "base/containers/contains.h"
#include "base/functional/callback_helpers.h"
#include "base/hash/hash.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "components/collaboration/public/messaging/message.h"
#include "components/collaboration/public/messaging/messaging_backend_service.h"
#include "components/data_sharing/public/group_data.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace collaboration::messaging {

struct MessageAggregationKey {
  InstantNotificationLevel level;
  CollaborationEvent event;
  std::optional<data_sharing::GroupId> collaboration_id;

  bool operator==(const MessageAggregationKey& other) const {
    return level == other.level && event == other.event &&
           collaboration_id == other.collaboration_id;
  }
};

struct MessageAggregationKeyHash {
  MessageAggregationKeyHash() = default;
  ~MessageAggregationKeyHash() = default;
  size_t operator()(const MessageAggregationKey& k) const {
    size_t hash = 0;
    return base::HashCombine(hash, k.level, k.event, k.collaboration_id);
  }
};

}  // namespace collaboration::messaging

namespace collaboration::messaging {
namespace {
constexpr base::TimeDelta kQueueInterval = base::Seconds(1);

bool ShouldAggregate(const InstantMessage& message) {
  switch (message.collaboration_event) {
    case CollaborationEvent::TAB_UPDATED:
    case CollaborationEvent::TAB_REMOVED:
      return false;
    case CollaborationEvent::TAB_GROUP_REMOVED:
    case CollaborationEvent::COLLABORATION_MEMBER_ADDED:
      return true;
    case CollaborationEvent::UNDEFINED:
    case CollaborationEvent::TAB_ADDED:
    case CollaborationEvent::TAB_GROUP_ADDED:
    case CollaborationEvent::TAB_GROUP_NAME_UPDATED:
    case CollaborationEvent::TAB_GROUP_COLOR_UPDATED:
    case CollaborationEvent::COLLABORATION_ADDED:
    case CollaborationEvent::COLLABORATION_REMOVED:
    case CollaborationEvent::COLLABORATION_MEMBER_REMOVED:
      CHECK(false) << "Unexpected event in instant message "
                   << static_cast<int>(message.collaboration_event);
  }
}

// All key constructions should happen via this method.
MessageAggregationKey CreateMessageAggregationKey(
    const InstantMessage& message) {
  CHECK(ShouldAggregate(message));

  MessageAggregationKey key;
  key.level = message.level;
  key.event = message.collaboration_event;
  if (message.collaboration_event ==
      CollaborationEvent::COLLABORATION_MEMBER_ADDED) {
    key.collaboration_id = message.attributions[0].collaboration_id;
  }

  return key;
}

}  // namespace

InstantMessageProcessorImpl::InstantMessageProcessorImpl() = default;

InstantMessageProcessorImpl::~InstantMessageProcessorImpl() = default;

void InstantMessageProcessorImpl::SetMessagingBackendService(
    MessagingBackendService* messaging_backend_service) {
  CHECK(!messaging_backend_service_);
  CHECK(messaging_backend_service);
  messaging_backend_service_ = messaging_backend_service;
}

void InstantMessageProcessorImpl::SetInstantMessageDelegate(
    InstantMessageDelegate* instant_message_delegate) {
  // We must be either setting a delegate where there was none before or
  // we should be resetting a non-null delegate.
  CHECK((instant_message_delegate_ == nullptr &&
         instant_message_delegate != nullptr) ||
        (instant_message_delegate_ != nullptr &&
         instant_message_delegate == nullptr));
  instant_message_delegate_ = instant_message_delegate;
}

bool InstantMessageProcessorImpl::IsEnabled() const {
  return !!instant_message_delegate_;
}

void InstantMessageProcessorImpl::DisplayInstantMessage(
    const InstantMessage& message) {
  message_queue_.push_back(message);
  ScheduleProcessing();
}

void InstantMessageProcessorImpl::HideInstantMessage(
    const std::set<base::Uuid>& message_ids) {
  // Remove the messages from the queue that have MessageAttribution IDs that
  // match.
  message_queue_.erase(
      std::remove_if(message_queue_.begin(), message_queue_.end(),
                     [&message_ids](const InstantMessage& message) {
                       CHECK(IsSingleMessage(message));
                       std::optional<base::Uuid> current_message_id =
                           message.attributions[0].id;

                       if (!current_message_id.has_value()) {
                         return false;
                       }
                       return base::Contains(message_ids, *current_message_id);
                     }),
      message_queue_.end());

  // In case the message was displayed previously, we still tell the UI about
  // every message ID, even if it might have been removed from the queue.
  instant_message_delegate_->HideInstantaneousMessage(message_ids);
}

void InstantMessageProcessorImpl::ScheduleProcessing() {
  if (processing_scheduled_) {
    return;
  }
  processing_scheduled_ = true;

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&InstantMessageProcessorImpl::ProcessQueue,
                     weak_ptr_factory_.GetWeakPtr()),
      kQueueInterval);
}

void InstantMessageProcessorImpl::ProcessQueue() {
  processing_scheduled_ = false;
  if (message_queue_.empty()) {
    // The queue might have been cleared by HideInstantMessage.
    return;
  }
  std::vector<InstantMessage> aggregated_messages =
      AggregateMessages(message_queue_);
  message_queue_.clear();

  for (const auto& message : aggregated_messages) {
    std::vector<base::Uuid> message_ids;
    for (const auto& attribution : message.attributions) {
      CHECK(attribution.id.has_value());
      message_ids.emplace_back(attribution.id.value());
    }
    instant_message_delegate_->DisplayInstantaneousMessage(
        message,
        base::BindOnce(&InstantMessageProcessorImpl::OnMessageDisplayed,
                       weak_ptr_factory_.GetWeakPtr(), message_ids));
  }
}

std::vector<InstantMessage> InstantMessageProcessorImpl::AggregateMessages(
    const std::vector<InstantMessage>& messages) {
  std::unordered_map<MessageAggregationKey, std::vector<const InstantMessage*>,
                     MessageAggregationKeyHash>
      aggregation_map;
  std::vector<InstantMessage> aggregated_messages;
  std::vector<InstantMessage> non_aggregated_messages;

  // Populate the aggregation map.
  for (const auto& message : messages) {
    CHECK(IsSingleMessage(message));
    if (!ShouldAggregate(message)) {
      non_aggregated_messages.emplace_back(message);
      continue;
    }

    MessageAggregationKey key = CreateMessageAggregationKey(message);
    aggregation_map[key].emplace_back(&message);
  }

  // Process the aggregation map.
  for (const auto& [key, message_ptrs] : aggregation_map) {
    if (message_ptrs.size() == 1) {
      non_aggregated_messages.push_back(*message_ptrs[0]);
    } else {
      // Aggregated message case.
      std::vector<InstantMessage> aggregated_msgs_temp;
      for (const auto* message_ptr : message_ptrs) {
        aggregated_msgs_temp.push_back(*message_ptr);
      }
      aggregated_messages.push_back(
          CreateAggregatedMessage(aggregated_msgs_temp));
    }
  }

  // Combine with non-aggregated messages as a single list and return.
  aggregated_messages.insert(aggregated_messages.end(),
                             non_aggregated_messages.begin(),
                             non_aggregated_messages.end());
  return aggregated_messages;
}

InstantMessage InstantMessageProcessorImpl::CreateAggregatedMessage(
    const std::vector<InstantMessage>& messages) {
  CHECK(messages.size() > 1u);
  InstantMessage aggregated_message;
  aggregated_message.collaboration_event = messages[0].collaboration_event;
  aggregated_message.level = messages[0].level;
  aggregated_message.type = messages[0].type;

  // Populate message attributions for the aggregated message.
  for (const auto& message : messages) {
    CHECK(IsSingleMessage(message));
    aggregated_message.attributions.emplace_back(message.attributions[0]);
  }

  // Create message string for aggregation based on message type.
  switch (messages[0].collaboration_event) {
    case CollaborationEvent::COLLABORATION_MEMBER_ADDED: {
      std::string group_name =
          messages[0]
              .attributions[0]
              .tab_group_metadata->last_known_title.value();
      aggregated_message.localized_message = l10n_util::GetStringFUTF16(
          IDS_DATA_SHARING_TOAST_NEW_MEMBER_MULTIPLE_MEMBERS,
          base::UTF8ToUTF16(base::ToString(messages.size())),
          base::UTF8ToUTF16(group_name));
      break;
    }
    case CollaborationEvent::TAB_GROUP_REMOVED: {
      aggregated_message.localized_message = l10n_util::GetStringFUTF16(
          IDS_DATA_SHARING_TOAST_BLOCK_LEAVE_MULTIPLE_GROUPS,
          base::UTF8ToUTF16(base::ToString(messages.size())));
      break;
    }
    default:
      CHECK(false);
  }

  return aggregated_message;
}

void InstantMessageProcessorImpl::OnMessageDisplayed(
    const std::vector<base::Uuid>& message_ids,
    bool success) {
  if (!success) {
    return;
  }

  for (const base::Uuid& message_id : message_ids) {
    messaging_backend_service_->ClearPersistentMessage(
        message_id, PersistentNotificationType::INSTANT_MESSAGE);
  }
}

}  // namespace collaboration::messaging
