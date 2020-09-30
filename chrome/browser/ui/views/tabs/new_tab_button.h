// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_NEW_TAB_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_NEW_TAB_BUTTON_H_

#include "base/scoped_observer.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/view.h"

namespace views {
class InkDropContainerView;
}

///////////////////////////////////////////////////////////////////////////////
// NewTabButton
//
//  A subclass of ImageButton that hit-tests to the shape of the new tab button
//  and does custom drawing.
//
///////////////////////////////////////////////////////////////////////////////
class NewTabButton : public views::ImageButton,
                     public views::MaskedTargeterDelegate {
 public:
  static constexpr char kClassName[] = "NewTabButton";

  static const gfx::Size kButtonSize;

  NewTabButton(TabStrip* tab_strip, PressedCallback callback);
  NewTabButton(const NewTabButton&) = delete;
  NewTabButton& operator=(const NewTabButton&) = delete;
  ~NewTabButton() override;

  // Called when the tab strip transitions to/from single tab mode, the frame
  // state changes or the accent color changes.  Updates the glyph colors for
  // the best contrast on the background.
  virtual void FrameColorsChanged();

  void AnimateInkDropToStateForTesting(views::InkDropState state);

  // views::ImageButton:
  const char* GetClassName() const override;
  void AddLayerBeneathView(ui::Layer* new_layer) override;
  void RemoveLayerBeneathView(ui::Layer* old_layer) override;

 protected:
  virtual void PaintIcon(gfx::Canvas* canvas);

  TabStrip* tab_strip() { return tab_strip_; }

  SkColor GetForegroundColor() const;

  // views::ImageButton:
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

 private:
  class HighlightPathGenerator;

// views::ImageButton:
#if defined(OS_WIN)
  void OnMouseReleased(const ui::MouseEvent& event) override;
#endif
  void OnGestureEvent(ui::GestureEvent* event) override;
  void NotifyClick(const ui::Event& event) override;
  void PaintButtonContents(gfx::Canvas* canvas) override;
  gfx::Size CalculatePreferredSize() const override;

  // views::MaskedTargeterDelegate:
  bool GetHitTestMask(SkPath* mask) const override;

  // Returns the radius to use for the button corners.
  int GetCornerRadius() const;

  // Paints the fill region of the button into |canvas|.
  void PaintFill(gfx::Canvas* canvas) const;

  SkColor GetButtonFillColor() const;

  // Returns the path for the given |origin| and |scale|.  If |extend_to_top| is
  // true, the path is extended vertically to y = 0.
  SkPath GetBorderPath(const gfx::Point& origin,
                       float scale,
                       bool extend_to_top) const;

  void UpdateInkDropBaseColor();

  // Tab strip that contains this button.
  TabStrip* tab_strip_;

  // Contains our ink drop layer so it can paint above our background.
  views::InkDropContainerView* ink_drop_container_;

  // were we destroyed?
  bool* destroyed_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_NEW_TAB_BUTTON_H_
