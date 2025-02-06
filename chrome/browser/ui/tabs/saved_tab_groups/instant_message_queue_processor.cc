// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/saved_tab_groups/instant_message_queue_processor.h"

#include "base/check_is_test.h"
#include "base/containers/queue.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/collaboration/messaging/messaging_backend_service_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/collaboration_messaging_tab_data.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"

namespace tab_groups {
namespace {

// Returns the local tab group ID from the InstantMessage.
std::optional<LocalTabGroupID> UnwrapTabGroupID(InstantMessage message) {
  auto tab_group_metadata = message.attribution.tab_group_metadata;
  if (tab_group_metadata.has_value()) {
    return tab_group_metadata->local_tab_group_id;
  }
  return std::nullopt;
}

}  // namespace

QueuedInstantMessage::QueuedInstantMessage(InstantMessage message_,
                                           SuccessCallback success_callback_)
    : message(message_), success_callback(std::move(success_callback_)) {}
QueuedInstantMessage::QueuedInstantMessage(QueuedInstantMessage&& other) =
    default;
QueuedInstantMessage::~QueuedInstantMessage() = default;

InstantMessageQueueProcessor::InstantMessageQueueProcessor(Profile* profile)
    : profile_(profile) {}
InstantMessageQueueProcessor::~InstantMessageQueueProcessor() = default;

void InstantMessageQueueProcessor::Enqueue(InstantMessage message,
                                           SuccessCallback success_callback) {
  if (!base::FeatureList::IsEnabled(toast_features::kToastFramework)) {
    return;
  }

  if (message.level !=
      collaboration::messaging::InstantNotificationLevel::BROWSER) {
    // Only handle browser notifications.
    return;
  }

  instant_message_queue_.emplace(message, std::move(success_callback));
  MaybeShowInstantMessage();
}

const InstantMessage& InstantMessageQueueProcessor::GetCurrentMessage() {
  CHECK(!instant_message_queue_.empty());
  const QueuedInstantMessage& it = instant_message_queue_.front();
  return it.message;
}

bool InstantMessageQueueProcessor::IsMessageShowing() {
  return is_showing_instant_message_;
}

int InstantMessageQueueProcessor::GetQueueSize() {
  return instant_message_queue_.size();
}

void InstantMessageQueueProcessor::MaybeShowInstantMessage() {
  if (IsMessageShowing()) {
    // Exit early. Since a message is being shown, a callback will cause
    // this code path to be entered again when the message is no
    // longer showing.
    return;
  }

  if (instant_message_queue_.empty()) {
    // End the queue-processing loop.
    return;
  }

  // Peek at the next item in queue and attempt to find the appropriate
  // browser to show the message.
  QueuedInstantMessage& it = instant_message_queue_.front();
  const bool message_shown = MaybeShowToastInBrowser(
      GetBrowser(it.message), GetParamsForMessage(it.message));
  if (!message_shown) {
    // Inform the backend that the message could not be displayed.
    std::move(it.success_callback).Run(false);
    return ProceedToNextQueueMessage();
  }

  // Inform the backend that the message was displayed.
  std::move(it.success_callback).Run(true);
  is_showing_instant_message_ = true;

  // Schedule next step of queue-processing.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          &InstantMessageQueueProcessor::ProcessQueueAfterMessageShown,
          base::Unretained(this)),
      GetMessageInterval());
}

Browser* InstantMessageQueueProcessor::GetBrowser(
    const InstantMessage& message) {
  Browser* browser = nullptr;

  const bool is_tab_removed_message =
      message.collaboration_event == CollaborationEvent::TAB_REMOVED &&
      message.type == collaboration::messaging::InstantNotificationType::
                          CONFLICT_TAB_REMOVED;
  const bool is_member_added_message =
      message.collaboration_event ==
      CollaborationEvent::COLLABORATION_MEMBER_ADDED;
  const bool is_tab_group_removed_message =
      message.collaboration_event == CollaborationEvent::TAB_GROUP_REMOVED;

  if (is_tab_removed_message || is_member_added_message ||
      is_tab_group_removed_message) {
    if (std::optional<LocalTabGroupID> local_tab_group_id =
            UnwrapTabGroupID(message)) {
      browser = SavedTabGroupUtils::GetBrowserWithTabGroupId(
          local_tab_group_id.value());
    }

    // In the case of TAB_GROUP_REMOVED, the group may or may not be open.
    // Find a fallback browser for this profile.
    if (!browser) {
      browser = chrome::FindLastActiveWithProfile(profile_);
    }
  }

  return browser;
}

bool InstantMessageQueueProcessor::MaybeShowToastInBrowser(
    Browser* browser,
    std::optional<ToastParams> params) {
  if (!browser) {
    // Browser state does not support showing this message or this is
    // not a message that has a corresponding toast.
    return false;
  }

  if (!params.has_value()) {
    // Not a valid message to show.
    return false;
  }

  ToastController* toast_controller =
      browser->browser_window_features()->toast_controller();
  if (!toast_controller) {
    // Encountered an issue with the toast controller for this browser.
    return false;
  }

  return toast_controller->MaybeShowToast(std::move(params.value()));
}

std::optional<ToastParams> InstantMessageQueueProcessor::GetParamsForMessage(
    const InstantMessage& message) {
  using collaboration::messaging::TabGroupMessageMetadata;
  using collaboration::messaging::TabMessageMetadata;

  switch (message.collaboration_event) {
    case CollaborationEvent::TAB_REMOVED: {
      std::optional<data_sharing::GroupMember> user =
          message.attribution.triggering_user;
      std::optional<TabMessageMetadata> tab_metadata =
          message.attribution.tab_metadata;
      const bool has_title = tab_metadata.has_value() &&
                             tab_metadata->last_known_title.has_value();
      if (!user.has_value() || !has_title) {
        return std::nullopt;
      }

      ToastParams params(ToastId::kTabGroupSyncTabRemoved);
      params.body_string_replacement_params = {
          base::UTF8ToUTF16(user->given_name),
          base::UTF8ToUTF16(tab_metadata->last_known_title.value()),
      };
      return params;
    }
    case CollaborationEvent::COLLABORATION_MEMBER_ADDED: {
      std::optional<data_sharing::GroupMember> user =
          message.attribution.affected_user;
      std::optional<TabGroupMessageMetadata> tab_group_metadata =
          message.attribution.tab_group_metadata;
      const bool has_group_title =
          tab_group_metadata.has_value() &&
          tab_group_metadata->last_known_title.has_value();
      if (!user.has_value() || !has_group_title) {
        return std::nullopt;
      }

      ToastParams params(ToastId::kTabGroupSyncUserJoined);
      params.body_string_replacement_params = {
          base::UTF8ToUTF16(user->given_name),
          base::UTF8ToUTF16(tab_group_metadata->last_known_title.value()),
      };
      return params;
    }
    case CollaborationEvent::TAB_GROUP_REMOVED: {
      std::optional<TabGroupMessageMetadata> tab_group_metadata =
          message.attribution.tab_group_metadata;
      const bool has_group_title =
          tab_group_metadata.has_value() &&
          tab_group_metadata->last_known_title.has_value();
      if (!has_group_title) {
        return std::nullopt;
      }

      ToastParams params(ToastId::kTabGroupSyncRemovedFromGroup);
      params.body_string_replacement_params = {
          base::UTF8ToUTF16(tab_group_metadata->last_known_title.value()),
      };
      return params;
    }
    default:
      return std::nullopt;
  }
}

base::TimeDelta InstantMessageQueueProcessor::GetMessageInterval() {
  // Take the maximum time a toast can show and add a second to ensure
  // that we wait until a message has completely timed out before trying
  // to show the next message.
  // TODO(crbug.com/390814333): Determine the correct heuristic for
  // time-between-messages.
  return base::Seconds(1) +
         std::max(toast_features::kToastTimeout.Get(),
                  toast_features::kToastWithoutActionTimeout.Get());
}

void InstantMessageQueueProcessor::ProcessQueueAfterMessageShown() {
  // This function is only entered if a toast was successfully shown and is
  // solely responsible for resetting the |is_showing_instant_message_| bool.
  CHECK(IsMessageShowing());
  is_showing_instant_message_ = false;
  ProceedToNextQueueMessage();
}

void InstantMessageQueueProcessor::ProceedToNextQueueMessage() {
  CHECK(!IsMessageShowing());
  CHECK(!instant_message_queue_.empty());
  instant_message_queue_.pop();
  MaybeShowInstantMessage();
}

}  // namespace tab_groups
