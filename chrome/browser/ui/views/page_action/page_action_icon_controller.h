// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_ICON_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_ICON_CONTROLLER_H_

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view_observer.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/zoom/zoom_event_manager.h"
#include "components/zoom/zoom_event_manager_observer.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/view.h"

class Browser;
class PageActionIconContainer;
struct PageActionIconParams;
class ZoomView;

class PageActionIconController : public PageActionIconViewObserver,
                                 public zoom::ZoomEventManagerObserver,
                                 public content::WebContentsObserver {
 public:
  PageActionIconController();
  PageActionIconController(const PageActionIconController&) = delete;
  PageActionIconController& operator=(const PageActionIconController&) = delete;
  ~PageActionIconController() override;

  void Init(const PageActionIconParams& params,
            PageActionIconContainer* icon_container);

  PageActionIconView* GetIconView(PageActionIconType type);
  PageActionIconType GetIconType(PageActionIconView* view);

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

  // Observe the new web contents.
  void UpdateWebContents(content::WebContents* contents);

  // WebContentsObserver
  void ReadyToCommitNavigation(
      content::NavigationHandle* navigation_handle) override;
  void PrimaryPageChanged(content::Page& page) override;

  std::vector<const PageActionIconView*> GetPageActionIconViewsForTesting()
      const;

 private:
  using IconViews =
      base::flat_map<PageActionIconType,
                     raw_ptr<PageActionIconView, CtnExperimental>>;

  // PageActionIconViewObserver:
  void OnPageActionIconViewShown(PageActionIconView* view) override;
  void OnPageActionIconViewClicked(PageActionIconView* view) override;

  // ZoomEventManagerObserver:
  // Updates the view for the zoom icon when default zoom levels change.
  void OnDefaultZoomLevelChanged() override;

  // Returns the number of page actions which are currently visible. Does not
  // include permanent page actions, such as the bookmarks star and sharing
  // hub.
  int VisibleEphemeralActionCount() const;

  // Logs UMA data as appropriate when the current displayed URL changes.
  void RecordMetricsOnURLChange(GURL url);

  // Logs UMA data about the set of currently visible page actions overall, eg.
  // the total number of page actions shown.
  void RecordOverallMetrics();

  // Logs UMA data about an individual visible page action, eg. the type of
  // action shown.
  void RecordIndividualMetrics(PageActionIconType type,
                               PageActionIconView* view) const;

  // Logs UMA data when an individual visible page action is clicked.
  void RecordClickMetrics(PageActionIconType type,
                          PageActionIconView* view) const;

  raw_ptr<Browser> browser_ = nullptr;

  raw_ptr<PageActionIconContainer> icon_container_ = nullptr;

  raw_ptr<ZoomView> zoom_icon_ = nullptr;

  IconViews page_action_icon_views_;

  PrefChangeRegistrar pref_change_registrar_;

  std::map<GURL, std::vector<raw_ptr<PageActionIconView, VectorExperimental>>>
      page_actions_excluded_from_logging_;

  base::ScopedObservation<zoom::ZoomEventManager,
                          zoom::ZoomEventManagerObserver>
      zoom_observation_{this};

  // Max number of actions shown concurrently after the latest page change, as
  // tracked in metrics logging. Used to ensure we don't double-count certain
  // metrics.
  int max_actions_recorded_on_current_page_ = 0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_ICON_CONTROLLER_H_
