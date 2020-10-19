// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SHARESHEET_SHARESHEET_EXPAND_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_SHARESHEET_SHARESHEET_EXPAND_BUTTON_H_

#include "chrome/browser/sharesheet/sharesheet_types.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"

class SharesheetExpandButton : public views::Button {
 public:
  explicit SharesheetExpandButton(views::ButtonListener* listener);
  SharesheetExpandButton(const SharesheetExpandButton&) = delete;
  SharesheetExpandButton& operator=(const SharesheetExpandButton&) = delete;

  void SetDefaultView();
  void SetExpandedView();

 private:
  // views::View overrides
  gfx::Size CalculatePreferredSize() const override;

  views::ImageView* icon_ = nullptr;
  views::Label* label_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_SHARESHEET_SHARESHEET_EXPAND_BUTTON_H_
