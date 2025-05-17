// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LENS_LENS_OVERLAY_HELP_MENU_UTILS_H_
#define CHROME_BROWSER_UI_LENS_LENS_OVERLAY_HELP_MENU_UTILS_H_

namespace tabs {
class TabInterface;
}

namespace lens {

// Shows the feedback page.
void FeedbackRequestedByEvent(tabs::TabInterface* tab, int event_flags);

// Shows the info page.
void InfoRequestedByEvent(tabs::TabInterface* tab, int event_flags);

// Shows My Activity.
void ActivityRequestedByEvent(tabs::TabInterface* tab, int event_flags);

}  // namespace lens

#endif  // CHROME_BROWSER_UI_LENS_LENS_OVERLAY_HELP_MENU_UTILS_H_
