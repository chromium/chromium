// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_CHROME_LABS_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_CHROME_LABS_BUTTON_H_

#include "chrome/browser/ui/views/toolbar/chrome_labs_bubble_view_model.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "ui/views/metadata/metadata_header_macros.h"

class Browser;

class ChromeLabsButton : public ToolbarButton {
 public:
  METADATA_HEADER(ChromeLabsButton);
  explicit ChromeLabsButton(Browser* browser,
                            const ChromeLabsBubbleViewModel* model);
  ChromeLabsButton(const ChromeLabsButton&) = delete;
  ChromeLabsButton& operator=(const ChromeLabsButton&) = delete;
  ~ChromeLabsButton() override;

  // ToolbarButton:
  void UpdateIcon() override;

  static bool ShouldShowButton(const ChromeLabsBubbleViewModel* model);

 private:
  void ButtonPressed();

  Browser* browser_;

  const ChromeLabsBubbleViewModel* model_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_CHROME_LABS_BUTTON_H_
