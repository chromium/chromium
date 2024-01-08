// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_SCROLLING_OVERFLOW_INDICATOR_STRATEGY_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_SCROLLING_OVERFLOW_INDICATOR_STRATEGY_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/ui_features.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/view.h"

/* Class for defining the different Overflow View Strategies, some do not have
 * a corresponding view class and require different methods for displaying than
 * scroll views default overflow indicator */
class TabStripScrollingOverflowIndicatorStrategy {
 public:
  TabStripScrollingOverflowIndicatorStrategy() = delete;
  explicit TabStripScrollingOverflowIndicatorStrategy(
      views::ScrollView* scroll_view,
      base::RepeatingCallback<SkColor4f()> get_frame_color,
      base::RepeatingCallback<SkColor4f()> get_shadow_color);
  virtual ~TabStripScrollingOverflowIndicatorStrategy();

  // Function to get the Featured Overflow Indicator Strategy
  static std::unique_ptr<TabStripScrollingOverflowIndicatorStrategy>
  CreateFromFeatureFlag(views::ScrollView* scroll_view,
                        base::RepeatingCallback<SkColor4f()> get_frame_color,
                        base::RepeatingCallback<SkColor4f()> get_shadow_color);

  // Performs the setup for the Strategy. Initialize any views for overflow to
  // the scroll view here.
  virtual void Init() = 0;

  // When frame colors are changed the overflow indicators need to react.
  virtual void FrameColorsChanged() {}

  // Accessors.
  views::ScrollView* scroll_view() const { return scroll_view_; }
  SkColor4f get_frame_color() const { return get_frame_color_.Run(); }
  SkColor4f get_shadow_color() const { return get_shadow_color_.Run(); }

 private:
  // The scroll view the indicators are attached to/owned by if they are views.
  const raw_ptr<views::ScrollView> scroll_view_;

  // A callback to get the current frame color.
  const base::RepeatingCallback<SkColor4f()> get_frame_color_;

  // A callback to get the shadow color.
  const base::RepeatingCallback<SkColor4f()> get_shadow_color_;

  // The callback used to update the inidicators from the scrollview.
  base::CallbackListSubscription on_contents_scrolled_subscription_;
};

class GradientIndicatorView : public views::View {
  METADATA_HEADER(GradientIndicatorView, views::View)

 public:
  explicit GradientIndicatorView(views::OverflowIndicatorAlignment side);
  GradientIndicatorView(views::OverflowIndicatorAlignment side,
                        int opaque_width,
                        int shadow_opaque_width,
                        int shadow_blur_width);
  // Making this smaller than the margin provided by the leftmost/rightmost
  // tab's tail (TabStyle::kTabOverlap / 2) makes the transition in and out of
  // the scroll state smoother.
  static constexpr int kDefaultOpaqueWidth = 8;

  // The width of the full opacity part of the shadow.
  static constexpr int kDefaultShadowSpread = 1;

  // The width of the soft edge of the shadow.
  static constexpr int kDefaultShadowBlur = 3;

  // views::View overrides:
  void OnPaint(gfx::Canvas* canvas) override;

  // Mutators for colors for the overflow.
  void SetShadowColor(SkColor4f new_shadow_color);
  void SetFrameColor(SkColor4f new_frame_color);

  // Mutators for widths for the overflow.
  void SetOpaqueWidth(int opaque_width) { opaque_width_ = opaque_width; }
  void SetShadowOpaqueWidth(int shadow_opaque_width) {
    shadow_opaque_width_ = shadow_opaque_width;
  }
  void SetShadowBlurWidth(int shadow_blur_width) {
    shadow_blur_width_ = shadow_blur_width;
  }

  // Accessor for the full width of the GradientView
  int GetTotalWidth() {
    return opaque_width_ + shadow_opaque_width_ + shadow_blur_width_;
  }

 private:
  // Which side of the scroll view the indicator is on.
  const views::OverflowIndicatorAlignment side_;

  // The color used for the shadow part of the view.
  SkColor4f shadow_color_;

  // The color used for the frame part of the view. The frame hides the folio
  // bottom of the tab on the sides.
  SkColor4f frame_color_;

  // Configuration parameters for painting the indicator.
  int opaque_width_;
  int shadow_opaque_width_;
  int shadow_blur_width_;
};

class GradientOverflowIndicatorStrategy
    : public TabStripScrollingOverflowIndicatorStrategy {
 public:
  GradientOverflowIndicatorStrategy(
      views::ScrollView* scroll_view,
      base::RepeatingCallback<SkColor4f()> get_frame_color,
      base::RepeatingCallback<SkColor4f()> get_shadow_color);
  ~GradientOverflowIndicatorStrategy() override = default;

  void Init() override;

  GradientIndicatorView* left_overflow_indicator() const {
    return left_overflow_indicator_;
  }

  GradientIndicatorView* right_overflow_indicator() const {
    return right_overflow_indicator_;
  }

 protected:
  // The views, owned by |scroll_view_|, that indicate that there are more
  // tabs overflowing to the left or right.
  raw_ptr<GradientIndicatorView> left_overflow_indicator_;
  raw_ptr<GradientIndicatorView> right_overflow_indicator_;
};

class ShadowOverflowIndicatorStrategy
    : public GradientOverflowIndicatorStrategy {
 public:
  ShadowOverflowIndicatorStrategy(
      views::ScrollView* scroll_view,
      base::RepeatingCallback<SkColor4f()> get_frame_color,
      base::RepeatingCallback<SkColor4f()> get_shadow_color);
  ~ShadowOverflowIndicatorStrategy() override = default;

  void FrameColorsChanged() override;
};

class FadeOverflowIndicatorStrategy : public GradientOverflowIndicatorStrategy {
 public:
  FadeOverflowIndicatorStrategy(
      views::ScrollView* scroll_view,
      base::RepeatingCallback<SkColor4f()> get_frame_color,
      base::RepeatingCallback<SkColor4f()> get_shadow_color);
  ~FadeOverflowIndicatorStrategy() override = default;

  void Init() override;
  void FrameColorsChanged() override;
};

class DividerOverflowIndicatorStrategy
    : public GradientOverflowIndicatorStrategy {
 public:
  DividerOverflowIndicatorStrategy(
      views::ScrollView* scroll_view,
      base::RepeatingCallback<SkColor4f()> get_frame_color,
      base::RepeatingCallback<SkColor4f()> get_shadow_color);
  ~DividerOverflowIndicatorStrategy() override = default;

  void Init() override;
  void FrameColorsChanged() override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_SCROLLING_OVERFLOW_INDICATOR_STRATEGY_H_
