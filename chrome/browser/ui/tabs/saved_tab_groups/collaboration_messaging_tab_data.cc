// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/saved_tab_groups/collaboration_messaging_tab_data.h"

#include "chrome/browser/data_sharing/data_sharing_service_factory.h"
#include "chrome/browser/image_fetcher/image_fetcher_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/ui/layout_constants.h"
#include "components/collaboration/public/messaging/message.h"
#include "components/data_sharing/public/data_sharing_service.h"
#include "components/image_fetcher/core/image_fetcher_service.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_util.h"

using collaboration::messaging::PersistentMessage;

namespace tab_groups {
namespace {

int GetHoverCardImageSize() {
  return GetLayoutConstant(TAB_ALERT_INDICATOR_ICON_WIDTH);
}

int GetPageActionImageSize() {
  return GetLayoutConstant(LOCATION_BAR_TRAILING_ICON_SIZE);
}

gfx::Image ResizeForHoverCard(const gfx::Image& image) {
  auto image_size = GetHoverCardImageSize();
  auto size = gfx::Size(image_size, image_size);
  return gfx::ResizedImage(image, size);
}

}  // namespace

CollaborationMessagingTabData::CollaborationMessagingTabData(Profile* profile)
    : profile_(profile) {}
CollaborationMessagingTabData::~CollaborationMessagingTabData() = default;

void CollaborationMessagingTabData::SetMessage(PersistentMessage message) {
  using collaboration::messaging::CollaborationEvent;
  using collaboration::messaging::PersistentNotificationType;

  // Only Chip messages are allowed.
  CHECK(message.type == PersistentNotificationType::CHIP);

  // Chip messages are always TAB_ADDED or TAB_UPDATED.
  CHECK(message.collaboration_event == CollaborationEvent::TAB_ADDED ||
        message.collaboration_event == CollaborationEvent::TAB_UPDATED);

  // Chip messages must contain a triggering user.
  CHECK(message.attribution.triggering_user.has_value());

  // Cache this message to prevent committing stale data when the
  // image request resolves.
  message_to_commit_ = message;

  if (mock_avatar_for_testing_.has_value()) {
    // Testing path only.
    // Commit the message outright without triggering a network request.
    CommitMessage(message, mock_avatar_for_testing_.value());
  } else {
    // Initiate the request to fetch the avatar image. Rather than setting
    // the message data directly, the data will be set once the image request
    // is resolved. This prevents UI flicker in the user's avatar.
    FetchAvatar(message);
  }
}

void CollaborationMessagingTabData::ClearMessage(PersistentMessage message) {
  // Reject committing data from any in-flight requests.
  message_to_commit_ = std::nullopt;

  // Clear out all data, i.e. set HasMessage() to return false.
  given_name_ = std::u16string();
  page_action_avatar_ = ui::ImageModel();
  hover_card_avatar_ = ui::ImageModel();
  collaboration_event_ = CollaborationEvent::UNDEFINED;

  NotifyMessageChanged();
}

base::CallbackListSubscription
CollaborationMessagingTabData::RegisterMessageChangedCallback(
    CallbackList::CallbackType cb) {
  return message_changed_callback_list_.Add(std::move(cb));
}

void CollaborationMessagingTabData::NotifyMessageChanged() {
  message_changed_callback_list_.Notify();
}

void CollaborationMessagingTabData::FetchAvatar(PersistentMessage message) {
  // Safe to unwrap member because it was previously CHECKed.
  auto avatar_url = message.attribution.triggering_user->avatar_url;

  image_fetcher::ImageFetcherService* image_fetcher_service =
      ImageFetcherServiceFactory::GetForKey(profile_->GetProfileKey());
  if (!image_fetcher_service) {
    // Commit message immediately without an image.
    return CommitMessage(message, gfx::Image());
  }

  data_sharing::DataSharingService* const data_sharing_service =
      data_sharing::DataSharingServiceFactory::GetForProfile(profile_);
  if (!data_sharing_service) {
    // Commit message immediately without an image.
    return CommitMessage(message, gfx::Image());
  }

  // Perform the request using disk caching. The page action icon size
  // is used to request the larger of the 2 images needed. The sizes are
  // similar enough that the larger image is resized to accommodate the
  // smaller hover card image.
  data_sharing_service->GetAvatarImageForURL(
      avatar_url, GetPageActionImageSize(),
      base::BindOnce(&CollaborationMessagingTabData::CommitMessage,
                     base::Unretained(this), message),
      image_fetcher_service->GetImageFetcher(
          image_fetcher::ImageFetcherConfig::kDiskCacheOnly));
}

void CollaborationMessagingTabData::CommitMessage(
    PersistentMessage requested_message,
    const gfx::Image& avatar) {
  if (!message_to_commit_.has_value()) {
    // If the message to commit has been cleared, do nothing.
    return;
  }

  // Only commit message data if the requested avatar has not changed
  // since last requested.
  if (message_to_commit_->attribution.triggering_user->avatar_url !=
      requested_message.attribution.triggering_user->avatar_url) {
    return;
  }

  // Since it is possible for the message to have changed while the avatar
  // stayed the same (i.e. the same user performed another action on
  // this tab), this will use the collaboration_event from the most recent
  // message_to_commit_ and subsequently reset it.
  given_name_ = base::UTF8ToUTF16(
      message_to_commit_->attribution.triggering_user->given_name);
  collaboration_event_ = message_to_commit_->collaboration_event;

  // In rare cases such as null services or no response data, there
  // may not be an avatar to display. Downstream UIs will be responsible
  // for deciding what to show instead.
  if (avatar.IsEmpty()) {
    page_action_avatar_ = ui::ImageModel();
    hover_card_avatar_ = ui::ImageModel();
  } else {
    page_action_avatar_ = ui::ImageModel::FromImage(avatar);
    hover_card_avatar_ = ui::ImageModel::FromImage(ResizeForHoverCard(avatar));
  }

  // Message has been committed.
  message_to_commit_ = std::nullopt;
  NotifyMessageChanged();
}

}  // namespace tab_groups
