// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_GLIC_GLIC_BASE_SHIM_H_
#define CHROME_BROWSER_UI_VIEWS_GLIC_GLIC_BASE_SHIM_H_

#include "chrome/browser/ui/color/chrome_color_id.h"
#include "ui/color/color_id.h"
#include "ui/views/view.h"

// GlicBaseShim is used by Glic views that are required to be multiple types of
// buttons (e.g. TabStripNudgeButton and ToolbarButton). GlicBaseShim provides
// an interface that allows for certain methods either be implemented by T, or
// overridden by the subclass. These methods are often required for unified GLic
// functionality, but are not necessary for all T views.
template <typename T>
class GlicBaseShim : public T {
 public:
  using T::T;

  virtual void UpdateColors() {
    if constexpr (requires { this->T::UpdateColors(); }) {
      this->T::UpdateColors();
    }
  }

  virtual void SetCloseButtonFocusBehavior(
      views::View::FocusBehavior focus_behavior) {
    if constexpr (requires {
                    this->T::SetCloseButtonFocusBehavior(focus_behavior);
                  }) {
      T::SetCloseButtonFocusBehavior(focus_behavior);
    }
  }

  virtual void SetForegroundFrameActiveColorId(ui::ColorId new_color_id) {
    if constexpr (requires {
                    this->T::SetForegroundFrameActiveColorId(new_color_id);
                  }) {
      T::SetForegroundFrameActiveColorId(new_color_id);
    }
  }

  virtual void SetForegroundFrameInactiveColorId(ui::ColorId new_color_id) {
    if constexpr (requires {
                    this->T::SetForegroundFrameInactiveColorId(new_color_id);
                  }) {
      T::SetForegroundFrameInactiveColorId(new_color_id);
    }
  }

  virtual void SetBackgroundFrameActiveColorId(ui::ColorId new_color_id) {
    if constexpr (requires {
                    this->T::SetBackgroundFrameActiveColorId(new_color_id);
                  }) {
      T::SetBackgroundFrameActiveColorId(new_color_id);
    }
  }

  virtual void SetBackgroundFrameInactiveColorId(ui::ColorId new_color_id) {
    if constexpr (requires {
                    this->T::SetBackgroundFrameInactiveColorId(new_color_id);
                  }) {
      T::SetBackgroundFrameInactiveColorId(new_color_id);
    }
  }

  virtual void SetLeftRightCornerRadii(int left, int right) {
    if constexpr (requires { this->T::SetLeftRightCornerRadii(left, right); }) {
      T::SetLeftRightCornerRadii(left, right);
    }
  }

  virtual void SetInkdropHoverColorId(const ChromeColorIds new_color_id) {
    if constexpr (requires { this->T::SetInkdropHoverColorId(new_color_id); }) {
      T::SetInkdropHoverColorId(new_color_id);
    }
  }

  virtual void SetIsShowingNudge(bool is_showing) {
    if constexpr (requires { this->T::SetIsShowingNudge(is_showing); }) {
      T::SetIsShowingNudge(is_showing);
    }
  }
};

#endif  // CHROME_BROWSER_UI_VIEWS_GLIC_GLIC_BASE_SHIM_H_
