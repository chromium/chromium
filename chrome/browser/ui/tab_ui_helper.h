// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TAB_UI_HELPER_H_
#define CHROME_BROWSER_UI_TAB_UI_HELPER_H_

#include <string>

#include "chrome/browser/ui/tabs/contents_observing_tab_feature.h"

namespace tabs {
class TabInterface;
}

namespace ui {
class ImageModel;
}  // namespace ui

// TabUIHelper is used by UI code to obtain the title and favicon for a
// WebContents. The values returned by TabUIHelper differ from the WebContents
// when the WebContents hasn't loaded.
class TabUIHelper : public tabs::ContentsObservingTabFeature {
 public:
  explicit TabUIHelper(tabs::TabInterface& tab);
  ~TabUIHelper() override;

  // Get the title of the tab. When the associated WebContents' title is empty,
  // a customized title is used.
  std::u16string GetTitle() const;

  // Get the favicon of the tab. It will return a favicon from history service
  // if it needs to, otherwise, it will return the favicon of the WebContents.
  ui::ImageModel GetFavicon() const;

  // Return true if the throbber should be hidden during a page load.
  bool ShouldHideThrobber() const;

  void SetWasActiveAtLeastOnce();

  // tabs::ContentsObservingTabFeature override:
  void DidStopLoading() override;
  void OnVisibilityChanged(content::Visibility visiblity) override;

  void set_created_by_session_restore(bool created_by_session_restore) {
    created_by_session_restore_ = created_by_session_restore;
  }
  bool is_created_by_session_restore_for_testing() {
    return created_by_session_restore_;
  }

 private:
  bool was_active_at_least_once_ = false;
  bool created_by_session_restore_ = false;
};

#endif  // CHROME_BROWSER_UI_TAB_UI_HELPER_H_
