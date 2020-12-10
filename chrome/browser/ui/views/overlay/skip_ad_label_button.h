// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OVERLAY_SKIP_AD_LABEL_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_OVERLAY_SKIP_AD_LABEL_BUTTON_H_

#include "chrome/browser/ui/views/overlay/overlay_window_views.h"
#include "ui/views/controls/button/label_button.h"

namespace views {

// A label button representing a skip-ad button.
class SkipAdLabelButton : public views::LabelButton {
 public:
  explicit SkipAdLabelButton(PressedCallback callback);

  // Sets the position of itself with an offset from the given window size.
  void SetPosition(const gfx::Size& size);

  // Overridden from views::View.
  void SetVisible(bool is_visible) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(SkipAdLabelButton);
};

}  // namespace views

#endif  // CHROME_BROWSER_UI_VIEWS_OVERLAY_SKIP_AD_LABEL_BUTTON_H_
