// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_STRIP_SCROLL_BAR_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_STRIP_SCROLL_BAR_H_

#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "ui/views/controls/scrollbar/base_scroll_bar_thumb.h"
#include "ui/views/controls/scrollbar/scroll_bar.h"

// The transparent vertical scrollbar which overlays its contents and expands on
// hover. Used for the pinned and unpinned tab containers in the vertical tab
// strip.
class VerticalTabStripScrollBar : public views::ScrollBar {
  METADATA_HEADER(VerticalTabStripScrollBar, ScrollBar)

 public:
  VerticalTabStripScrollBar();

  VerticalTabStripScrollBar(const VerticalTabStripScrollBar&) = delete;
  VerticalTabStripScrollBar& operator=(const VerticalTabStripScrollBar&) =
      delete;

  ~VerticalTabStripScrollBar() override;

  // ScrollBar:
  gfx::Insets GetInsets() const override;
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

    // Shows this (effectively, the thumb) without delay.
    void Show();
    // Hides this with a delay.
    void Hide();
    // Starts a countdown that hides this when it fires.
    void StartHideCountdown();

   protected:
    // BaseScrollBarThumb:
    gfx::Size CalculatePreferredSize(
        const views::SizeBounds& /*available_size*/) const override;
    void OnPaint(gfx::Canvas* canvas) override;
    void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
    void OnStateChanged() override;

   private:
    base::OneShotTimer hide_timer_;
  };
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_STRIP_SCROLL_BAR_H_
