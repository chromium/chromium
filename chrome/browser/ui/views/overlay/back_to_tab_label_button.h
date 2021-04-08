// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OVERLAY_BACK_TO_TAB_LABEL_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_OVERLAY_BACK_TO_TAB_LABEL_BUTTON_H_

#include "base/optional.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/metadata/metadata_header_macros.h"

// A button representing a back-to-tab button.
class BackToTabLabelButton : public views::LabelButton {
 public:
  METADATA_HEADER(BackToTabLabelButton);

  explicit BackToTabLabelButton(PressedCallback callback);
  BackToTabLabelButton(const BackToTabLabelButton&) = delete;
  BackToTabLabelButton& operator=(const BackToTabLabelButton&) = delete;
  ~BackToTabLabelButton() override;

  // views::LabelButton:
  void SetText(const std::u16string& text) override;

  // Updates the position of this button within the new bounds of the window.
  void SetWindowSize(const gfx::Size& window_size);

  // Returns true if the underlying label has elided text.
  bool IsTextElidedForTesting();

 private:
  void UpdateSizingAndPosition();

  base::Optional<gfx::Size> window_size_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_OVERLAY_BACK_TO_TAB_LABEL_BUTTON_H_
