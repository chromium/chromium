// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_media_controls/cast_media_notification_producer.h"

#include <map>

#include "base/ranges/algorithm.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/profiles/profile.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/global_media_controls/public/media_item_manager.h"
#include "components/media_message_center/media_notification_util.h"
#include "components/media_router/browser/media_router.h"
#include "components/media_router/browser/media_router_factory.h"
#include "components/media_router/common/pref_names.h"
#include "components/media_router/common/providers/cast/cast_media_source.h"
#include "components/prefs/pref_service.h"
#include "media/base/media_switches.h"

namespace {

// Returns false if a notification item shouldn't be created for |route|. If a
// route should be hidden, it's impossible to create an item for this route
// until the next time |OnRoutesUpdated()| is called.
bool ShouldHideNotification(Profile* profile,
                            const media_router::MediaRoute& route) {
  // TODO(crbug.com/40176012): Display multizone group route.
  if (route.is_connecting()) {
    return true;
  }
  // If the user changes the pref to show all Cast sessions, they won't be shown
  // until `OnRoutesUpdated()` is called again.
  // TODO(crbug.com/41321719): Ash currently considers Lacros routes non-local
  // and hides them if the pref is set to false.
  if (!route.is_local() &&
      !profile->GetPrefs()->GetBoolean(
          media_router::prefs::
              kMediaRouterShowCastSessionsStartedByOtherDevices)) {
    return true;
  }
  std::unique_ptr<media_router::CastMediaSource> source =
      media_router::CastMediaSource::FromMediaSource(route.media_source());
  if (media_router::GlobalMediaControlsCastStartStopEnabled(profile)) {
    // Show local site-initiated Mirroring routes.
    if (source && route.is_local() &&
        media_router::IsSiteInitiatedMirroringSource(source->source_id())) {
      return false;
    }
    // Hide a route if it contains a Streaming App, i.e. Tab/Desktop Mirroring
    // and Remote Playback routes.
    if (source && source->ContainsStreamingApp()) {
      // Don't hide it in case of MirroringType::kOffscreenTab.
      // This happens when 1UA mode is being used. It uses a URL for MediaSource
      // and a streaming receiver app for CastMediaSource.
      return !route.media_source().url().SchemeIsHTTPOrHTTPS();
    }
  } else if (route.controller_type() !=
             media_router::RouteControllerType::kGeneric) {
    // Hide a route if it doesn't have a generic controller (play, pause etc.).
    return true;
  }

  // Skip the multizone member check if it's a DIAL route.
  if (!route.media_source().IsCastPresentationUrl()) {
    return false;
  }

  // If the session is multizone member, then it would appear as a duplicate of
  // the multizone group's session, so it should instead be hidden.
  return source && source->GetAppIds().size() == 1 &&
         base::Contains(media_router::kMultizoneMemberAppIds,
                        source->GetAppIds()[0]);
}

}  // namespace

CastMediaNotificationProducer::CastMediaNotificationProducer(
    Profile* profile,
    global_media_controls::MediaItemManager* item_manager)
    : CastMediaNotificationProducer(
          profile,
          media_router::MediaRouterFactory::GetApiForBrowserContext(profile),
          item_manager) {}

CastMediaNotificationProducer::CastMediaNotificationProducer(
    Profile* profile,
    media_router::MediaRouter* router,
    global_media_controls::MediaItemManager* item_manager)
    : media_router::MediaRoutesObserver(router),
      profile_(profile),
      router_(router),
      item_manager_(item_manager),
      item_ui_observer_set_(this) {}

CastMediaNotificationProducer::~CastMediaNotificationProducer() = default;

base::WeakPtr<media_message_center::MediaNotificationItem>
CastMediaNotificationProducer::GetMediaItem(const std::string& id) {
  const auto item_it = items_.find(id);
  if (item_it == items_.end())
    return nullptr;
  return item_it->second.GetWeakPtr();
}

std::set<std::string>
CastMediaNotificationProducer::GetActiveControllableItemIds() const {
  std::set<std::string> ids;
  for (const auto& item : items_) {
    if (!item.second.is_active()) {
      continue;
    }
    if (!profile_->GetPrefs()->GetBoolean(
            media_router::prefs::
                kMediaRouterShowCastSessionsStartedByOtherDevices) &&
        !item.second.route_is_local()) {
      continue;
    }
    ids.insert(item.first);
  }
  return ids;
}

bool CastMediaNotificationProducer::HasFrozenItems() {
  return false;
}

void CastMediaNotificationProducer::OnItemShown(
    const std::string& id,
    global_media_controls::MediaItemUI* item_ui) {
  if (item_ui)
    item_ui_observer_set_.Observe(id, item_ui);
}

void CastMediaNotificationProducer::OnDialogDisplayed() {
  media_message_center::RecordConcurrentCastNotificationCount(
      GetActiveItemCount());
}

bool CastMediaNotificationProducer::IsItemActivelyPlaying(
    const std::string& id) {
  // TODO: This is a stub, since we currently only care about
  // MediaSessionNotificationProducer, but we probably should care about the
  // other ones.
  return false;
}

void CastMediaNotificationProducer::OnMediaItemUIDismissed(
    const std::string& id) {
  auto item = GetMediaItem(id);
  if (item) {
    item->Dismiss();
  }
  if (!HasActiveItems()) {
    item_manager_->OnItemsChanged();
  }
}

void CastMediaNotificationProducer::OnRoutesUpdated(
    const std::vector<media_router::MediaRoute>& routes) {
  const bool had_items = HasActiveItems();

  std::erase_if(items_, [&routes](const auto& item) {
    return !base::Contains(routes, item.first,
                           &media_router::MediaRoute::media_route_id);
  });

  for (const auto& route : routes) {
    if (ShouldHideNotification(profile_, route))
      continue;

    auto item_it = base::ranges::find(items_, route.media_route_id(),
                                      &Items::value_type::first);
    if (item_it == items_.end()) {
      mojo::Remote<media_router::mojom::MediaController> controller_remote;
      mojo::PendingReceiver<media_router::mojom::MediaController>
          controller_receiver = controller_remote.BindNewPipeAndPassReceiver();
      auto it_pair = items_.emplace(
          std::piecewise_construct,
          std::forward_as_tuple(route.media_route_id()),
          std::forward_as_tuple(route, item_manager_,
                                std::make_unique<CastMediaSessionController>(
                                    std::move(controller_remote)),
                                profile_));
      router_->GetMediaController(
          route.media_route_id(), std::move(controller_receiver),
          it_pair.first->second.GetObserverPendingRemote());
      item_manager_->ShowItem(route.media_route_id());
    } else {
      item_it->second.OnRouteUpdated(route);
    }
  }
  if (HasActiveItems() != had_items) {
    item_manager_->OnItemsChanged();
  }
}

size_t CastMediaNotificationProducer::GetActiveItemCount() const {
  return GetActiveControllableItemIds().size();
}

bool CastMediaNotificationProducer::HasActiveItems() const {
  return !GetActiveControllableItemIds().empty();
}

bool CastMediaNotificationProducer::HasLocalMediaRoute() const {
  return base::ranges::any_of(items_,
                              &CastMediaNotificationItem::route_is_local,
                              &Items::value_type::second);
}
