// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_BACK_FORWARD_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_BACK_FORWARD_BUTTON_H_

#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "ui/views/metadata/metadata_header_macros.h"

class Browser;

class BackForwardButton : public ToolbarButton {
 public:
  METADATA_HEADER(BackForwardButton);

  enum class Direction { kBack, kForward };

  BackForwardButton(Direction direction,
                    PressedCallback callback,
                    Browser* browser);
  BackForwardButton(const BackForwardButton&) = delete;
  BackForwardButton& operator=(const BackForwardButton&) = delete;
  ~BackForwardButton() override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_BACK_FORWARD_BUTTON_H_
