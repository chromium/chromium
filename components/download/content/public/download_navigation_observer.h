// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_CONTENT_PUBLIC_DOWNLOAD_NAVIGATION_OBSERVER_H_
#define COMPONENTS_DOWNLOAD_CONTENT_PUBLIC_DOWNLOAD_NAVIGATION_OBSERVER_H_

#include "base/memory/raw_ptr.h"
#include "components/download/public/background_service/navigation_monitor.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace download {

// Forwards navigation events to download service.
// Each DownloadNavigationObserver is associated with a particular WebContents.
class DownloadNavigationObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<DownloadNavigationObserver> {
 public:
  DownloadNavigationObserver(content::WebContents* web_contents,
                             NavigationMonitor* navigation_monitor);

  DownloadNavigationObserver(const DownloadNavigationObserver&) = delete;
  DownloadNavigationObserver& operator=(const DownloadNavigationObserver&) =
      delete;

  ~DownloadNavigationObserver() override;

 private:
  friend class content::WebContentsUserData<DownloadNavigationObserver>;

  // content::WebContentsObserver implementation.
  void DidStartLoading() override;
  void DidStopLoading() override;

  // Notifies |navigation_monitor_| about navigation events.
  void NotifyNavigationEvent(NavigationEvent navigation_event);

  // Used to inform the navigation events to download systems.
  raw_ptr<NavigationMonitor> navigation_monitor_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_CONTENT_PUBLIC_DOWNLOAD_NAVIGATION_OBSERVER_H_
