// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_ICON_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_ICON_CONTROLLER_H_

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "components/zoom/zoom_event_manager.h"
#include "components/zoom/zoom_event_manager_observer.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/view.h"

class PageActionIconContainer;
struct PageActionIconParams;
class ZoomView;

class PageActionIconController : public zoom::ZoomEventManagerObserver {
 public:
  PageActionIconController();
  PageActionIconController(const PageActionIconController&) = delete;
  PageActionIconController& operator=(const PageActionIconController&) = delete;
  ~PageActionIconController() override;

  void Init(const PageActionIconParams& params,
            PageActionIconContainer* icon_container);

  PageActionIconView* GetIconView(PageActionIconType type);

  // Updates the visual state of all enabled page action icons.
  void UpdateAll();

  bool IsAnyIconVisible() const;

  // Activates the first visible but inactive icon for accessibility. Returns
  // whether any icons were activated.
  bool ActivateFirstInactiveBubbleForAccessibility();

  // Update the icons' color.
  void SetIconColor(SkColor icon_color);

  // Update the icons' fonts.
  void SetFontList(const gfx::FontList& font_list);

  // See comment in browser_window.h for more info.
  void ZoomChangedForActiveTab(bool can_show_bubble);

  std::vector<const PageActionIconView*> GetPageActionIconViewsForTesting()
      const;

 private:
  // ZoomEventManagerObserver:
  // Updates the view for the zoom icon when default zoom levels change.
  void OnDefaultZoomLevelChanged() override;

  raw_ptr<PageActionIconContainer> icon_container_ = nullptr;

  raw_ptr<ZoomView> zoom_icon_ = nullptr;

  base::flat_map<PageActionIconType, PageActionIconView*>
      page_action_icon_views_;

  base::ScopedObservation<zoom::ZoomEventManager,
                          zoom::ZoomEventManagerObserver>
      zoom_observation_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_ICON_CONTROLLER_H_
