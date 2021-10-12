// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_WINDOWS_10_TAB_SEARCH_CAPTION_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_WINDOWS_10_TAB_SEARCH_CAPTION_BUTTON_H_

#include "chrome/browser/ui/views/frame/windows_10_caption_button.h"
#include "ui/base/metadata/metadata_header_macros.h"

class GlassBrowserFrameView;
class TabSearchBubbleHost;

class Windows10TabSearchCaptionButton : public Windows10CaptionButton {
 public:
  METADATA_HEADER(Windows10TabSearchCaptionButton);
  Windows10TabSearchCaptionButton(GlassBrowserFrameView* frame_view,
                                  ViewID button_type,
                                  const std::u16string& accessible_name);
  Windows10TabSearchCaptionButton(const Windows10TabSearchCaptionButton&) =
      delete;
  Windows10TabSearchCaptionButton& operator=(
      const Windows10TabSearchCaptionButton&) = delete;
  ~Windows10TabSearchCaptionButton() override;

  TabSearchBubbleHost* tab_search_bubble_host() {
    return tab_search_bubble_host_.get();
  }

 private:
  std::unique_ptr<TabSearchBubbleHost> tab_search_bubble_host_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_WINDOWS_10_TAB_SEARCH_CAPTION_BUTTON_H_
