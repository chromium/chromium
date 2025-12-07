// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/saved_tab_groups/instant_message_queue_processor.h"

#include "base/check_is_test.h"
#include "base/containers/queue.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/collaboration/messaging/messaging_backend_service_factory.h"
#include "chrome/browser/data_sharing/data_sharing_service_factory.h"
#include "chrome/browser/image_fetcher/image_fetcher_service_factory.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/collaboration_messaging_tab_data.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_features.h"
#include "chrome/browser/ui/toasts/toast_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "components/collaboration/public/messaging/message.h"
#include "components/data_sharing/public/data_sharing_service.h"
#include "components/image_fetcher/core/image_fetcher_service.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/signin/public/base/avatar_icon_util.h"
#include "components/tabs/public/tab_group.h"

namespace tab_groups {
namespace {

// Returns the local tab group ID from the InstantMessage.
std::optional<LocalTabGroupID> UnwrapTabGroupID(InstantMessage message) {
  auto tab_group_metadata = message.attributions[0].tab_group_metadata;
  if (tab_group_metadata.has_value()) {
    return tab_group_metadata->local_tab_group_id;
  }
  return std::nullopt;
}

}  // namespace

QueuedInstantMessage::QueuedInstantMessage(InstantMessage message_,
                                           gfx::Image avatar_,
                                           SuccessCallback success_callback_)
    : message(message_),
      avatar(avatar_),
      success_callback(std::move(success_callback_)) {}
QueuedInstantMessage::QueuedInstantMessage(QueuedInstantMessage&& other) =
    default;
QueuedInstantMessage::~QueuedInstantMessage() = default;

InstantMessageQueueProcessor::InstantMessageQueueProcessor(Profile* profile)
    : profile_(profile) {}
InstantMessageQueueProcessor::~InstantMessageQueueProcessor() = default;

void InstantMessageQueueProcessor::Enqueue(InstantMessage message,
                                           SuccessCallback success_callback) {
  if (message.level !=
      collaboration::messaging::InstantNotificationLevel::BROWSER) {
    // Only handle browser notifications.
    return;
  }

  if (message.localized_message.empty()) {
    return;
  }

  FetchAvatar(message,
              base::BindOnce(&InstantMessageQueueProcessor::OnAvatarFetched,
                             weak_factory_.GetWeakPtr(), message,
                             std::move(success_callback)));
}

void InstantMessageQueueProcessor::FetchAvatar(
    InstantMessage message,
    FetchAvatarSuccessCallback success_callback) {
  // Aggregated messages do not have a single attribution, therefore cannot
  // show an avatar.
  if (message.attributions.size() != 1) {
    return std::move(success_callback).Run(gfx::Image());
  }

  GURL avatar_url;
  switch (message.collaboration_event) {
    case CollaborationEvent::TAB_REMOVED: {
      MessageAttribution attribution = message.attributions.front();
      if (attribution.triggering_user.has_value()) {
        avatar_url = attribution.triggering_user->avatar_url;
      }
      break;
    }
    case CollaborationEvent::COLLABORATION_MEMBER_ADDED: {
      MessageAttribution attribution = message.attributions.front();
      if (attribution.affected_user.has_value()) {
        avatar_url = attribution.affected_user->avatar_url;
      }
      break;
    }
    default:
      break;
  }

  if (!avatar_url.is_valid()) {
    // Message has no avatar to show, immediately trigger callback.
    return std::move(success_callback).Run(gfx::Image());
  }

  image_fetcher::ImageFetcherService* image_fetcher_service =
      ImageFetcherServiceFactory::GetForKey(profile_->GetProfileKey());
  if (!image_fetcher_service) {
    return std::move(success_callback).Run(gfx::Image());
  }

  data_sharing::DataSharingService* const data_sharing_service =
      data_sharing::DataSharingServiceFactory::GetForProfile(profile_);
  if (!data_sharing_service) {
    return std::move(success_callback).Run(gfx::Image());
  }

  // Request the avatar image using the standard size. This will be
  // resized to accommodate the Toast surface.
  data_sharing_service->GetAvatarImageForURL(
      avatar_url, toasts::ToastView::GetIconSize(), std::move(success_callback),
      image_fetcher_service->GetImageFetcher(
          image_fetcher::ImageFetcherConfig::kDiskCacheOnly));
}

void InstantMessageQueueProcessor::OnAvatarFetched(
    InstantMessage message,
    SuccessCallback success_callback,
    const gfx::Image& avatar) {
  instant_message_queue_.emplace(message, avatar, std::move(success_callback));
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
  const bool message_shown =
      MaybeShowToastInBrowser(GetBrowser(it.message), GetParamsForMessage(it));
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
    const QueuedInstantMessage& queued_message) {
  using collaboration::messaging::TabGroupMessageMetadata;
  using collaboration::messaging::TabMessageMetadata;

  switch (queued_message.message.collaboration_event) {
    case CollaborationEvent::TAB_REMOVED: {
      ToastParams params(ToastId::kTabGroupSyncTabRemoved);
      params.body_string_override = queued_message.message.localized_message;
      if (!queued_message.avatar.IsEmpty()) {
        params.image_override =
            ui::ImageModel::FromImage(queued_message.avatar);
      }
      return params;
    }
    case CollaborationEvent::COLLABORATION_MEMBER_ADDED: {
      ToastParams params(ToastId::kTabGroupSyncUserJoined);
      params.body_string_override = queued_message.message.localized_message;
      if (!queued_message.avatar.IsEmpty()) {
        params.image_override =
            ui::ImageModel::FromImage(queued_message.avatar);
      }
      return params;
    }
    case CollaborationEvent::TAB_GROUP_REMOVED: {
      ToastParams params(ToastId::kTabGroupSyncRemovedFromGroup);
      params.body_string_override = queued_message.message.localized_message;
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
  return base::Seconds(1) + std::max(ToastController::kToastDefaultTimeout,
                                     ToastController::kToastWithActionTimeout);
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
