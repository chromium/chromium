// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_TAB_SEARCH_FRAME_CAPTION_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_TAB_SEARCH_FRAME_CAPTION_BUTTON_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/window/frame_caption_button.h"

class Browser;
class Profile;
class TabSearchBubbleHost;

class TabSearchFrameCaptionButton : public views::FrameCaptionButton {
 public:
  METADATA_HEADER(TabSearchFrameCaptionButton);
  explicit TabSearchFrameCaptionButton(Profile* profile);
  TabSearchFrameCaptionButton(const TabSearchFrameCaptionButton&) = delete;
  TabSearchFrameCaptionButton& operator=(const TabSearchFrameCaptionButton&) =
      delete;
  ~TabSearchFrameCaptionButton() override;

  static bool IsTabSearchCaptionButtonEnabled(Browser* browser);

  // views::FrameCaptionButton:
  gfx::Rect GetAnchorBoundsInScreen() const override;

  TabSearchBubbleHost* tab_search_bubble_host() {
    return tab_search_bubble_host_.get();
  }

 private:
  std::unique_ptr<TabSearchBubbleHost> tab_search_bubble_host_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_TAB_SEARCH_FRAME_CAPTION_BUTTON_H_
