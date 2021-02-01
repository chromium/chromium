// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ZOOM_CHROME_ZOOM_LEVEL_OTR_DELEGATE_H_
#define CHROME_BROWSER_UI_ZOOM_CHROME_ZOOM_LEVEL_OTR_DELEGATE_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/zoom_level_delegate.h"

namespace zoom {
class ZoomEventManager;
}

// This class is a light-weight version of ChromeZoomLevelPrefs and is used
// to connect an OTR StoragePartition's HostZoomMap to the OTR profile's
// ZoomEventManager.
class ChromeZoomLevelOTRDelegate : public content::ZoomLevelDelegate {
 public:
  ChromeZoomLevelOTRDelegate(
      base::WeakPtr<zoom::ZoomEventManager> zoom_event_manager);
  ~ChromeZoomLevelOTRDelegate() override;

  // content::ZoomLevelDelegate
  void InitHostZoomMap(content::HostZoomMap* host_zoom_map) override;

 private:
  // This is a callback function that receives notifications from HostZoomMap
  // when per-host zoom levels change. It is used to update the per-host
  // zoom levels (if any) managed by this class (for its associated partition).
  void OnZoomLevelChanged(const content::HostZoomMap::ZoomLevelChange& change);

  base::WeakPtr<zoom::ZoomEventManager> zoom_event_manager_;
  content::HostZoomMap* host_zoom_map_;
  base::CallbackListSubscription zoom_subscription_;

  DISALLOW_COPY_AND_ASSIGN(ChromeZoomLevelOTRDelegate);
};

#endif  // CHROME_BROWSER_UI_ZOOM_CHROME_ZOOM_LEVEL_OTR_DELEGATE_H_
