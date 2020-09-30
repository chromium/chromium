// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_media_controls/cast_media_notification_provider.h"

#include "chrome/browser/profiles/profile.h"
#include "components/media_message_center/media_notification_controller.h"
#include "components/media_router/browser/media_router.h"
#include "components/media_router/browser/media_router_factory.h"
#include "components/media_router/common/providers/cast/cast_media_source.h"

namespace {

bool ShouldHideNotification(const media_router::MediaRoute& route) {
  if (!route.for_display() ||
      route.controller_type() != media_router::RouteControllerType::kGeneric) {
    return true;
  }
  if (!route.media_source().IsCastPresentationUrl()) {
    return false;
  }
  std::unique_ptr<media_router::CastMediaSource> source =
      media_router::CastMediaSource::FromMediaSource(route.media_source());
  // If the session is multizone member, then it would appear as a duplicate of
  // the multizone group's session, so it should instead be hidden.
  return source && source->GetAppIds().size() == 1 &&
         base::Contains(media_router::kMultizoneMemberAppIds,
                        source->GetAppIds()[0]);
}

}  // namespace

CastMediaNotificationProvider::CastMediaNotificationProvider(
    Profile* profile,
    media_message_center::MediaNotificationController* notification_controller,
    base::RepeatingClosure items_changed_callback)
    : CastMediaNotificationProvider(
          profile,
          media_router::MediaRouterFactory::GetApiForBrowserContext(profile),
          notification_controller,
          std::move(items_changed_callback)) {}

CastMediaNotificationProvider::CastMediaNotificationProvider(
    Profile* profile,
    media_router::MediaRouter* router,
    media_message_center::MediaNotificationController* notification_controller,
    base::RepeatingClosure items_changed_callback)
    : media_router::MediaRoutesObserver(router),
      profile_(profile),
      router_(router),
      notification_controller_(notification_controller),
      items_changed_callback_(std::move(items_changed_callback)) {}

CastMediaNotificationProvider::~CastMediaNotificationProvider() = default;

void CastMediaNotificationProvider::OnRoutesUpdated(
    const std::vector<media_router::MediaRoute>& routes,
    const std::vector<media_router::MediaRoute::Id>& joinable_route_ids) {
  const bool had_items = HasItems();

  base::EraseIf(items_, [&routes](const auto& item) {
    return std::find_if(routes.begin(), routes.end(),
                        [&item](const media_router::MediaRoute& route) {
                          return item.first == route.media_route_id();
                        }) == routes.end();
  });

  for (const auto& route : routes) {
    if (ShouldHideNotification(route))
      continue;
    auto item_it =
        std::find_if(items_.begin(), items_.end(), [&route](const auto& item) {
          return item.first == route.media_route_id();
        });
    if (item_it == items_.end()) {
      mojo::Remote<media_router::mojom::MediaController> controller_remote;
      mojo::PendingReceiver<media_router::mojom::MediaController>
          controller_receiver = controller_remote.BindNewPipeAndPassReceiver();
      auto it_pair = items_.emplace(
          std::piecewise_construct,
          std::forward_as_tuple(route.media_route_id()),
          std::forward_as_tuple(route, notification_controller_,
                                std::make_unique<CastMediaSessionController>(
                                    std::move(controller_remote)),
                                profile_));
      router_->GetMediaController(
          route.media_route_id(), std::move(controller_receiver),
          it_pair.first->second.GetObserverPendingRemote());
      notification_controller_->ShowNotification(route.media_route_id());
    } else {
      item_it->second.OnRouteUpdated(route);
    }
  }
  if (HasItems() != had_items)
    items_changed_callback_.Run();
}

base::WeakPtr<media_message_center::MediaNotificationItem>
CastMediaNotificationProvider::GetNotificationItem(const std::string& id) {
  const auto item_it = items_.find(id);
  if (item_it == items_.end())
    return nullptr;
  return item_it->second.GetWeakPtr();
}

bool CastMediaNotificationProvider::HasItems() const {
  return !items_.empty();
}

size_t CastMediaNotificationProvider::GetItemCount() const {
  return items_.size();
}
