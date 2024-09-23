// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_DRAG_CONTROLLER_INTERACTIVE_UITEST_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_DRAG_CONTROLLER_INTERACTIVE_UITEST_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "url/url_constants.h"

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
  TabDragControllerTest(const TabDragControllerTest&) = delete;
  TabDragControllerTest& operator=(const TabDragControllerTest&) = delete;
  ~TabDragControllerTest() override;

  // Cover for TabStrip::StopAnimating(true).
  void StopAnimating(TabStrip* tab_strip);

  // Adds |additional_tabs| new tabs to |browser| using the provided |url| or
  // blank. Stops animations and resets the ids of the tabs in |browser|.
  void AddTabsAndResetBrowser(Browser* browser,
                              int additional_tabs,
                              const GURL& url = GURL(url::kAboutBlankURL));

  // Resizes browser1 and browser2 to be side by side.
  void Resize(Browser* browser1, Browser* browser2);

  // Creates a new Browser and resizes browser() and the new browser to be side
  // by side.
  Browser* CreateAnotherBrowserAndResize();

  void SetWindowFinderForTabStrip(TabStrip* tab_strip,
                                  std::unique_ptr<WindowFinder> window_finder);

  const BrowserList* browser_list() const { return browser_list_; }

 protected:
  void HandleGestureEvent(TabStrip* tab_strip, ui::GestureEvent* event);

  bool HasDragStarted(TabStrip* tab_strip) const;

  // InProcessBrowserTest:
  void SetUp() override;

 private:
  raw_ptr<const BrowserList> browser_list_;
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
