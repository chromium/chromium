// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXCLUSIVE_ACCESS_FULLSCREEN_WITHIN_TAB_HELPER_H_
#define CHROME_BROWSER_UI_EXCLUSIVE_ACCESS_FULLSCREEN_WITHIN_TAB_HELPER_H_

#include "content/public/browser/web_contents_user_data.h"

// Helper used by FullscreenController to track the state of a WebContents that
// is in fullscreen mode, but the browser window is not. See
// 'FullscreenWithinTab Note' in fullscreen_controller.h.
//
// The purpose of this class is to associate some fullscreen state at the tab
// level rather than at the Browser level. This allows tabs to be
// dragged/dropped between Browsers and have their fullscreen state handed off
// between FullscreenControllers as well.
//
// FullscreenWithinTabHelper is created on-demand, and its lifecycle is tied to
// that of its associated WebContents. It is automatically destroyed.
class FullscreenWithinTabHelper
    : public content::WebContentsUserData<FullscreenWithinTabHelper> {
 public:
  FullscreenWithinTabHelper(const FullscreenWithinTabHelper&) = delete;
  FullscreenWithinTabHelper& operator=(const FullscreenWithinTabHelper&) =
      delete;

  ~FullscreenWithinTabHelper() override;

  bool is_fullscreen_within_tab() const { return is_fullscreen_within_tab_; }

  void SetIsFullscreenWithinTab(bool is_fullscreen) {
    is_fullscreen_within_tab_ = is_fullscreen;
  }

  // Immediately remove and destroy the FullscreenWithinTabHelper instance
  // associated with |web_contents|.
  static void RemoveForWebContents(content::WebContents* web_contents);

 private:
  friend class content::WebContentsUserData<FullscreenWithinTabHelper>;
  explicit FullscreenWithinTabHelper(content::WebContents* ignored);

  bool is_fullscreen_within_tab_ = false;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_UI_EXCLUSIVE_ACCESS_FULLSCREEN_WITHIN_TAB_HELPER_H_
