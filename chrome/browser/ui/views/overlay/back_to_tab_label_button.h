// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OVERLAY_BACK_TO_TAB_LABEL_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_OVERLAY_BACK_TO_TAB_LABEL_BUTTON_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/label_button.h"

// A button representing a back-to-tab button.
class BackToTabLabelButton : public views::LabelButton {
 public:
  METADATA_HEADER(BackToTabLabelButton);

  explicit BackToTabLabelButton(PressedCallback callback);
  BackToTabLabelButton(const BackToTabLabelButton&) = delete;
  BackToTabLabelButton& operator=(const BackToTabLabelButton&) = delete;
  ~BackToTabLabelButton() override;

  // views::View:
  ui::Cursor GetCursor(const ui::MouseEvent& event) override;
  void OnThemeChanged() override;

  // Updates the position of this button within the new bounds of the window.
  void SetWindowSize(const gfx::Size& window_size);

 private:
  void UpdateSizingAndPosition();

  absl::optional<gfx::Size> window_size_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_OVERLAY_BACK_TO_TAB_LABEL_BUTTON_H_
