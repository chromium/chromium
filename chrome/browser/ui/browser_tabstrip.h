// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_TABSTRIP_H_
#define CHROME_BROWSER_UI_BROWSER_TABSTRIP_H_

#include <optional>

#include "chrome/browser/ui/browser_navigator_params.h"
#include "components/tab_groups/tab_group_id.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"

class Browser;
class GURL;

namespace blink {
namespace mojom {
class WindowFeatures;
}
}  // namespace blink

namespace gfx {
class Rect;
}

namespace chrome {

// Adds a tab to the tab strip of the specified browser and loads |url| into it.
// If |url| is an empty URL, then the new tab-page is laoded. An |index| of -1
// means to append it to the end of the tab strip.
content::WebContents* AddAndReturnTabAt(
    Browser* browser,
    const GURL& url,
    int index,
    bool foreground,
    std::optional<tab_groups::TabGroupId> group = std::nullopt);

// Same as above, but eats the return value to make Bind*() easier.
void AddTabAt(Browser* browser,
              const GURL& url,
              int index,
              bool foreground,
              std::optional<tab_groups::TabGroupId> group = std::nullopt);

// Adds a selected tab with the specified URL and transition, returns the
// created WebContents.
content::WebContents* AddSelectedTabWithURL(Browser* browser,
                                            const GURL& url,
                                            ui::PageTransition transition);

// Creates a new tab with the already-created WebContents 'new_contents'.
// The window for the added contents will be reparented correctly when this
// method returns. If |disposition| is NEW_POPUP, |window_features| should hold
// the initial position and size and other features of the new window.
// |window_action| may optionally specify whether the window should be shown or
// activated.
// Returns the WebContents instance where navigation completed.
// Invariant: If `new_contents` is not nullptr, then the returned instance
// should always match new_contents.get().
content::WebContents* AddWebContents(
    Browser* browser,
    content::WebContents* source_contents,
    std::unique_ptr<content::WebContents> new_contents,
    const GURL& target_url,
    WindowOpenDisposition disposition,
    const blink::mojom::WindowFeatures& window_features,
    NavigateParams::WindowAction window_action = NavigateParams::SHOW_WINDOW);

// Closes the specified WebContents in the specified Browser. If
// |add_to_history| is true, an entry in the historical tab database is created.
void CloseWebContents(Browser* browser,
                      content::WebContents* contents,
                      bool add_to_history);

// Configures |nav_params| to create a new tab group with the source, if
// applicable.
void ConfigureTabGroupForNavigation(NavigateParams* nav_params);

// Decides whether or not to create a new tab group.
bool ShouldAutoCreateGroupForNavigation(NavigateParams* nav_params);

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_BROWSER_TABSTRIP_H_
