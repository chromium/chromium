// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_STRIP_SCROLL_BAR_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_STRIP_SCROLL_BAR_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/tabs/shared/rounded_scroll_bar.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace tabs {
class VerticalTabStripStateController;
}

// The scrollbar used for the pinned and unpinned tab containers in the vertical
// tab strip that updates its thickness based on the collapsed state of the tab
// strip.
class VerticalTabStripScrollBar : public tabs::RoundedScrollBar {
  METADATA_HEADER(VerticalTabStripScrollBar, tabs::RoundedScrollBar)

 public:
  explicit VerticalTabStripScrollBar(
      tabs::VerticalTabStripStateController* state_controller);

  VerticalTabStripScrollBar(const VerticalTabStripScrollBar&) = delete;
  VerticalTabStripScrollBar& operator=(const VerticalTabStripScrollBar&) =
      delete;

  ~VerticalTabStripScrollBar() override;

  // tabs::RoundedScrollBar:
  bool ShouldHaveRightMargin() const override;

 private:
  void OnCollapsedStateChanged(
      tabs::VerticalTabStripStateController* state_controller);

  bool tab_strip_collapsed_ = false;
  base::CallbackListSubscription collapsed_state_changed_subscription_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_STRIP_SCROLL_BAR_H_
