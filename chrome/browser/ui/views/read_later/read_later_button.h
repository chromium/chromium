// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_READ_LATER_READ_LATER_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_READ_LATER_READ_LATER_BUTTON_H_

#include "chrome/browser/ui/views/bubble/webui_bubble_manager.h"
#include "chrome/browser/ui/webui/read_later/read_later_ui.h"
#include "ui/views/controls/button/label_button.h"

class Browser;
class WebUIBubbleDialogView;

// Button in the bookmarks bar that provides access to the corresponding
// read later menu.
// TODO(corising): Handle the the async presentation of the UI bubble.
class ReadLaterButton : public views::LabelButton {
 public:
  explicit ReadLaterButton(Browser* browser);
  ReadLaterButton(const ReadLaterButton&) = delete;
  ReadLaterButton& operator=(const ReadLaterButton&) = delete;
  ~ReadLaterButton() override;

  const char* GetClassName() const override;

 private:
  // LabelButton:
  std::unique_ptr<views::InkDrop> CreateInkDrop() override;
  std::unique_ptr<views::InkDropHighlight> CreateInkDropHighlight()
      const override;
  SkColor GetInkDropBaseColor() const override;
  void OnThemeChanged() override;

  void ButtonPressed();

  Browser* const browser_;

  // TODO(pbos): Figure out a better way to handle this.
  WebUIBubbleDialogView* read_later_side_panel_bubble_ = nullptr;

  std::unique_ptr<WebUIBubbleManager<ReadLaterUI>> webui_bubble_manager_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_READ_LATER_READ_LATER_BUTTON_H_
