// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/content/public/download_navigation_observer.h"

#include <memory>

namespace download {

DownloadNavigationObserver::DownloadNavigationObserver(
    content::WebContents* web_contents,
    NavigationMonitor* navigation_monitor)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<DownloadNavigationObserver>(*web_contents),
      navigation_monitor_(navigation_monitor) {}

DownloadNavigationObserver::~DownloadNavigationObserver() = default;

void DownloadNavigationObserver::DidStartLoading() {
  NotifyNavigationEvent(NavigationEvent::START_NAVIGATION);
}

void DownloadNavigationObserver::DidStopLoading() {
  NotifyNavigationEvent(NavigationEvent::NAVIGATION_COMPLETE);
}

void DownloadNavigationObserver::NotifyNavigationEvent(
    NavigationEvent navigation_event) {
  DCHECK(navigation_monitor_);
  navigation_monitor_->OnNavigationEvent(navigation_event);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(DownloadNavigationObserver);

}  // namespace download
