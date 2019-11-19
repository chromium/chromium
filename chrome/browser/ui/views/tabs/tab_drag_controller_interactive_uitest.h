// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_DRAG_CONTROLLER_INTERACTIVE_UITEST_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_DRAG_CONTROLLER_INTERACTIVE_UITEST_H_

#include <string>

#include "base/macros.h"
#include "chrome/test/base/in_process_browser_test.h"

class Browser;
class BrowserList;
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
class TabDragControllerTest : public InProcessBrowserTest {
 public:
  TabDragControllerTest();
  ~TabDragControllerTest() override;

  // Cover for TabStrip::StopAnimating(true).
  void StopAnimating(TabStrip* tab_strip);

  // Adds a new blank tab to |browser|, stops animations and resets the ids of
  // the tabs in |browser|.
  void AddTabsAndResetBrowser(Browser* browser, int additional_tabs);

  // Resizes browser1 and browser2 to be side by side.
  void Resize(Browser* browser1, Browser* browser2);

  // Creates a new Browser and resizes browser() and the new browser to be side
  // by side.
  Browser* CreateAnotherBrowserAndResize();

  void SetWindowFinderForTabStrip(TabStrip* tab_strip,
                                  std::unique_ptr<WindowFinder> window_finder);

  const BrowserList* browser_list;

 protected:
  void HandleGestureEvent(TabStrip* tab_strip, ui::GestureEvent* event);

  bool HasDragStarted(TabStrip* tab_strip) const;

  // InProcessBrowserTest:
  void SetUp() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(TabDragControllerTest);
};

namespace test {

// Returns the TabStrip for |browser|.
TabStrip* GetTabStripForBrowser(Browser* browser);

// Sets the id of |web_contents| to |id|.
void SetID(content::WebContents* web_contents, int id);

// Resets the ids of all the tabs in |model| starting at |start|. That is, the
// id of the first tab is set to |start|, the second tab |start + 1| ...
void ResetIDs(TabStripModel* model, int start);

// Returns a string representation of the ids of the tabs in |model|. Each id
// is separated by a space.
std::string IDString(TabStripModel* model);

}  // namespace test

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_DRAG_CONTROLLER_INTERACTIVE_UITEST_H_
