// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_WINDOWS_TAB_SEARCH_CAPTION_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_WINDOWS_TAB_SEARCH_CAPTION_BUTTON_H_

#include "chrome/browser/ui/views/frame/windows_caption_button.h"
#include "ui/base/metadata/metadata_header_macros.h"

class BrowserFrameViewWin;
class TabSearchBubbleHost;

class WindowsTabSearchCaptionButton : public WindowsCaptionButton {
  METADATA_HEADER(WindowsTabSearchCaptionButton, WindowsCaptionButton)

 public:
  WindowsTabSearchCaptionButton(BrowserFrameViewWin* frame_view,
                                ViewID button_type,
                                const std::u16string& accessible_name);
  WindowsTabSearchCaptionButton(const WindowsTabSearchCaptionButton&) = delete;
  WindowsTabSearchCaptionButton& operator=(
      const WindowsTabSearchCaptionButton&) = delete;
  ~WindowsTabSearchCaptionButton() override;

  TabSearchBubbleHost* tab_search_bubble_host() {
    return tab_search_bubble_host_.get();
  }

 private:
  std::unique_ptr<TabSearchBubbleHost> tab_search_bubble_host_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_WINDOWS_TAB_SEARCH_CAPTION_BUTTON_H_
