// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/saved_tab_groups/collaboration_messaging_tab_data.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/data_sharing/data_sharing_service_factory.h"
#include "chrome/browser/image_fetcher/image_fetcher_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "components/collaboration/public/messaging/message.h"
#include "components/data_sharing/public/data_sharing_service.h"
#include "components/image_fetcher/core/image_fetcher_service.h"
#include "components/signin/public/base/avatar_icon_util.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "ui/compositor/compositor.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/widget/widget.h"

using collaboration::messaging::PersistentMessage;

namespace tab_groups {
namespace {
struct CollaborationFallbackAvatarParameters {
  float scale_factor;
  const raw_ptr<const ui::ColorProvider> color_provider;
};

const CollaborationFallbackAvatarParameters
GetCollaborationAvatarFallbackParametersFromWidget(
    const views::Widget* widget) {
  CHECK(widget);

  // Get devices scale factor for scaling the bitmaps.
  const float scale_factor =
      widget->GetCompositor() ? widget->GetCompositor()->device_scale_factor()
                              : 1.0f;

  return CollaborationFallbackAvatarParameters(scale_factor,
                                               widget->GetColorProvider());
}
}  // namespace

DEFINE_USER_DATA(CollaborationMessagingTabData);

CollaborationMessagingTabData::CollaborationMessagingTabData(
    tabs::TabInterface* tab)
    : profile_(tab->GetBrowserWindowInterface()->GetProfile()),
      scoped_unowned_user_data_(tab->GetUnownedUserDataHost(), *this) {}
CollaborationMessagingTabData::~CollaborationMessagingTabData() = default;

CollaborationMessagingTabData* CollaborationMessagingTabData::From(
    tabs::TabInterface* tab) {
  return Get(tab->GetUnownedUserDataHost());
}

void CollaborationMessagingTabData::SetMessage(PersistentMessage message) {
  using collaboration::messaging::CollaborationEvent;
  using collaboration::messaging::PersistentNotificationType;

  // Only Chip messages are allowed.
  CHECK(message.type == PersistentNotificationType::CHIP);

  // Chip messages are always TAB_ADDED or TAB_UPDATED.
  CHECK(message.collaboration_event == CollaborationEvent::TAB_ADDED ||
        message.collaboration_event == CollaborationEvent::TAB_UPDATED);

  // Chip messages must contain a triggering user.
  if (!message.attribution.triggering_user.has_value()) {
    return;
  }

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
  avatar_ = gfx::Image();
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
  // Safe to unwrap member because it was previously verified.
  GURL avatar_url = message.attribution.triggering_user->avatar_url;
  if (!avatar_url.is_valid()) {
    // Commit message immediately without an image.
    return CommitMessage(message, gfx::Image());
  }

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

  // Request the avatar image using the standard size. This will be
  // resized to accommodate the UI surfaces that use it.
  data_sharing_service->GetAvatarImageForURL(
      avatar_url, signin::kAccountInfoImageSize,
      base::BindOnce(&CollaborationMessagingTabData::CommitMessage,
                     GetWeakPtr(), message),
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
  avatar_ = avatar;

  // Message has been committed.
  message_to_commit_ = std::nullopt;
  NotifyMessageChanged();
}

ui::ImageModel CollaborationMessagingTabData::GetPageActionImage(
    const views::Widget* widget) const {
  auto fallback_params =
      GetCollaborationAvatarFallbackParametersFromWidget(widget);

  return GetPageActionImage(fallback_params.scale_factor,
                            fallback_params.color_provider);
}

ui::ImageModel CollaborationMessagingTabData::GetPageActionImage(
    float scale_factor,
    const ui::ColorProvider* color_provider) const {
  const int icon_width = GetLayoutConstant(LOCATION_BAR_TRAILING_ICON_SIZE);
  return GetImage(scale_factor, color_provider, icon_width,
                  /*add_border=*/true);
}

ui::ImageModel CollaborationMessagingTabData::GetHoverCardImage(
    const views::Widget* widget) const {
  auto fallback_params =
      GetCollaborationAvatarFallbackParametersFromWidget(widget);
  const int icon_width = GetLayoutConstant(TAB_ALERT_INDICATOR_ICON_WIDTH);
  return GetImage(fallback_params.scale_factor, fallback_params.color_provider,
                  icon_width,
                  /*add_border=*/false);
}

ui::ImageModel CollaborationMessagingTabData::GetImage(
    float scale_factor,
    const ui::ColorProvider* color_provider,
    int icon_width,
    bool add_border) const {
  if (!HasMessage()) {
    return ui::ImageModel();
  }
  if (!avatar_.IsEmpty()) {
    return ui::ImageModel::FromImage(
        gfx::ResizedImage(avatar_, gfx::Size(icon_width, icon_width)));
  }
  return CreateSizedFallback(scale_factor, color_provider, icon_width,
                             add_border);
}

ui::ImageModel CollaborationMessagingTabData::CreateSizedFallback(
    float scale_factor,
    const ui::ColorProvider* color_provider,
    int icon_width,
    bool add_border) const {
  const int icon_padding = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_COLLABORATION_MESSAGING_AVATAR_FALLBACK_ICON_PADDING);

  // Icon bounds represents the entire available area to draw the image.
  const gfx::Rect icon_bounds = gfx::Rect(icon_width, icon_width);

  gfx::Canvas canvas(icon_bounds.size(), scale_factor, /*is_opaque=*/false);
  canvas.SaveLayerAlpha(0xff);

  int border_width = 0;
  cc::PaintFlags background_flags;
  if (add_border) {
    border_width = ChromeLayoutProvider::Get()->GetDistanceMetric(
        DISTANCE_COLLABORATION_MESSAGING_AVATAR_FALLBACK_ICON_BORDER_SIZE);

    // The border will contain a color, but the background will be
    // transparent.
    background_flags.setAntiAlias(true);
    background_flags.setBlendMode(SkBlendMode::kClear);

    // Paint circle border, taking up the entire |icon_width|.
    cc::PaintFlags border_flags;
    border_flags.setColor(color_provider->GetColor(ui::kColorSysTonalOutline));
    canvas.DrawCircle(icon_bounds.CenterPoint(), icon_width / 2.0,
                      border_flags);
  } else {
    // The background will have a color.
    background_flags.setColor(
        color_provider->GetColor(ui::kColorSysTonalContainer));
  }

  // Paint circle background. This will be be the width of the icon
  // container minus the border width from both sides, if any.
  const int background_radius = (icon_width / 2.0) - border_width;
  canvas.DrawCircle(icon_bounds.CenterPoint(), background_radius,
                    background_flags);
  canvas.Restore();

  // Paint fallback icon. This will be the width of the icon container
  // minus the padding from both sides.
  canvas.Translate({icon_padding, icon_padding});
  gfx::PaintVectorIcon(&canvas, kPersonFilledPaddedSmallIcon,
                       icon_width - (icon_padding * 2),
                       color_provider->GetColor(ui::kColorSysOnTonalContainer));

  return ui::ImageModel::FromImageSkia(
      gfx::ImageSkia::CreateFromBitmap(canvas.GetBitmap(), scale_factor));
}

}  // namespace tab_groups
