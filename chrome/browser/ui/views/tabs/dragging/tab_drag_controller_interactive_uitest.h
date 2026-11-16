// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_DRAGGING_TAB_DRAG_CONTROLLER_INTERACTIVE_UITEST_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_DRAGGING_TAB_DRAG_CONTROLLER_INTERACTIVE_UITEST_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/views/tabs/dragging/tab_drag_controller.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "url/url_constants.h"

class BrowserList;
class BrowserWindowInterface;
class TabStrip;
class TabStripModel;
class WindowFinder;

namespace content {
class WebContents;
}

namespace ui {
class GestureEvent;
}

// TabDragControllerTest is the basis for the two tests that exercise
// TabDragController.
class TabDragControllerTest : public InteractiveBrowserTest {
 public:
  TabDragControllerTest();
  TabDragControllerTest(const TabDragControllerTest&) = delete;
  TabDragControllerTest& operator=(const TabDragControllerTest&) = delete;
  ~TabDragControllerTest() override;

  // Cover for TabStrip::StopAnimating(true).
  void StopAnimating(TabStrip* tab_strip);

  // Adds `additional_tabs` new tabs to `browser` using the provided `url` or
  // blank. Stops animations and resets the ids of the tabs in `browser`.
  void AddTabsAndResetBrowser(BrowserWindowInterface* browser,
                              int additional_tabs,
                              const GURL& url = GURL(url::kAboutBlankURL));

  // Resizes browser1 and browser2 to be side by side.
  void Resize(BrowserWindowInterface* browser1,
              BrowserWindowInterface* browser2);

  // Creates a new Browser and resizes browser() and the new browser to be side
  // by side.
  BrowserWindowInterface* CreateAnotherBrowserAndResize();

  void SetWindowFinderForTabStrip(TabStrip* tab_strip,
                                  std::unique_ptr<WindowFinder> window_finder);

  const BrowserList* browser_list() const { return browser_list_; }

  // Returns true if the tab dragging info is correctly set on / cleared from
  // the window of `attached_tabstrip`. Always true outside ChromeOS.
  bool IsTabDraggingInfoSet(TabStrip* attached_tabstrip) const;
  bool IsTabDraggingInfoCleared(TabStrip* attached_tabstrip) const;

 protected:
  void HandleGestureEvent(TabStrip* tab_strip, ui::GestureEvent* event);

  bool HasDragStarted(TabStrip* tab_strip) const;

  // Returns true if `tab_strip`'s drag controller is in the state where it has
  // detached into a new browser and is waiting for that browser's widget to
  // become visible before entering the window-move loop.
  bool IsWaitingForWindowToShow(TabStrip* tab_strip) const;

  // Returns true if any browser's drag controller is actively dragging a
  // detached window in a move loop or system DnD session (and has finished
  // waiting for the window to show).
  bool IsWindowDragLoopActive() const;

  // Returns true if `tab_strip` owns an active drag controller in the
  // `kDraggingTabs` state and is not inside a nested move loop.
  bool IsDraggingTabsIn(TabStrip* tab_strip) const;

  // Returns the native window of the widget currently attached to the active
  // `TabDragController`, or null if no drag is active.
  gfx::NativeWindow GetAttachedDragWindow() const;

  void SetTabDragPointResolver(TabDragPointResolver& resolver);

  // InteractiveBrowserTest:
  void SetUp() override;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  raw_ptr<const BrowserList> browser_list_;
};

namespace test {

// Returns the TabStrip for `browser`.
TabStrip* GetTabStripForBrowser(BrowserWindowInterface* browser);

// Sets the id of `web_contents` to `id`.
void SetID(content::WebContents* web_contents, int id);

// Resets the ids of all the tabs in `model` starting at `start`. That is, the
// id of the first tab is set to `start`, the second tab `start + 1` ...
void ResetIDs(TabStripModel* model, int start);

// Returns a string representation of the ids of the tabs in `model`. Each id
// is separated by a space.
std::string IDString(TabStripModel* model);

// Returns true if `tab_strip` owns an active drag controller.
bool IsDragSessionActive(TabStrip* tab_strip);

// Returns how far below `tab_strip` a drag has to go to detach.
int GetDetachY(TabStrip* tab_strip);

// Returns whether `browser`'s window is marked as dragged (ChromeOS only).
bool GetIsDragged(BrowserWindowInterface* browser);

// Returns a point within `target_bounds` to drag to, near `preferred_x` but
// right of `source_bounds`, which can overlap the target on small screens.
gfx::Point DragPointInTabStrip(const gfx::Rect& target_bounds,
                               const gfx::Rect& source_bounds,
                               int preferred_x);

// If this returns false, tests must use mouse emulation to resize windows.
bool PlatformSupportsScreenCoordinates();

}  // namespace test

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_DRAGGING_TAB_DRAG_CONTROLLER_INTERACTIVE_UITEST_H_
