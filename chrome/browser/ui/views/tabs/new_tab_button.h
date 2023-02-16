// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_NEW_TAB_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_NEW_TAB_BUTTON_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "build/build_config.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "ui/base/metadata/metadata_header_macros.h"
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
  METADATA_HEADER(NewTabButton);

  static const gfx::Size kButtonSize;

  NewTabButton(TabStrip* tab_strip, PressedCallback callback);
  NewTabButton(const NewTabButton&) = delete;
  NewTabButton& operator=(const NewTabButton&) = delete;
  ~NewTabButton() override;

  // Called when the tab strip transitions to/from single tab mode, the frame
  // state changes or the accent color changes.  Updates the glyph colors for
  // the best contrast on the background.
  virtual void FrameColorsChanged();

  void AnimateToStateForTesting(views::InkDropState state);

  // views::ImageButton:
  void AddLayerToRegion(ui::Layer* new_layer,
                        views::LayerRegion region) override;
  void RemoveLayerFromRegions(ui::Layer* old_layer) override;

 protected:
  virtual void PaintIcon(gfx::Canvas* canvas);

  TabStrip* tab_strip() { return tab_strip_; }

  SkColor GetForegroundColor() const;

  // views::ImageButton:
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  void AddedToWidget() override;
  void RemovedFromWidget() override;
  void OnThemeChanged() override;

 private:
  class HighlightPathGenerator;

// views::ImageButton:
#if BUILDFLAG(IS_WIN)
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

  // Returns the path for the given |origin| and |scale|.  If |extend_to_top| is
  // true, the path is extended vertically to y = 0.
  SkPath GetBorderPath(const gfx::Point& origin,
                       float scale,
                       bool extend_to_top) const;

  // Tab strip that contains this button.
  raw_ptr<TabStrip, DanglingUntriaged> tab_strip_;

  // Contains our ink drop layer so it can paint above our background.
  raw_ptr<views::InkDropContainerView, DanglingUntriaged> ink_drop_container_;

  base::CallbackListSubscription paint_as_active_subscription_;

  // For tracking whether this object has been destroyed. Must be last.
  base::WeakPtrFactory<NewTabButton> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_NEW_TAB_BUTTON_H_
