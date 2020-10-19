// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SHARESHEET_SHARESHEET_TARGET_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_SHARESHEET_SHARESHEET_TARGET_BUTTON_H_

#include "chrome/browser/sharesheet/sharesheet_types.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"

class SharesheetTargetButton : public views::Button {
 public:
  SharesheetTargetButton(views::ButtonListener* listener,
                         const base::string16& display_name,
                         const base::string16& secondary_display_name,
                         const gfx::ImageSkia* icon);
  SharesheetTargetButton(const SharesheetTargetButton&) = delete;
  SharesheetTargetButton& operator=(const SharesheetTargetButton&) = delete;

 private:
  void SetLabelProperties(views::Label* label);

  // views::View overrides
  gfx::Size CalculatePreferredSize() const override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_SHARESHEET_SHARESHEET_TARGET_BUTTON_H_
