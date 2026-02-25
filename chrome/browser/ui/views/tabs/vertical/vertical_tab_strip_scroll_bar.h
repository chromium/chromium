// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_STRIP_SCROLL_BAR_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_STRIP_SCROLL_BAR_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "ui/views/controls/scrollbar/base_scroll_bar_thumb.h"
#include "ui/views/controls/scrollbar/scroll_bar.h"

namespace tabs {
class VerticalTabStripStateController;
}

// The transparent vertical scrollbar which overlays its contents and expands on
// hover. Used for the pinned and unpinned tab containers in the vertical tab
// strip.
class VerticalTabStripScrollBar : public views::ScrollBar {
  METADATA_HEADER(VerticalTabStripScrollBar, ScrollBar)

 public:
  explicit VerticalTabStripScrollBar(
      tabs::VerticalTabStripStateController* state_controller);

  VerticalTabStripScrollBar(const VerticalTabStripScrollBar&) = delete;
  VerticalTabStripScrollBar& operator=(const VerticalTabStripScrollBar&) =
      delete;

  ~VerticalTabStripScrollBar() override;

  // ScrollBar:
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  bool OverlapsContent() const override;
  gfx::Rect GetTrackBounds() const override;
  int GetThickness() const override;

 private:
  class Thumb : public views::BaseScrollBarThumb {
    METADATA_HEADER(Thumb, BaseScrollBarThumb)

   public:
    explicit Thumb(VerticalTabStripScrollBar* scroll_bar);

    Thumb(const Thumb&) = delete;
    Thumb& operator=(const Thumb&) = delete;

    ~Thumb() override;

    void Init();

    void Show();
    void Hide();
    // Starts a countdown that hides the thumb when it fires.
    void StartHideCountdown();

   protected:
    // BaseScrollBarThumb:
    gfx::Size CalculatePreferredSize(
        const views::SizeBounds& /*available_size*/) const override;
    void OnPaint(gfx::Canvas* canvas) override;
    void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

   private:
    base::OneShotTimer hide_timer_;
    raw_ptr<VerticalTabStripScrollBar> scroll_bar_ = nullptr;
  };
  friend class Thumb;

  void OnCollapsedStateChanged(
      tabs::VerticalTabStripStateController* state_controller);

  bool tab_strip_collapsed_ = false;
  base::CallbackListSubscription collapsed_state_changed_subscription_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_STRIP_SCROLL_BAR_H_
