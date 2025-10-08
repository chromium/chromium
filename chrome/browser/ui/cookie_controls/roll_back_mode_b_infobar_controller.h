// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COOKIE_CONTROLS_ROLL_BACK_MODE_B_INFOBAR_CONTROLLER_H_
#define CHROME_BROWSER_UI_COOKIE_CONTROLS_ROLL_BACK_MODE_B_INFOBAR_CONTROLLER_H_

#include "base/scoped_observation.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

// Controls the Mode B rollback infobar, notifying users that they are no longer
// in the 3PCD 1% experiment.
class RollBackModeBInfoBarController : public content::WebContentsObserver,
                                       infobars::InfoBarManager::Observer {
 public:
  explicit RollBackModeBInfoBarController(content::WebContents* web_contents);
  ~RollBackModeBInfoBarController() override;

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void OnVisibilityChanged(content::Visibility visibility) override;

  // InfoBarManager::Observer:
  void OnInfoBarRemoved(infobars::InfoBar* infobar, bool animate) override;

 private:
  raw_ptr<infobars::InfoBar> infobar_ = nullptr;
  base::ScopedObservation<infobars::InfoBarManager,
                          infobars::InfoBarManager::Observer>
      infobar_scoped_observation_{this};
};

#endif  // CHROME_BROWSER_UI_COOKIE_CONTROLS_ROLL_BACK_MODE_B_INFOBAR_CONTROLLER_H_
