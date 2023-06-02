// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LOCATION_BAR_LOCATION_BAR_H_
#define CHROME_BROWSER_UI_LOCATION_BAR_LOCATION_BAR_H_

#include <stddef.h>

#include "base/time/time.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

class LocationBarTesting;
class OmniboxView;

namespace content {
class WebContents;
}

// The LocationBar class is a virtual interface, defining access to the
// window's location bar component.  This class exists so that cross-platform
// components like the browser command system can talk to the platform
// specific implementations of the location bar control.  It also allows the
// location bar to be mocked for testing.
class LocationBar {
 public:
  // The details necessary to open the user's desired omnibox match.
  virtual GURL GetDestinationURL() const = 0;
  virtual WindowOpenDisposition GetWindowOpenDisposition() const = 0;
  virtual ui::PageTransition GetPageTransition() const = 0;
  virtual base::TimeTicks GetMatchSelectionTimestamp() const = 0;
  virtual bool IsInputTypedUrlWithoutScheme() const = 0;
  virtual bool IsInputTypedUrlWithHttpScheme() const = 0;

  // Focuses the location bar. User-initiated focuses (like pressing Ctrl+L)
  // should have |is_user_initiated| set to true. In those cases, we want to
  // take some extra steps, like selecting everything and maybe uneliding.
  //
  // Renderer-initiated focuses (like browser startup or NTP finished loading),
  // should have |is_user_initiated| set to false, so we can avoid disrupting
  // user actions and avoid requesting on-focus suggestions.
  virtual void FocusLocation(bool is_user_initiated) = 0;

  // Puts the user into keyword mode with their default search provider.
  // TODO(tommycli): See if there's a more descriptive name for this method.
  virtual void FocusSearch() = 0;

  // Updates the state of the images showing the content settings status.
  virtual void UpdateContentSettingsIcons() = 0;

  // Saves the state of the location bar to the specified WebContents, so that
  // it can be restored later. (Done when switching tabs).
  virtual void SaveStateToContents(content::WebContents* contents) = 0;

  // Reverts the location bar.  The bar's permanent text will be shown.
  virtual void Revert() = 0;

  virtual const OmniboxView* GetOmniboxView() const = 0;
  virtual OmniboxView* GetOmniboxView() = 0;

  // Returns a pointer to the testing interface.
  virtual LocationBarTesting* GetLocationBarForTesting() = 0;

 protected:
  virtual ~LocationBar() = default;
};

class LocationBarTesting {
 public:
  // Invokes the content setting image at |index|, displaying the bubble.
  // Returns false if there is none.
  virtual bool TestContentSettingImagePressed(size_t index) = 0;

  // Returns if the content setting image at |index| is displaying a bubble.
  virtual bool IsContentSettingBubbleShowing(size_t index) = 0;

 protected:
  virtual ~LocationBarTesting() = default;
};

#endif  // CHROME_BROWSER_UI_LOCATION_BAR_LOCATION_BAR_H_
