// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TAB_UI_HELPER_H_
#define CHROME_BROWSER_UI_TAB_UI_HELPER_H_

#include <optional>
#include <string>
#include <vector>

#include "base/byte_size.h"
#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "chrome/browser/ui/tabs/contents_observing_tab_feature.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace content {
class NavigationEntry;
class NavigationHandle;
class Page;
}  // namespace content

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace ui {
class ImageModel;
}  // namespace ui

// TabUIHelper is used by UI code to obtain the title and favicon for a
// WebContents. The values returned by TabUIHelper differ from the WebContents
// when the WebContents hasn't loaded.
class TabUIHelper : public tabs::ContentsObservingTabFeature {
 public:
  DECLARE_USER_DATA(TabUIHelper);

  explicit TabUIHelper(tabs::TabInterface& tab);
  ~TabUIHelper() override;

  static TabUIHelper* From(tabs::TabInterface* tab);
  static const TabUIHelper* From(const tabs::TabInterface* tab);

  base::CallbackListSubscription AddTabUIChangeCallback(
      base::RepeatingClosure callback);

  // Get the title of the tab. When the associated WebContents' title is empty,
  // a customized title is used.
  std::u16string GetTitle() const;

  bool ShouldRenderLoadingTitle();

  bool ShouldThemifyFavicon();

#if !BUILDFLAG(IS_ANDROID)
  bool ShouldDisplayFavicon();
  bool IsMonochromeFavicon();
#endif

  // Get the favicon of the tab. It will return a favicon from history service
  // if it needs to, otherwise, it will return the favicon of the WebContents.
  ui::ImageModel GetFavicon();

  // Return true if the throbber should be hidden during a page load.
  bool ShouldHideThrobber() const;

  void SetWasActiveAtLeastOnce();

  // Returns true if the tab is crashed and false otherwise.
  bool IsCrashed();

  bool ShouldDisplayURL();

  GURL GetVisibleURL();

  // tabs::ContentsObservingTabFeature override:
  void TitleWasSet(content::NavigationEntry* entry) override;
  void DidStopLoading() override;
  void OnVisibilityChanged(content::Visibility visiblity) override;
  void WasDiscarded() override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
#if !BUILDFLAG(IS_ANDROID)
  void PrimaryPageChanged(content::Page& page) override;
#endif

  void set_created_by_session_restore(bool created_by_session_restore) {
    created_by_session_restore_ = created_by_session_restore;
  }
  bool is_created_by_session_restore_for_testing() {
    return created_by_session_restore_;
  }

  void SetNeedsAttention(bool needs_attention);
  bool needs_attention() const { return needs_attention_; }

  // Returns true if the tab is eligible to show the discard UI.
  bool ShouldShowDiscardStatus();

  // Returns the amount of bytes saved from discarding the tab.
  std::optional<base::ByteSize> GetDiscardedMemorySavings();

  bool was_active_at_least_once_for_testing() const {
    return was_active_at_least_once_;
  }

 private:
  void OnTabPinnedStatusChange(tabs::TabInterface* tab_interface,
                               bool new_pinned_state);

  bool was_active_at_least_once_ = false;
  bool created_by_session_restore_ = false;
  bool needs_attention_ = false;
  base::CallbackListSubscription pin_tab_subscription_;

  base::RepeatingClosureList tab_ui_change_callbacks_;

  ui::ScopedUnownedUserData<TabUIHelper> scoped_unowned_user_data_;
};

#endif  // CHROME_BROWSER_UI_TAB_UI_HELPER_H_
