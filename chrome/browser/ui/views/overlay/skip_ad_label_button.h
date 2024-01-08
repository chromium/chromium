// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OVERLAY_SKIP_AD_LABEL_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_OVERLAY_SKIP_AD_LABEL_BUTTON_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/label_button.h"

// A label button representing a skip-ad button.
class SkipAdLabelButton : public views::LabelButton {
  METADATA_HEADER(SkipAdLabelButton, views::LabelButton)

 public:
  explicit SkipAdLabelButton(PressedCallback callback);
  SkipAdLabelButton(const SkipAdLabelButton&) = delete;
  SkipAdLabelButton& operator=(const SkipAdLabelButton&) = delete;
  ~SkipAdLabelButton() override = default;

  // Sets the position of itself with an offset from the given window size.
  void SetPosition(const gfx::Size& size);

  // Overridden from views::View.
  void SetVisible(bool is_visible) override;
  void OnThemeChanged() override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_OVERLAY_SKIP_AD_LABEL_BUTTON_H_
