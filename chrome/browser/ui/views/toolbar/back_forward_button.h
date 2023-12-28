// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_BACK_FORWARD_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_BACK_FORWARD_BUTTON_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/window_open_disposition.h"

class Browser;

class BackForwardButton : public ToolbarButton {
  METADATA_HEADER(BackForwardButton, ToolbarButton)

 public:
  enum class Direction { kBack, kForward };

  BackForwardButton(Direction direction,
                    PressedCallback callback,
                    Browser* browser);
  BackForwardButton(const BackForwardButton&) = delete;
  BackForwardButton& operator=(const BackForwardButton&) = delete;
  ~BackForwardButton() override;

 private:
  // views::Button:
  void NotifyClick(const ui::Event& event) override;
  void StateChanged(ButtonState old_state) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  bool ShouldShowInkdropAfterIphInteraction() override;

  const std::u16string GetAccessiblePageLoadingMessage();

  const raw_ptr<Browser> browser_;
  const Direction direction_;

  // Reflects whether any modifiers are down, which would affect which tab would
  // be navigated, at the time of a mouse enter on the back button. This is only
  // for heuristic purposes. Only the modifiers of the actual click determine
  // which tab is navigated.
  WindowOpenDisposition last_back_assumed_disposition_ =
      WindowOpenDisposition::CURRENT_TAB;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_BACK_FORWARD_BUTTON_H_
