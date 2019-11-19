// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TAB_UI_HELPER_H_
#define CHROME_BROWSER_UI_TAB_UI_HELPER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/favicon_base/favicon_callback.h"
#include "components/favicon_base/favicon_types.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

// TabUIHelper is used by UI code to obtain the title and favicon for a
// WebContents. The values returned by TabUIHelper differ from the WebContents
// when the WebContents hasn't loaded.
class TabUIHelper : public content::WebContentsObserver,
                    public content::WebContentsUserData<TabUIHelper> {
 public:
  ~TabUIHelper() override;

  // Get the title of the tab. When the associated WebContents' title is empty,
  // a customized title is used.
  base::string16 GetTitle() const;

  // Get the favicon of the tab. It will return a favicon from history service
  // if it needs to, otherwise, it will return the favicon of the WebContents.
  gfx::Image GetFavicon() const;

  // Return true if the throbber should be hidden during a page load.
  bool ShouldHideThrobber() const;

  // Notifies TabUIHelper that the WebContents' initial navigation is delayed.
  // This is called by TabManager when it decides to delay a new tab's
  // navigation. TabUIHelper will obtain appropriate title and favicon after
  // receiving this signal.
  void NotifyInitialNavigationDelayed(bool is_navigation_delayed);

  // content::WebContentsObserver implementation
  void DidStopLoading() override;

  void set_was_active_at_least_once() { was_active_at_least_once_ = true; }
  void set_created_by_session_restore(bool created_by_session_restore) {
    created_by_session_restore_ = created_by_session_restore;
  }

 private:
  friend class content::WebContentsUserData<TabUIHelper>;

  struct TabUIData {
    explicit TabUIData(const GURL& url);
    base::string16 title;
    gfx::Image favicon;
  };

  explicit TabUIHelper(content::WebContents* contents);

  // Returns true if a favicon from history should be used. It is used when a
  // new tab is opened in the background and its initial navigation is delayed.
  bool ShouldUseFaviconFromHistory() const;

  void FetchFaviconFromHistory(const GURL& url,
                               favicon_base::FaviconImageCallback callback);
  void OnURLFaviconFetched(const favicon_base::FaviconImageResult& favicon);
  void OnHostFaviconFetched(const favicon_base::FaviconImageResult& favicon);
  void UpdateFavicon(const favicon_base::FaviconImageResult& favicon);

  bool was_active_at_least_once_ = false;
  bool is_navigation_delayed_ = false;
  bool created_by_session_restore_ = false;

  // The data that stores favicon and title. It is non-null only during initial
  // navigation when the tab is opened in background.
  std::unique_ptr<TabUIData> tab_ui_data_;
  base::CancelableTaskTracker favicon_tracker_;
  base::WeakPtrFactory<TabUIHelper> weak_ptr_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(TabUIHelper);
};

#endif  // CHROME_BROWSER_UI_TAB_UI_HELPER_H_
