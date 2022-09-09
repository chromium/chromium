// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_TAB_DISCARD_TAB_HELPER_H_
#define CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_TAB_DISCARD_TAB_HELPER_H_

#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

// Per-tab class to manage discard state. When pages are in the background, they
// can be discarded to save memory. When the user returns to that tab, we need
// information about whether the page had previously been discarded in order to
// convey this information to the user.
class TabDiscardTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<TabDiscardTabHelper> {
 public:
  TabDiscardTabHelper(const TabDiscardTabHelper&) = delete;
  TabDiscardTabHelper& operator=(const TabDiscardTabHelper&) = delete;

  ~TabDiscardTabHelper() override;

  // Returns whether the chip associated with a discarded tab should be shown.
  bool IsChipVisible() const;

  // Returns whether the chip associated with a discarded tab should animate in.
  bool ShouldIconAnimate() const;

  // Indicates that the chip has been animated for the current discard.
  void SetWasAnimated();

  // content::WebContentsObserver
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;

 private:
  friend class content::WebContentsUserData<TabDiscardTabHelper>;
  explicit TabDiscardTabHelper(content::WebContents* contents);
  bool was_discarded_ = false;
  bool was_animated_ = false;
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_TAB_DISCARD_TAB_HELPER_H_
