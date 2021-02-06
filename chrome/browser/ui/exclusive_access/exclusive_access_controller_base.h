// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXCLUSIVE_ACCESS_EXCLUSIVE_ACCESS_CONTROLLER_BASE_H_
#define CHROME_BROWSER_UI_EXCLUSIVE_ACCESS_EXCLUSIVE_ACCESS_CONTROLLER_BASE_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_bubble_type.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class ExclusiveAccessManager;
class GURL;

namespace content {
class WebContents;
}

// The base class for the different exclusive access controllers like the
// FullscreenController, KeyboardLockController, and MouseLockController which
// controls lifetime for which the resource (screen/mouse/keyboard) is held
// exclusively.
class ExclusiveAccessControllerBase : public content::NotificationObserver {
 public:
  explicit ExclusiveAccessControllerBase(ExclusiveAccessManager* manager);
  ~ExclusiveAccessControllerBase() override;

  GURL GetExclusiveAccessBubbleURL() const;
  virtual GURL GetURLForExclusiveAccessBubble() const;

  content::WebContents* exclusive_access_tab() const {
    return tab_with_exclusive_access_;
  }

  // Functions implemented by derived classes:

  // Control behavior when escape is pressed returning true if it was handled.
  virtual bool HandleUserPressedEscape() = 0;

  // Called by Browser in response to call from ExclusiveAccessBubble.
  virtual void ExitExclusiveAccessToPreviousState() = 0;

  // Called by ExclusiveAccessManager in response to calls from Browser.
  virtual void OnTabDeactivated(content::WebContents* web_contents);
  virtual void OnTabDetachedFromView(content::WebContents* web_contents);
  virtual void OnTabClosing(content::WebContents* web_contents);

  // Callbacks ////////////////////////////////////////////////////////////////

  // content::NotificationObserver to detect page navigation and exit exclusive
  // access.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // For recording UMA.
  void RecordBubbleReshownUMA();
  // Called when the exclusive access session ends.
  void RecordExitingUMA();

 protected:
  void SetTabWithExclusiveAccess(content::WebContents* tab);

  ExclusiveAccessManager* exclusive_access_manager() const { return manager_; }

  // Exits exclusive access mode for the tab if currently exclusive.
  virtual void ExitExclusiveAccessIfNecessary() = 0;

  // Notifies the tab that it has been forced out of exclusive access mode
  // if necessary.
  virtual void NotifyTabExclusiveAccessLost() = 0;

  // Records the BubbleReshowsPerSession data to the appropriate histogram for
  // this controller.
  virtual void RecordBubbleReshowsHistogram(int bubble_reshow_count) = 0;

 private:
  void UpdateNotificationRegistrations();

  ExclusiveAccessManager* const manager_;

  content::NotificationRegistrar registrar_;

  content::WebContents* tab_with_exclusive_access_ = nullptr;

  // The number of bubble re-shows for the current session (reset upon exiting).
  int bubble_reshow_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(ExclusiveAccessControllerBase);
};

#endif  // CHROME_BROWSER_UI_EXCLUSIVE_ACCESS_EXCLUSIVE_ACCESS_CONTROLLER_BASE_H_
