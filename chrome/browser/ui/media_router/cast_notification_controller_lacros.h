// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_MEDIA_ROUTER_CAST_NOTIFICATION_CONTROLLER_LACROS_H_
#define CHROME_BROWSER_UI_MEDIA_ROUTER_CAST_NOTIFICATION_CONTROLLER_LACROS_H_

#include "components/keyed_service/core/keyed_service.h"
#include "components/media_router/browser/media_routes_observer.h"
#include "components/media_router/browser/mirroring_media_controller_host.h"
#include "components/media_router/common/media_route.h"
#include "ui/message_center/public/cpp/notification.h"

class NotificationDisplayService;
class Profile;

namespace media_router {

class MediaRouter;

// Manages showing Chrome OS notifications when casting from Lacros and handling
// user input from the notifications, e.g. to stop casting.
//
// Notifications for Cast sessions started from Ash are managed by
// ash::CastNotificationController instead.
class CastNotificationControllerLacros
    : public KeyedService,
      public MediaRoutesObserver,
      public MirroringMediaControllerHost::Observer {
 public:
  explicit CastNotificationControllerLacros(Profile* profile);
  CastNotificationControllerLacros(
      Profile* profile,
      NotificationDisplayService* notification_service,
      MediaRouter* router);
  CastNotificationControllerLacros(const CastNotificationControllerLacros&) =
      delete;
  CastNotificationControllerLacros& operator=(
      const CastNotificationControllerLacros&) = delete;

  ~CastNotificationControllerLacros() override;

  // MediaRoutesObserver:
  void OnRoutesUpdated(const std::vector<MediaRoute>& routes) override;

  // MirroringMediaControllerHost::Observer:
  void OnFreezeInfoChanged() override;

 private:
  void ShowNotification(const MediaRoute& route);
  void HideNotification();

  message_center::Notification CreateNotification(const MediaRoute& route);
  std::vector<message_center::ButtonInfo> GetButtons(
      const MediaRoute& route,
      MirroringMediaControllerHost* freeze_host);

  void OnNotificationClicked(absl::optional<int> button_index);
  void StopCasting();
  void FreezeOrUnfreezeCastStream();
  void StopObservingFreezeHost();

  const raw_ptr<Profile> profile_;
  const raw_ptr<NotificationDisplayService> notification_service_;
  const raw_ptr<MediaRouter> media_router_;

  absl::optional<MediaRoute> displayed_route_;
  bool displayed_route_is_frozen_ = false;
  absl::optional<int> freeze_button_index_;
  absl::optional<int> stop_button_index_;
  raw_ptr<MirroringMediaControllerHost> freeze_host_ = nullptr;

  base::WeakPtrFactory<CastNotificationControllerLacros> weak_ptr_factory_{
      this};
};

}  // namespace media_router

#endif  // CHROME_BROWSER_UI_MEDIA_ROUTER_CAST_NOTIFICATION_CONTROLLER_LACROS_H_
