// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ACCESSIBILITY_CAPTION_BUBBLE_SESSION_OBSERVER_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_ACCESSIBILITY_CAPTION_BUBBLE_SESSION_OBSERVER_VIEWS_H_

#include "components/live_caption/caption_bubble_session_observer.h"

#include "content/public/browser/web_contents_observer.h"

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

namespace captions {

class CaptionBubbleSessionObserverViews : public CaptionBubbleSessionObserver,
                                          public content::WebContentsObserver {
 public:
  explicit CaptionBubbleSessionObserverViews(
      content::WebContents* web_contents);
  ~CaptionBubbleSessionObserverViews() override;
  CaptionBubbleSessionObserverViews(const CaptionBubbleSessionObserverViews&) =
      delete;
  CaptionBubbleSessionObserverViews& operator=(
      const CaptionBubbleSessionObserverViews&) = delete;

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void WebContentsDestroyed() override;

  // CaptionBubbleSessionObserver:
  void SetEndSessionCallback(EndSessionCallback callback) override;

 private:
  std::string web_contents_id_;
  EndSessionCallback end_session_callback_;
};

}  // namespace captions

#endif  // CHROME_BROWSER_UI_VIEWS_ACCESSIBILITY_CAPTION_BUBBLE_SESSION_OBSERVER_VIEWS_H_
