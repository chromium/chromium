// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_SHARED_ROUNDED_SCROLL_BAR_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_SHARED_ROUNDED_SCROLL_BAR_H_

#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/scrollbar/base_scroll_bar_thumb.h"
#include "ui/views/controls/scrollbar/scroll_bar.h"

namespace tabs {

// A transparent vertical scrollbar which overlays its contents and fades in on
// hover. It uses a rounded thumb that fades out when not in use.
class RoundedScrollBar : public views::ScrollBar {
  METADATA_HEADER(RoundedScrollBar, views::ScrollBar)

 public:
  RoundedScrollBar();

  RoundedScrollBar(const RoundedScrollBar&) = delete;
  RoundedScrollBar& operator=(const RoundedScrollBar&) = delete;

  ~RoundedScrollBar() override;

  void SetIsAnimatingSize(bool is_animating);

  // ScrollBar:
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  bool OverlapsContent() const override;
  gfx::Rect GetTrackBounds() const override;
  int GetThickness() const override;

 protected:
  class Thumb : public views::BaseScrollBarThumb {
    METADATA_HEADER(Thumb, views::BaseScrollBarThumb)

   public:
    explicit Thumb(RoundedScrollBar* scroll_bar);

    Thumb(const Thumb&) = delete;
    Thumb& operator=(const Thumb&) = delete;

    ~Thumb() override;

    void Init();

    void Show();
    void Hide();
    // Starts a countdown that hides the thumb when it fires.
    void StartHideCountdown();

    void set_is_animating_size(bool is_animating) {
      is_animating_size_ = is_animating;
    }

   protected:
    // BaseScrollBarThumb:
    gfx::Size CalculatePreferredSize(
        const views::SizeBounds& /*available_size*/) const override;
    void OnPaint(gfx::Canvas* canvas) override;
    void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

   private:
    base::OneShotTimer hide_timer_;
    raw_ptr<RoundedScrollBar> scroll_bar_ = nullptr;
    bool is_animating_size_ = false;
  };

  virtual bool ShouldHaveRightMargin() const;

 private:
  friend class Thumb;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_SHARED_ROUNDED_SCROLL_BAR_H_
