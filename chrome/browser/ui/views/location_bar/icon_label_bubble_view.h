// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_ICON_LABEL_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_ICON_LABEL_BUBBLE_VIEW_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/optional.h"
#include "base/scoped_observer.h"
#include "base/strings/string16.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/material_design/material_design_controller_observer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/animation/ink_drop_host_view.h"
#include "ui/views/animation/ink_drop_observer.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget_observer.h"

namespace gfx {
class FontList;
class ImageSkia;
}

namespace views {
class ImageView;
class InkDropContainerView;
}

// View used to draw a bubble, containing an icon and a label. We use this as a
// base for the classes that handle the location icon (including the EV bubble),
// tab-to-search UI, and content settings.
class IconLabelBubbleView : public views::InkDropObserver,
                            public views::Button,
                            public ui::MaterialDesignControllerObserver {
 public:
  static constexpr int kTrailingPaddingPreMd = 2;

  // A view that draws the separator.
  class SeparatorView : public views::View {
   public:
    explicit SeparatorView(IconLabelBubbleView* owner);

    // views::View:
    void OnPaint(gfx::Canvas* canvas) override;

    // Updates the opacity based on the ink drop's state.
    void UpdateOpacity();

    void set_disable_animation_for_test(bool disable_animation_for_test) {
      disable_animation_for_test_ = disable_animation_for_test;
    }

   private:
    // Weak.
    IconLabelBubbleView* owner_;

    bool disable_animation_for_test_ = false;

    DISALLOW_COPY_AND_ASSIGN(SeparatorView);
  };

  explicit IconLabelBubbleView(const gfx::FontList& font_list);
  ~IconLabelBubbleView() override;

  // views::InkDropObserver:
  void InkDropAnimationStarted() override;
  void InkDropRippleAnimationEnded(views::InkDropState state) override;

  // Returns true when the label should be visible.
  virtual bool ShouldShowLabel() const;

  void SetLabel(const base::string16& label);
  void SetImage(const gfx::ImageSkia& image);
  void SetFontList(const gfx::FontList& font_list);

  const views::ImageView* GetImageView() const { return image_; }
  views::ImageView* GetImageView() { return image_; }

  // Exposed for testing.
  SeparatorView* separator_view() const { return separator_view_; }

  void set_next_element_interior_padding(int padding) {
    next_element_interior_padding_ = padding;
  }

 protected:
  static constexpr int kOpenTimeMS = 150;

  views::ImageView* image() { return image_; }
  const views::ImageView* image() const { return image_; }
  views::Label* label() { return label_; }
  const views::Label* label() const { return label_; }
  const views::InkDropContainerView* ink_drop_container() const {
    return ink_drop_container_;
  }

  // Gets the color for displaying text.
  virtual SkColor GetTextColor() const = 0;

  // Returns true when the separator should be visible.
  virtual bool ShouldShowSeparator() const;

  // Returns true when additional padding equal to
  // GetWidthBetweenIconAndSeparator() should be added to the end of the view.
  // This is useful in the case where it's required to layout subsequent views
  // in the same position regardless of whether the separator is shown or not.
  virtual bool ShouldShowExtraEndSpace() const;

  // Returns a multiplier used to calculate the actual width of the view based
  // on its desired width.  This ranges from 0 for a zero-width view to 1 for a
  // full-width view and can be used to animate the width of the view.
  virtual double WidthMultiplier() const;

  // Returns true when animation is in progress and is shrinking.
  virtual bool IsShrinking() const;

  // Returns true if a bubble was shown.
  virtual bool ShowBubble(const ui::Event& event);

  // Returns true if the bubble anchored to the icon is shown. This is to
  // prevent the bubble from reshowing on a mouse release.
  virtual bool IsBubbleShowing() const;

  // Sets the border padding around this view.
  virtual void UpdateBorder();

  // views::Button:
  gfx::Size CalculatePreferredSize() const override;
  void Layout() override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void OnNativeThemeChanged(const ui::NativeTheme* native_theme) override;
  void AddInkDropLayer(ui::Layer* ink_drop_layer) override;
  void RemoveInkDropLayer(ui::Layer* ink_drop_layer) override;
  std::unique_ptr<views::InkDrop> CreateInkDrop() override;
  std::unique_ptr<views::InkDropRipple> CreateInkDropRipple() const override;
  std::unique_ptr<views::InkDropHighlight> CreateInkDropHighlight()
      const override;
  std::unique_ptr<views::InkDropMask> CreateInkDropMask() const override;
  SkColor GetInkDropBaseColor() const override = 0;
  bool IsTriggerableEvent(const ui::Event& event) override;
  bool ShouldUpdateInkDropOnClickCanceled() const override;
  void NotifyClick(const ui::Event& event) override;
  void OnFocus() override;
  void OnBlur() override;
  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationCanceled(const gfx::Animation* animation) override;

  // ui::MaterialDesignControllerObserver:
  void OnTouchUiChanged() override;

  const gfx::FontList& font_list() const { return label_->font_list(); }

  SkColor GetParentBackgroundColor() const;

  gfx::Size GetSizeForLabelWidth(int label_width) const;

  // Set up for icons that animate their labels in and then out.
  void SetUpForInOutAnimation();

  // Animates the view in and disables highlighting for hover and focus. If a
  // |string_id| is provided it also sets/changes the label to that string.
  // TODO(bruthig): See https://crbug.com/669253. Since the ink drop highlight
  // currently cannot handle host resizes, the highlight needs to be disabled
  // when the animation is running.
  void AnimateIn(base::Optional<int> string_id);

  // Animates the view out.
  void AnimateOut();

  void PauseAnimation();
  void UnpauseAnimation();

  // Returns the current value of the slide animation
  double GetAnimationValue() const;

  // Sets the slide animation value without animating. |show| determines if
  // the animation is set to fully shown or fully hidden.
  void ResetSlideAnimation(bool show);

  // Returns true iff the slide animation has started, has not ended and is
  // currently paused.
  bool is_animation_paused() const { return is_animation_paused_; }

  // Reduces the slide duration to 1ms such that animation still follows
  // through in the code but is short enough that it is essentially skipped.
  void ReduceAnimationTimeForTesting();

 private:
  // Spacing between the image and the label.
  int GetInternalSpacing() const;

  // Subclasses that want extra spacing added to the internal spacing can
  // override this method. This may be used when we want to align the label text
  // to the suggestion text, like in the SelectedKeywordView.
  virtual int GetExtraInternalSpacing() const;

  // Subclasses that want a different duration for the slide animation can
  // override this method.
  virtual int GetSlideDurationTime() const;

  // Returns the width after the icon and before the separator. If the
  // separator is not shown, and ShouldShowExtraEndSpace() is false, this
  // returns 0.
  int GetWidthBetweenIconAndSeparator() const;

  // Padding after the separator. If this separator is shown, this includes the
  // separator width.
  int GetEndPaddingWithSeparator() const;

  // The view has been activated by a user gesture such as spacebar.
  // Returns true if some handling was performed.
  bool OnActivate(const ui::Event& event);

  // views::View:
  const char* GetClassName() const override;

  // Disables highlights and calls Show on the slide animation, should not be
  // called directly, use AnimateIn() instead, which handles label visibility.
  void ShowAnimation();

  // Disables highlights and calls Hide on the slide animation, should not be
  // called directly, use AnimateOut() instead, which handles label visibility.
  void HideAnimation();

  // The contents of the bubble.
  views::ImageView* image_;
  views::Label* label_;
  views::InkDropContainerView* ink_drop_container_;
  SeparatorView* separator_view_;

  // The padding of the element that will be displayed after |this|. This value
  // is relevant for calculating the amount of space to reserve after the
  // separator.
  int next_element_interior_padding_ = 0;

  // This is used to check if the bubble was showing in the last mouse press
  // event. If this is true then IsTriggerableEvent() will return false to
  // prevent the bubble from reshowing. This flag is necessary because the
  // bubble gets dismissed before the button handles the mouse release event.
  bool suppress_button_release_ = false;

  // Slide animation for label.
  gfx::SlideAnimation slide_animation_{this};

  // Parameters for the slide animation.
  bool is_animation_paused_ = false;
  double pause_animation_state_ = 0.0;
  double open_state_fraction_ = 0.0;

  ScopedObserver<ui::MaterialDesignController,
                 ui::MaterialDesignControllerObserver>
      md_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(IconLabelBubbleView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_ICON_LABEL_BUBBLE_VIEW_H_
