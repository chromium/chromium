// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/media_router/cast_notification_controller_lacros.h"

#include <algorithm>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_cast_feature.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "components/media_router/browser/media_router.h"
#include "components/media_router/browser/media_router_factory.h"
#include "components/media_router/browser/mirroring_media_controller_host.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/browser_context.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"

namespace media_router {

namespace {

constexpr char kNotificationId[] = "browser.cast.session";
constexpr char kNotifierId[] = "browser.cast";

std::u16string GetNotificationTitle(const std::string& sink_name) {
  if (sink_name.empty()) {
    return l10n_util::GetStringUTF16(
        IDS_MEDIA_ROUTER_NOTIFICATION_TITLE_UNKNOWN);
  }
  return l10n_util::GetStringFUTF16(IDS_MEDIA_ROUTER_NOTIFICATION_TITLE,
                                    base::UTF8ToUTF16(sink_name));
}

std::u16string GetNotificationMessage(const MediaRoute& route,
                                      MirroringMediaControllerHost* freeze_host,
                                      bool freeze_enabled) {
  if (!freeze_enabled || !freeze_host || !freeze_host->CanFreeze()) {
    if (route.media_source().IsDesktopMirroringSource()) {
      return l10n_util::GetStringUTF16(
          IDS_MEDIA_ROUTER_NOTIFICATION_MESSAGE_CAST_SCREEN);
    }
    return base::UTF8ToUTF16(route.description());
  }
  if (freeze_host->IsFrozen()) {
    if (route.media_source().IsDesktopMirroringSource()) {
      return l10n_util::GetStringUTF16(
          IDS_MEDIA_ROUTER_NOTIFICATION_MESSAGE_SCREEN_PAUSED);
    }
    return l10n_util::GetStringUTF16(
        IDS_MEDIA_ROUTER_NOTIFICATION_MESSAGE_PAUSED);
  }
  if (route.media_source().IsDesktopMirroringSource()) {
    return l10n_util::GetStringUTF16(
        IDS_MEDIA_ROUTER_NOTIFICATION_MESSAGE_SCREEN_CAN_PAUSE);
  }
  if (route.media_source().IsTabMirroringSource()) {
    return l10n_util::GetStringUTF16(
        IDS_MEDIA_ROUTER_NOTIFICATION_MESSAGE_TAB_CAN_PAUSE);
  }
  return base::UTF8ToUTF16(route.description());
}

}  // namespace

CastNotificationControllerLacros::CastNotificationControllerLacros(
    Profile* profile)
    : CastNotificationControllerLacros(
          profile,
          NotificationDisplayService::GetForProfile(profile),
          MediaRouterFactory::GetApiForBrowserContext(profile)) {}

CastNotificationControllerLacros::CastNotificationControllerLacros(
    Profile* profile,
    NotificationDisplayService* notification_service,
    MediaRouter* router)
    : MediaRoutesObserver(router),
      profile_(profile),
      notification_service_(notification_service),
      media_router_(router) {}

CastNotificationControllerLacros::~CastNotificationControllerLacros() {
  StopObservingFreezeHost();
}

void CastNotificationControllerLacros::OnRoutesUpdated(
    const std::vector<MediaRoute>& routes) {
  freeze_button_index_.reset();
  stop_button_index_.reset();
  displayed_route_is_frozen_ = false;
  StopObservingFreezeHost();

  auto route_it =
      std::find_if(routes.begin(), routes.end(),
                   [](const MediaRoute& route) { return route.is_local(); });
  if (route_it == routes.end()) {
    // There was no active local route, so we hide the current outstanding
    // notification, if it exists.
    HideNotification();
    return;
  }
  // This will overwrite the existing notification if there is one.
  ShowNotification(*route_it);
}

void CastNotificationControllerLacros::OnFreezeInfoChanged() {
  if (displayed_route_) {
    ShowNotification(*displayed_route_);
  }
}

void CastNotificationControllerLacros::ShowNotification(
    const MediaRoute& route) {
  displayed_route_ = route;
  notification_service_->Display(NotificationHandler::Type::TRANSIENT,
                                 CreateNotification(route), nullptr);
}

void CastNotificationControllerLacros::HideNotification() {
  displayed_route_.reset();
  notification_service_->Close(NotificationHandler::Type::TRANSIENT,
                               kNotificationId);
}

message_center::Notification
CastNotificationControllerLacros::CreateNotification(const MediaRoute& route) {
  MirroringMediaControllerHost* freeze_host =
      media_router_->GetMirroringMediaControllerHost(route.media_route_id());
  if (freeze_host && freeze_host != freeze_host_) {
    freeze_host->AddObserver(this);
    freeze_host_ = freeze_host;
  }
  message_center::RichNotificationData data;
  data.buttons = GetButtons(route, freeze_host);
  data.pinned = true;
  // `vector_small_image` is ignored by the crosapi so we must convert it to
  // `small_image`. Also, kAppIconImageSize=16 is used in AshNotificationView,
  // but 16 here somehow results in a blurry image.
  data.small_image = gfx::Image(gfx::CreateVectorIcon(
      gfx::IconDescription(vector_icons::kMediaRouterIdleIcon, 32)));

  return message_center::Notification{
      message_center::NotificationType::NOTIFICATION_TYPE_SIMPLE,
      kNotificationId,
      GetNotificationTitle(route.media_sink_name()),
      GetNotificationMessage(route, freeze_host,
                             IsAccessCodeCastFreezeUiEnabled(profile_)),
      /*icon=*/ui::ImageModel{},
      /*display_source=*/u"",
      /*origin_url=*/GURL{},
      message_center::NotifierId{message_center::NotifierType::SYSTEM_COMPONENT,
                                 kNotifierId},
      std::move(data),
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::BindRepeating(
              &CastNotificationControllerLacros::OnNotificationClicked,
              weak_ptr_factory_.GetWeakPtr())),
  };
}

std::vector<message_center::ButtonInfo>
CastNotificationControllerLacros::GetButtons(
    const MediaRoute& route,
    MirroringMediaControllerHost* freeze_host) {
  std::vector<message_center::ButtonInfo> buttons;
  if (IsAccessCodeCastFreezeUiEnabled(profile_) && freeze_host &&
      freeze_host->CanFreeze()) {
    displayed_route_is_frozen_ = freeze_host->IsFrozen();
    buttons.emplace_back(
        displayed_route_is_frozen_
            ? l10n_util::GetStringUTF16(IDS_MEDIA_ROUTER_SINK_VIEW_RESUME)
            : l10n_util::GetStringUTF16(IDS_MEDIA_ROUTER_SINK_VIEW_PAUSE));
    freeze_button_index_ = buttons.size() - 1;
  }
  buttons.emplace_back(
      l10n_util::GetStringUTF16(IDS_MEDIA_ROUTER_SINK_VIEW_STOP));
  stop_button_index_ = buttons.size() - 1;
  return buttons;
}

void CastNotificationControllerLacros::OnNotificationClicked(
    absl::optional<int> button_index) {
  if (freeze_button_index_ && button_index == freeze_button_index_) {
    FreezeOrUnfreezeCastStream();
  } else if (button_index == stop_button_index_) {
    StopCasting();
  }
}

void CastNotificationControllerLacros::StopCasting() {
  if (displayed_route_) {
    media_router_->TerminateRoute(displayed_route_->media_route_id());
  }
}

void CastNotificationControllerLacros::FreezeOrUnfreezeCastStream() {
  if (!displayed_route_) {
    return;
  }
  MirroringMediaControllerHost* freeze_host =
      media_router_->GetMirroringMediaControllerHost(
          displayed_route_->media_route_id());
  if (!freeze_host) {
    return;
  }
  if (displayed_route_is_frozen_) {
    freeze_host->Unfreeze();
  } else {
    freeze_host->Freeze();
  }
}

void CastNotificationControllerLacros::StopObservingFreezeHost() {
  if (!displayed_route_) {
    return;
  }
  // We don't reuse `freeze_host_` here, in case it's been freed.
  MirroringMediaControllerHost* freeze_host =
      media_router_->GetMirroringMediaControllerHost(
          displayed_route_->media_route_id());
  if (freeze_host) {
    freeze_host->RemoveObserver(this);
    freeze_host_ = nullptr;
  }
}

}  // namespace media_router
