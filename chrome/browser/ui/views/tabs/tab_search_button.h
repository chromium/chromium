// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_SEARCH_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_SEARCH_BUTTON_H_

#include "chrome/browser/ui/views/tab_search_bubble_host.h"
#include "chrome/browser/ui/views/tabs/new_tab_button.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace gfx {
class Canvas;
}

class TabStrip;

// TabSearchButton should leverage the look and feel of the existing
// NewTabButton for sizing and appropriate theming. This class updates the
// NewTabButton with the appropriate icon and will be used to anchor the
// Tab Search bubble.
//
// TODO(tluk): Break away common code from the NewTabButton and the
// TabSearchButton into a TabStripControlButton or similar.
class TabSearchButton : public NewTabButton {
 public:
  METADATA_HEADER(TabSearchButton);
  explicit TabSearchButton(TabStrip* tab_strip);
  TabSearchButton(const TabSearchButton&) = delete;
  TabSearchButton& operator=(const TabSearchButton&) = delete;
  ~TabSearchButton() override;

  TabSearchBubbleHost* tab_search_bubble_host() {
    return tab_search_bubble_host_.get();
  }

  // NewTabButton:
  void NotifyClick(const ui::Event& event) final;
  void FrameColorsChanged() override;

 protected:
  // NewTabButton:
  void PaintIcon(gfx::Canvas* canvas) override;

 private:
  std::unique_ptr<TabSearchBubbleHost> tab_search_bubble_host_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_SEARCH_BUTTON_H_
