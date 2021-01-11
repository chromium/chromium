// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_CHROME_LABS_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_CHROME_LABS_BUTTON_H_

#include "chrome/browser/ui/views/toolbar/toolbar_button.h"

class ChromeLabsButton : public ToolbarButton {
 public:
  ChromeLabsButton();

  // ToolbarButton:
  void UpdateIcon() override;

  // views::View:
  const char* GetClassName() const override;

 private:
  void ButtonPressed();
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_CHROME_LABS_BUTTON_H_
