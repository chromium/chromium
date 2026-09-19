// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_UI_OWNER_DRAW_CONTROLS_TEST_API_H_
#define CHROME_UPDATER_WIN_UI_OWNER_DRAW_CONTROLS_TEST_API_H_

#include "base/memory/raw_ref.h"
#include "chrome/updater/win/ui/owner_draw_controls.h"

namespace updater::test {

// Grants tests access to a tracked button's internals without widening
// the production API surface, in the spirit of `views::test::ButtonTestApi`.
class TrackedButtonTestApi {
 public:
  explicit TrackedButtonTestApi(const ui::TrackedButton& button)
      : button_(button) {}
  TrackedButtonTestApi(const TrackedButtonTestApi&) = delete;
  TrackedButtonTestApi& operator=(const TrackedButtonTestApi&) = delete;

  bool is_mouse_hovering() const { return button_->is_mouse_hovering_; }
  bool is_tracking_mouse_events() const {
    return button_->is_tracking_mouse_events_;
  }

 private:
  const base::raw_ref<const ui::TrackedButton> button_;
};

// Grants tests access to a single caption button's internals without widening
// the production API surface, in the spirit of `views::test::ButtonTestApi`.
class CaptionButtonTestApi : public TrackedButtonTestApi {
 public:
  explicit CaptionButtonTestApi(const ui::CaptionButton& button)
      : TrackedButtonTestApi(button), button_(button) {}
  CaptionButtonTestApi(const CaptionButtonTestApi&) = delete;
  CaptionButtonTestApi& operator=(const CaptionButtonTestApi&) = delete;

  using PaintState = ::updater::ui::CaptionButton::PaintState;

  bool is_dark_mode() const { return button_->is_dark_mode_; }

  // The color this button would paint its glyph with right now.
  COLORREF ResolveGlyphColor() const {
    const UINT item_state = button_->IsEnabled() ? 0 : ODS_DISABLED;
    return ui::CaptionButton::ResolveGlyphColor(
        button_->SnapshotPaintState(item_state));
  }

  // The same resolution for a state the caller constructs, which is what makes
  // the combinations testable without a window.
  static COLORREF ResolveGlyphColor(PaintState paint_state) {
    return ui::CaptionButton::ResolveGlyphColor(paint_state);
  }

 private:
  const base::raw_ref<const ui::CaptionButton> button_;
};

// Grants tests access to the title bar's child controls.
class OwnerDrawTitleBarTestApi {
 public:
  explicit OwnerDrawTitleBarTestApi(ui::OwnerDrawTitleBar& title_bar)
      : title_bar_window_(title_bar.title_bar_window_) {}
  OwnerDrawTitleBarTestApi(const OwnerDrawTitleBarTestApi&) = delete;
  OwnerDrawTitleBarTestApi& operator=(const OwnerDrawTitleBarTestApi&) = delete;

  ui::OwnerDrawTitleBarWindow& title_bar_window() { return *title_bar_window_; }
  ui::CloseButton& close_button() { return title_bar_window_->close_button_; }
  ui::MinimizeButton& minimize_button() {
    return title_bar_window_->minimize_button_;
  }

 private:
  const base::raw_ref<ui::OwnerDrawTitleBarWindow> title_bar_window_;
};

}  // namespace updater::test

#endif  // CHROME_UPDATER_WIN_UI_OWNER_DRAW_CONTROLS_TEST_API_H_
