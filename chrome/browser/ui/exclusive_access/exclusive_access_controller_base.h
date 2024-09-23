// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXCLUSIVE_ACCESS_EXCLUSIVE_ACCESS_CONTROLLER_BASE_H_
#define CHROME_BROWSER_UI_EXCLUSIVE_ACCESS_EXCLUSIVE_ACCESS_CONTROLLER_BASE_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_bubble_type.h"
#include "content/public/browser/web_contents_observer.h"

class ExclusiveAccessManager;
class GURL;

namespace content {
class WebContents;
}

// The base class for the different exclusive access controllers like the
// FullscreenController, KeyboardLockController, and PointerLockController which
// controls lifetime for which the resource (screen/mouse-pointer/keyboard) is
// held exclusively.
class ExclusiveAccessControllerBase {
 public:
  explicit ExclusiveAccessControllerBase(ExclusiveAccessManager* manager);

  ExclusiveAccessControllerBase(const ExclusiveAccessControllerBase&) = delete;
  ExclusiveAccessControllerBase& operator=(
      const ExclusiveAccessControllerBase&) = delete;

  virtual ~ExclusiveAccessControllerBase();

  GURL GetExclusiveAccessBubbleURL() const;
  virtual GURL GetURLForExclusiveAccessBubble() const;

  content::WebContents* exclusive_access_tab() const {
    return web_contents_observer_.web_contents();
  }

  // Functions implemented by derived classes:

  // Called when Esc is pressed. Returns true if the event is handled.
  virtual bool HandleUserPressedEscape() = 0;

  // Called when Esc is held for longer than the press-and-hold duration.
  virtual void HandleUserHeldEscape() = 0;

  // Called when Esc is released before reaching the press-and-hold duration.
  virtual void HandleUserReleasedEscapeEarly() = 0;

  // Returns true if the controller requires press-and-hold to exit.
  virtual bool RequiresPressAndHoldEscToExit() const = 0;

  // Called by Browser in response to call from ExclusiveAccessBubble.
  virtual void ExitExclusiveAccessToPreviousState() = 0;

  // Called by ExclusiveAccessManager in response to calls from Browser.
  virtual void OnTabDeactivated(content::WebContents* web_contents);
  virtual void OnTabDetachedFromView(content::WebContents* web_contents);
  virtual void OnTabClosing(content::WebContents* web_contents);

 protected:
  void SetTabWithExclusiveAccess(content::WebContents* tab);

  ExclusiveAccessManager* exclusive_access_manager() const { return manager_; }

  // Exits exclusive access mode for the tab if currently exclusive.
  virtual void ExitExclusiveAccessIfNecessary() = 0;

  // Notifies the tab that it has been forced out of exclusive access mode
  // if necessary.
  virtual void NotifyTabExclusiveAccessLost() = 0;

 private:
  const raw_ptr<ExclusiveAccessManager> manager_;

  class WebContentsObserver : public content::WebContentsObserver {
   public:
    explicit WebContentsObserver(ExclusiveAccessControllerBase& controller);
    // Detect page navigation and exit exclusive access.
    void NavigationEntryCommitted(
        const content::LoadCommittedDetails& load_details) override;
    // Let the controller set the WebContents to be observed.
    using content::WebContentsObserver::Observe;

   private:
    const raw_ref<ExclusiveAccessControllerBase> controller_;
  } web_contents_observer_{*this};
};

#endif  // CHROME_BROWSER_UI_EXCLUSIVE_ACCESS_EXCLUSIVE_ACCESS_CONTROLLER_BASE_H_
