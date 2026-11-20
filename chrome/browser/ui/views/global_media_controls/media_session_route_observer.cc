// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/global_media_controls/media_session_route_observer.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/ui/views/global_media_controls/media_item_ui_helper.h"
#include "components/media_message_center/media_notification_item.h"
#include "components/media_router/browser/media_router.h"
#include "components/media_router/browser/media_router_factory.h"
#include "components/media_router/common/media_route.h"

MediaSessionRouteObserver::ShownItem::ShownItem() = default;
MediaSessionRouteObserver::ShownItem::ShownItem(const ShownItem&) = default;
MediaSessionRouteObserver::ShownItem&
MediaSessionRouteObserver::ShownItem::operator=(const ShownItem&) = default;
MediaSessionRouteObserver::ShownItem::~ShownItem() = default;

// static
std::unique_ptr<MediaSessionRouteObserver> MediaSessionRouteObserver::Create(
    content::BrowserContext* context,
    RefreshCallback refresh_callback) {
  if (!context || !media_router::MediaRouterEnabled(context)) {
    return nullptr;
  }
  media_router::MediaRouter* router =
      media_router::MediaRouterFactory::GetApiForBrowserContext(context);
  if (!router) {
    return nullptr;
  }
  return base::WrapUnique(new MediaSessionRouteObserver(
      router, context, std::move(refresh_callback)));
}

MediaSessionRouteObserver::MediaSessionRouteObserver(
    media_router::MediaRouter* router,
    content::BrowserContext* context,
    RefreshCallback refresh_callback)
    : media_router::MediaRoutesObserver(router),
      context_(context),
      refresh_callback_(std::move(refresh_callback)) {}

MediaSessionRouteObserver::~MediaSessionRouteObserver() = default;

void MediaSessionRouteObserver::OnItemShown(
    const std::string& id,
    base::WeakPtr<media_message_center::MediaNotificationItem> item) {
  if (!item || item->GetSourceType() !=
                   media_message_center::SourceType::kLocalMediaSession) {
    shown_items_.erase(id);
    return;
  }

  ShownItem& shown_item = shown_items_[id];
  shown_item.item = item;
  shown_item.route_id = GetRouteId(id, item);
}

void MediaSessionRouteObserver::OnItemHidden(const std::string& id) {
  shown_items_.erase(id);
}

void MediaSessionRouteObserver::OnRoutesUpdated(
    const std::vector<media_router::MediaRoute>& routes) {
  // Collect the items first, since refreshing an item calls back into
  // OnItemShown() or OnItemHidden().
  std::vector<std::pair<
      std::string, base::WeakPtr<media_message_center::MediaNotificationItem>>>
      items_to_refresh;
  for (const auto& [id, shown_item] : shown_items_) {
    if (shown_item.item &&
        GetRouteId(id, shown_item.item) != shown_item.route_id) {
      items_to_refresh.emplace_back(id, shown_item.item);
    }
  }

  base::WeakPtr<MediaSessionRouteObserver> weak_this =
      weak_factory_.GetWeakPtr();
  for (const auto& [id, item] : items_to_refresh) {
    // An earlier refresh may destroy `this` or hide the item.
    if (!weak_this) {
      return;
    }
    if (shown_items_.contains(id)) {
      refresh_callback_.Run(id, item);
    }
  }
}

std::optional<std::string> MediaSessionRouteObserver::GetRouteId(
    const std::string& id,
    base::WeakPtr<media_message_center::MediaNotificationItem> item) const {
  std::optional<media_router::MediaRoute> route =
      GetSessionRoute(id, item, context_);
  if (!route) {
    return std::nullopt;
  }
  return route->media_route_id();
}
