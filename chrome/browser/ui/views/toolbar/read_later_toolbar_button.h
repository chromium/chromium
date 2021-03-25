// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_READ_LATER_TOOLBAR_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_READ_LATER_TOOLBAR_BUTTON_H_

#include "chrome/browser/ui/views/bubble/bubble_contents_wrapper.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/browser/ui/webui/read_later/read_later_ui.h"

class Browser;

// TODO(pbos): Make this a ReadingListModelObserver, observe changes and add a
// notification dot. See ReadLaterButton.
class ReadLaterToolbarButton : public ToolbarButton {
 public:
  explicit ReadLaterToolbarButton(Browser* browser);
  ReadLaterToolbarButton(const ReadLaterToolbarButton&) = delete;
  ReadLaterToolbarButton& operator=(const ReadLaterToolbarButton&) = delete;
  ~ReadLaterToolbarButton() override;

 private:
  void ButtonPressed();

  Browser* const browser_;

  views::View* side_panel_webview_ = nullptr;
  std::unique_ptr<BubbleContentsWrapperT<ReadLaterUI>> contents_wrapper_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_READ_LATER_TOOLBAR_BUTTON_H_
