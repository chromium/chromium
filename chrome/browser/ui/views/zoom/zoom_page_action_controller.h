// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ZOOM_ZOOM_PAGE_ACTION_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_ZOOM_ZOOM_PAGE_ACTION_CONTROLLER_H_

#include "base/scoped_observation.h"
#include "chrome/browser/ui/tabs/public/tab_interface.h"
#include "components/zoom/zoom_observer.h"

namespace zoom {

class ZoomController;

// This class observes the ZoomController and update the action accordingly to
// reflect the current zoom level.
class ZoomPageActionController : public ZoomObserver {
 public:
  explicit ZoomPageActionController(tabs::TabInterface& tab_interface);

  ZoomPageActionController(const ZoomPageActionController&) = delete;
  ZoomPageActionController& operator=(const ZoomPageActionController&) = delete;

  ~ZoomPageActionController() override;

  // ZoomObserver:
  void OnZoomControllerDestroyed(ZoomController* source) final;
  void OnZoomChanged(const ZoomController::ZoomChangedEventData& data) final;

 private:
  // Depending on the zoom level, the page action icon and tooltip should be
  // updated accordingly. This method ensures that the page action state is
  // correctly updated.
  void UpdatePageAction();

  // The `ZoomPageController` is per-tab and the `ZoomController` is per-tab and
  // web contents specific. In the case where the tab or the web content is
  // gone, the `ZoomController` will be destroyed. In the case where the web
  // content is gone, the `ZoomPageActionController` should observe the new web
  // content in the tab.
  void WillDiscardContents(tabs::TabInterface* tab,
                           content::WebContents* old_content,
                           content::WebContents* new_content);

  // The zoom feature is per-tab. It's safe to assume that `tab_` will always be
  // valid because the tab interface owns this object.
  const raw_ref<tabs::TabInterface> tab_interface_;

  // Used to observe zoom level change. It get reset when web content is gone
  // and get re-instantiated with the new active web content.
  base::ScopedObservation<ZoomController, ZoomObserver> zoom_observation_{this};

  base::CallbackListSubscription will_discard_contents_subscription_;
};

}  // namespace zoom

#endif  // CHROME_BROWSER_UI_VIEWS_ZOOM_ZOOM_PAGE_ACTION_CONTROLLER_H_
