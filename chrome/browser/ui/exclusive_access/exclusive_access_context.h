// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXCLUSIVE_ACCESS_EXCLUSIVE_ACCESS_CONTEXT_H_
#define CHROME_BROWSER_UI_EXCLUSIVE_ACCESS_EXCLUSIVE_ACCESS_CONTEXT_H_

#include "build/build_config.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_bubble_hide_callback.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_bubble_type.h"

class GURL;
class Profile;

namespace content {
class WebContents;
}

// Context in which exclusive access operation is being performed. This
// interface is implemented once in Browser context and in Platform Application
// context.
class ExclusiveAccessContext {
 public:

  virtual ~ExclusiveAccessContext() = default;

  // Returns the current profile associated with the window.
  virtual Profile* GetProfile() = 0;

  // Returns true if the window hosting the exclusive access bubble is
  // fullscreen.
  virtual bool IsFullscreen() const = 0;

  // Called when we transition between tab and browser fullscreen. This method
  // updates the UI by showing/hiding the tab strip, toolbar and bookmark bar
  // in the browser fullscreen. Currently only supported on Mac.
  virtual void UpdateUIForTabFullscreen() {}

  // Updates the toolbar state to be hidden or shown in fullscreen according to
  // the preference's state. Only supported on Mac.
  virtual void UpdateFullscreenToolbar() {}

  // Enters fullscreen and update exit bubble.
  virtual void EnterFullscreen(const GURL& url,
                               ExclusiveAccessBubbleType bubble_type,
                               const int64_t display_id) = 0;

  // Exits fullscreen and update exit bubble.
  virtual void ExitFullscreen() = 0;

  // Updates the content of exclusive access exit bubble content.
  virtual void UpdateExclusiveAccessExitBubbleContent(
      const GURL& url,
      ExclusiveAccessBubbleType bubble_type,
      ExclusiveAccessBubbleHideCallback bubble_first_hide_callback,
      bool notify_download,
      bool force_update) = 0;

  virtual bool IsExclusiveAccessBubbleDisplayed() const = 0;

  // Informs the exclusive access system of some user input, which may update
  // internal timers and/or re-display the bubble.
  virtual void OnExclusiveAccessUserInput() = 0;

  // Returns the currently active WebContents, or nullptr if there is none.
  virtual content::WebContents* GetActiveWebContents() = 0;

  // There are special modes where the user isn't allowed to exit fullscreen on
  // their own, and this function allows us to check for that.
  virtual bool CanUserExitFullscreen() const = 0;
};

#endif  // CHROME_BROWSER_UI_EXCLUSIVE_ACCESS_EXCLUSIVE_ACCESS_CONTEXT_H_
