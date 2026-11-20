// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_SESSION_ROUTE_OBSERVER_H_
#define CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_SESSION_ROUTE_OBSERVER_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/media_router/browser/media_routes_observer.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace media_message_center {
class MediaNotificationItem;
}  // namespace media_message_center

namespace media_router {
class MediaRoute;
}  // namespace media_router

// Watches Media Routes for a profile and reports shown local media session
// items whose associated Remote Playback or Tab Mirroring route started or
// ended, so that their stop casting footer and device selector can be rebuilt.
class MediaSessionRouteObserver : public media_router::MediaRoutesObserver {
 public:
  // Called for each shown item whose associated route changed.
  using RefreshCallback = base::RepeatingCallback<void(
      const std::string& id,
      base::WeakPtr<media_message_center::MediaNotificationItem> item)>;

  // Returns nullptr if the Media Router is not available for `context`.
  static std::unique_ptr<MediaSessionRouteObserver> Create(
      content::BrowserContext* context,
      RefreshCallback refresh_callback);

  MediaSessionRouteObserver(const MediaSessionRouteObserver&) = delete;
  MediaSessionRouteObserver& operator=(const MediaSessionRouteObserver&) =
      delete;
  ~MediaSessionRouteObserver() override;

  // Records the route currently associated with the item `id`. Call this
  // whenever the item's footer is built or rebuilt. Items that are not local
  // media sessions are ignored.
  void OnItemShown(
      const std::string& id,
      base::WeakPtr<media_message_center::MediaNotificationItem> item);

  // Stops tracking the item `id`.
  void OnItemHidden(const std::string& id);

  // media_router::MediaRoutesObserver:
  void OnRoutesUpdated(
      const std::vector<media_router::MediaRoute>& routes) override;

 private:
  struct ShownItem {
    ShownItem();
    ShownItem(const ShownItem&);
    ShownItem& operator=(const ShownItem&);
    ~ShownItem();

    base::WeakPtr<media_message_center::MediaNotificationItem> item;
    std::optional<std::string> route_id;
  };

  MediaSessionRouteObserver(media_router::MediaRouter* router,
                            content::BrowserContext* context,
                            RefreshCallback refresh_callback);

  std::optional<std::string> GetRouteId(
      const std::string& id,
      base::WeakPtr<media_message_center::MediaNotificationItem> item) const;

  const raw_ptr<content::BrowserContext> context_;
  const RefreshCallback refresh_callback_;

  // Shown local media session items, keyed by item ID.
  std::map<std::string, ShownItem> shown_items_;

  base::WeakPtrFactory<MediaSessionRouteObserver> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_SESSION_ROUTE_OBSERVER_H_
