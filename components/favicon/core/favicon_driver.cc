// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/favicon/core/favicon_driver.h"
#include "base/observer_list.h"

namespace favicon {

void FaviconDriver::AddObserver(FaviconDriverObserver* observer) {
  observer_list_.AddObserver(observer);
}

void FaviconDriver::RemoveObserver(FaviconDriverObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

FaviconDriver::FaviconDriver() {
}

FaviconDriver::~FaviconDriver() {
}

void FaviconDriver::NotifyFaviconUpdatedObservers(
    FaviconDriverObserver::NotificationIconType notification_icon_type,
    const GURL& icon_url,
    bool icon_url_changed,
    const gfx::Image& image) {
  for (FaviconDriverObserver& observer : observer_list_) {
    observer.OnFaviconUpdated(this, notification_icon_type, icon_url,
                              icon_url_changed, image);
  }
}

}  // namespace favicon
