// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/zoom/chrome_zoom_level_otr_delegate.h"

#include "base/functional/bind.h"
#include "components/zoom/zoom_event_manager.h"

ChromeZoomLevelOTRDelegate::ChromeZoomLevelOTRDelegate(
    base::WeakPtr<zoom::ZoomEventManager> zoom_event_manager)
    : zoom_event_manager_(zoom_event_manager), host_zoom_map_(nullptr) {}

ChromeZoomLevelOTRDelegate::~ChromeZoomLevelOTRDelegate() {
}

void ChromeZoomLevelOTRDelegate::InitHostZoomMap(
    content::HostZoomMap* host_zoom_map) {
  // This init function must be called only once.
  DCHECK(!host_zoom_map_);
  DCHECK(host_zoom_map);
  host_zoom_map_ = host_zoom_map;

  zoom_subscription_ = host_zoom_map_->AddZoomLevelChangedCallback(
      base::BindRepeating(&ChromeZoomLevelOTRDelegate::OnZoomLevelChanged,
                          base::Unretained(this)));
}

void ChromeZoomLevelOTRDelegate::OnZoomLevelChanged(
    const content::HostZoomMap::ZoomLevelChange& change) {
  // If there's a manager to aggregate ZoomLevelChanged events, pass this event
  // along. Since we already hold a subscription from our associated
  // HostZoomMap, we don't need to create a separate subscription for this.
  if (zoom_event_manager_)
    zoom_event_manager_->OnZoomLevelChanged(change);
}
