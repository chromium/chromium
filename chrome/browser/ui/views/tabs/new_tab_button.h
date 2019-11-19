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
#include "ui/views/widget/widget_observer.h"

class FeaturePromoBubbleView;

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
                     public views::MaskedTargeterDelegate,
                     public views::WidgetObserver {
 public:
  static constexpr char kClassName[] = "NewTabButton";

  static const gfx::Size kButtonSize;

  NewTabButton(TabStrip* tab_strip, views::ButtonListener* listener);
  ~NewTabButton() override;

  // Retrieves the last active BrowserView instance to display the NewTabPromo.
  static void ShowPromoForLastActiveBrowser();

  // Returns whether there was a bubble that was closed. A bubble closes only
  // when it exists.
  static void CloseBubbleForLastActiveBrowser();

  // Shows the NewTabPromo when the NewTabFeatureEngagementTracker calls for it.
  void ShowPromo();

  // Returns whether there was a bubble that was closed. A bubble closes only
  // when it exists.
  void CloseBubble();

  // Called when the tab strip transitions to/from single tab mode, the frame
  // state changes or the accent color changes.  Updates the glyph colors for
  // the best contrast on the background.
  void FrameColorsChanged();

  void AnimateInkDropToStateForTesting(views::InkDropState state);

  FeaturePromoBubbleView* new_tab_promo() { return new_tab_promo_; }

  // views::View:
  const char* GetClassName() const override;
  void AddLayerBeneathView(ui::Layer* new_layer) override;
  void RemoveLayerBeneathView(ui::Layer* old_layer) override;

 protected:
  // views::View:
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

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  // Returns the radius to use for the button corners.
  int GetCornerRadius() const;

  // Paints the fill region of the button into |canvas|.
  void PaintFill(gfx::Canvas* canvas) const;

  // Paints a properly sized plus (+) icon into the center of the button.
  void PaintPlusIcon(gfx::Canvas* canvas) const;

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

  // Promotional UI that appears next to the NewTabButton and encourages its
  // use. Owned by its NativeWidget.
  FeaturePromoBubbleView* new_tab_promo_ = nullptr;

  // were we destroyed?
  bool* destroyed_ = nullptr;

  // Observes the NewTabPromo's Widget.  Used to tell whether the promo is
  // open and get called back when it closes.
  ScopedObserver<views::Widget, WidgetObserver> new_tab_promo_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(NewTabButton);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_NEW_TAB_BUTTON_H_
