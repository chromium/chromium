// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_THUMBNAILS_THUMBNAIL_PAGE_EVENT_ADAPTER_H_
#define CHROME_BROWSER_UI_THUMBNAILS_THUMBNAIL_PAGE_EVENT_ADAPTER_H_

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "chrome/browser/ui/thumbnails/thumbnail_page_observer.h"
#include "content/public/browser/web_contents_observer.h"
#include "url/gurl.h"

namespace content {
class NavigationHandle;
class RenderFrameHost;
}  // namespace content

namespace thumbnails {

// Base class for thumbnail tab helper; processes specific web contents events
// into a filtered-down set of navigation and loading events for ease of
// processing.
class ThumbnailPageEventAdapter : public content::WebContentsObserver {
 public:
  explicit ThumbnailPageEventAdapter(content::WebContents* contents);
  ~ThumbnailPageEventAdapter() override;

  bool is_unloading() const { return is_unloading_; }

  void AddObserver(ThumbnailPageObserver* observer);
  void RemoveObserver(ThumbnailPageObserver* observer);

 protected:
  // WebContentsObserver:
  void OnVisibilityChanged(content::Visibility visibility) override;
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidRedirectNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DocumentAvailableInMainFrame() override;
  void DidFirstVisuallyNonEmptyPaint() override;
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;
  void DidFailLoad(content::RenderFrameHost* render_frame_host,
                   const GURL& validated_url,
                   int error_code,
                   const base::string16& error_description) override;
  void NavigationStopped() override;
  void BeforeUnloadFired(bool proceed,
                         const base::TimeTicks& proceed_time) override;
  void BeforeUnloadDialogCancelled() override;

 private:
  base::ObserverList<ThumbnailPageObserver> observers_;

  // True if the current page is in the process of being unloaded from the
  // browser (e.g. on a tab or window close).
  bool is_unloading_ = false;

  DISALLOW_COPY_AND_ASSIGN(ThumbnailPageEventAdapter);
};

}  // namespace thumbnails

#endif  // CHROME_BROWSER_UI_THUMBNAILS_THUMBNAIL_PAGE_EVENT_ADAPTER_H_
