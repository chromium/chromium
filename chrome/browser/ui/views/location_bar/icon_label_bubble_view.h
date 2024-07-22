// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_ICON_LABEL_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_ICON_LABEL_BUBBLE_VIEW_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/animation/ink_drop_host.h"
#include "ui/views/animation/ink_drop_observer.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget_observer.h"

namespace gfx {
class FontList;
}  // namespace gfx

namespace views {
class AXVirtualView;
}  // namespace views

// View used to draw a bubble, containing an icon and a label. We use this as a
// base for the classes that handle the location icon (including the EV bubble),
// tab-to-search UI, and content settings.
class IconLabelBubbleView : public views::InkDropObserver,
                            public views::LabelButton {
  METADATA_HEADER(IconLabelBubbleView, views::LabelButton)

 public:
  static constexpr int kTrailingPaddingPreMd = 2;

  // Determines when the icon label background should be visible.
  enum class BackgroundVisibility {
    kNever,
    kWithLabel,
    kAlways,
  };

  // TODO(tluk): These should be updated to return ColorIds instead of raw
  // SkColors.
  class Delegate {
   public:
    // Returns the foreground color of items around the IconLabelBubbleView,
    // e.g. nearby text items.  By default, the IconLabelBubbleView will use
    // this as its foreground color, separator, and ink drop base color.
    virtual SkColor GetIconLabelBubbleSurroundingForegroundColor() const = 0;

    // Returns the alpha to use when computing the color of the separator.
    virtual SkAlpha GetIconLabelBubbleSeparatorAlpha() const;

    // Returns the base color for ink drops.  If not overridden, this returns
    // GetIconLabelBubbleSurroundingForegroundColor().
    virtual SkColor GetIconLabelBubbleInkDropColor() const;

    // Returns the background color behind the IconLabelBubbleView.
    virtual SkColor GetIconLabelBubbleBackgroundColor() const = 0;
  };

  // A view that draws the separator.
  class SeparatorView : public views::View {
    METADATA_HEADER(SeparatorView, views::View)

   public:
    explicit SeparatorView(IconLabelBubbleView* owner);
    SeparatorView(const SeparatorView&) = delete;
    SeparatorView& operator=(const SeparatorView&) = delete;

    // views::View:
    void OnPaint(gfx::Canvas* canvas) override;
    void OnThemeChanged() override;

    // Updates the opacity based on the ink drop's state.
    void UpdateOpacity();

   private:
    // Weak.
    raw_ptr<IconLabelBubbleView> owner_;
  };

  IconLabelBubbleView(const gfx::FontList& font_list, Delegate* delegate);

  IconLabelBubbleView(const IconLabelBubbleView&) = delete;
  IconLabelBubbleView& operator=(const IconLabelBubbleView&) = delete;

  ~IconLabelBubbleView() override;

  // views::InkDropObserver:
  void InkDropAnimationStarted() override;
  void InkDropRippleAnimationEnded(views::InkDropState state) override;

  // views::LabelButton:
  void Layout(PassKey) override;

  // Returns true when the label should be visible.
  virtual bool ShouldShowLabel() const;

  void SetBackgroundVisibility(BackgroundVisibility background_visibility);

  void SetLabel(const std::u16string& label);
  void SetLabel(const std::u16string& label,
                const std::u16string& accessible_name);
  void SetFontList(const gfx::FontList& font_list);

  const views::View* GetImageContainerView() const {
    return image_container_view();
  }
  views::View* GetImageContainerView() { return image_container_view(); }

  // Exposed for testing.
  views::View* separator_view() const { return separator_view_; }

  // Exposed for testing.
  bool is_animating_label() const { return slide_animation_.is_animating(); }

  void set_next_element_interior_padding(int padding) {
    next_element_interior_padding_ = padding;
  }

  void set_grow_animation_starting_width_for_testing(int width) {
    grow_animation_starting_width_ = width;
  }

  // Reduces the slide duration to 1ms such that animation still follows
  // through in the code but is short enough that it is essentially skipped.
  void ReduceAnimationTimeForTesting();

  // Enables tests to reset slide animation to a state where the label is not
  // showing.
  void ResetSlideAnimationForTesting() { ResetSlideAnimation(false); }

 protected:
  static constexpr int kOpenTimeMS = 150;

  virtual SkColor GetBackgroundColor() const;

  // Gets the color for displaying text and/or icons.
  virtual SkColor GetForegroundColor() const;

  // Sets the label text and background colors.
  void UpdateLabelColors();

  // Update the icon label's background if necessary.
  virtual void UpdateBackground();

  // Returns true when the separator should be visible.
  virtual bool ShouldShowSeparator() const;

  // Returns true when the label should be shown on animation ended.
  virtual bool ShouldShowLabelAfterAnimation() const;

  // Gets the current width based on |slide_animation_| and given bounds.
  // Virtual for testing.
  virtual int GetWidthBetween(int min, int max) const;

  // Returns true when animation is in progress and is shrinking.
  // Virtual for testing.
  virtual bool IsShrinking() const;

  // Returns true if a bubble was shown.
  virtual bool ShowBubble(const ui::Event& event);

  // Returns true if the bubble anchored to the icon is shown. This is to
  // prevent the bubble from reshowing on a mouse release.
  virtual bool IsBubbleShowing() const;

  virtual void OnTouchUiChanged();

  // views::LabelButton:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnThemeChanged() override;
  bool IsTriggerableEvent(const ui::Event& event) override;
  bool ShouldUpdateInkDropOnClickCanceled() const override;
  void NotifyClick(const ui::Event& event) override;
  void OnFocus() override;
  void OnBlur() override;
  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationCanceled(const gfx::Animation* animation) override;

  const gfx::FontList& font_list() const { return label()->font_list(); }

  // To avoid clang warning about SetImageModel being overridden, tell compiler
  // to accept both SetImageModel functions.
  using LabelButton::SetImageModel;
  void SetImageModel(const ui::ImageModel& image);

  gfx::Size GetSizeForLabelWidth(int label_width) const;

  // Sets the border padding around this view.
  virtual void UpdateBorder();

  // Set up for icons that animate their labels in. Animating out is initiated
  // manually.
  void SetUpForAnimation();

  // Set up for icons that animate their labels in and then automatically out
  // after a period of time. The duration of the slide includes the just the
  // time when the label is fully expanded, it does not include the time to
  // animate in and out.
  void SetUpForInOutAnimation(
      base::TimeDelta duration = base::Milliseconds(1800));

  // Animates the view in and disables highlighting for hover and focus. If a
  // |string_id| is provided it also sets/changes the label to that string.
  // TODO(bruthig): See https://crbug.com/669253. Since the ink drop highlight
  // currently cannot handle host resizes, the highlight needs to be disabled
  // when the animation is running.
  void AnimateIn(std::optional<int> string_id);

  // Animates the view out.
  void AnimateOut();

  void PauseAnimation();
  void UnpauseAnimation();

  // Returns the current value of the slide animation
  double GetAnimationValue() const;

  // Sets the slide animation value without animating. |show| determines if
  // the animation is set to fully shown or fully hidden.
  void ResetSlideAnimation(bool show);

  // Spacing between the image and the label.
  int GetInternalSpacing() const;

  // Sets whether tonal colors are used for the background of the view when
  // expanded to show the label.
  void SetUseTonalColorsWhenExpanded(bool use_tonal_colors);

  // Subclasses that want extra spacing added to the internal spacing can
  // override this method. This may be used when we want to align the label text
  // to the suggestion text, like in the SelectedKeywordView.
  virtual int GetExtraInternalSpacing() const;

  // True if the icon color should match the label color specified by
  // GetForegroundColor().
  bool IconColorShouldMatchForeground() const;

  void SetCustomBackgroundColorId(const ui::ColorId color_id);
  void SetCustomForegroundColorId(const ui::ColorId color_id);

  // Slide animation for label.
  gfx::SlideAnimation slide_animation_{this};

 private:
  class HighlightPathGenerator;

  // Returns the width after the icon and before the separator. If the
  // separator is not shown, and ShouldShowExtraEndSpace() is false, this
  // returns 0.
  int GetWidthBetweenIconAndSeparator() const;

  // Padding after the separator. If this separator is shown, this includes the
  // separator width.
  int GetEndPaddingWithSeparator() const;

  // Disables highlights and calls Show on the slide animation, should not be
  // called directly, use AnimateIn() instead, which handles label visibility.
  void ShowAnimation();

  // Disables highlights and calls Hide on the slide animation, should not be
  // called directly, use AnimateOut() instead, which handles label visibility.
  void HideAnimation();

  // Gets the highlight path for ink drops and focus rings using the current
  // bounds and separator visibility.
  SkPath GetHighlightPath() const;

  // Returns true if the view is painted on a solid background, or if it is
  // intended to be transparent to the view over which it is painted. The view's
  // background and foreground color accessors will reflect this preference.
  bool PaintedOnSolidBackground() const;

  raw_ptr<Delegate, DanglingUntriaged> delegate_;

  // The contents of the bubble.
  raw_ptr<SeparatorView> separator_view_;

  // The padding of the element that will be displayed after |this|. This value
  // is relevant for calculating the amount of space to reserve after the
  // separator.
  int next_element_interior_padding_ = 0;

  // This is used to check if the bubble was showing in the last mouse press
  // event. If this is true then IsTriggerableEvent() will return false to
  // prevent the bubble from reshowing. This flag is necessary because the
  // bubble gets dismissed before the button handles the mouse release event.
  bool suppress_button_release_ = false;

  // Parameters for the slide animation.
  bool is_animation_paused_ = false;
  double pause_animation_state_ = 0.0;
  double open_state_fraction_ = 0.0;
  // This is used to offset the label animation by the current width (e.g. the
  // icon). Set before animation begins in AnimateIn().
  int grow_animation_starting_width_ = 0;

  // Controls when the icon label background should be visible.
  // TODO(tluk): Remove the kWithLabel opt-in after UX has conslusively decided
  // how icon labels should be painted when the label text is shown.
  BackgroundVisibility background_visibility_ = BackgroundVisibility::kNever;

  // Whether the tonal color should be used when the icon is expanded to show
  // the label.
  bool use_tonal_color_when_expanded_ = false;

  // Virtual view, used for announcing changes to the state of this view. A
  // virtual child of this view.
  raw_ptr<views::AXVirtualView> alert_virtual_view_;

  base::CallbackListSubscription subscription_ =
      ui::TouchUiController::Get()->RegisterCallback(
          base::BindRepeating(&IconLabelBubbleView::OnTouchUiChanged,
                              base::Unretained(this)));

  std::optional<ui::ColorId> background_color_id_;
  std::optional<ui::ColorId> foreground_color_id_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_ICON_LABEL_BUBBLE_VIEW_H_
