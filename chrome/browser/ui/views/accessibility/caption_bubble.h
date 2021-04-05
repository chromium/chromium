// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ACCESSIBILITY_CAPTION_BUBBLE_H_
#define CHROME_BROWSER_UI_VIEWS_ACCESSIBILITY_CAPTION_BUBBLE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_helpers.h"
#include "chrome/browser/ui/views/accessibility/caption_bubble_model.h"
#include "ui/native_theme/caption_style.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/metadata/view_factory.h"

namespace base {
class RetainingOneShotTimer;
}

namespace views {
class Label;
class ImageButton;
class ImageView;
}

namespace ui {
struct AXNodeData;
}

class BrowserView;

namespace captions {
class CaptionBubbleFrameView;
class CaptionBubbleLabel;

///////////////////////////////////////////////////////////////////////////////
// Caption Bubble
//
//  A caption bubble that floats above the BrowserView and shows automatically-
//  generated text captions for audio and media streams from the current tab.
//
class CaptionBubble : public views::BubbleDialogDelegateView {
 public:
  METADATA_HEADER(CaptionBubble);
  CaptionBubble(views::View* anchor,
                BrowserView* browser_view,
                base::OnceClosure destroyed_callback);
  CaptionBubble(const CaptionBubble&) = delete;
  CaptionBubble& operator=(const CaptionBubble&) = delete;
  ~CaptionBubble() override;

  // Sets the caption bubble model currently being used for this caption bubble.
  // There exists one CaptionBubble per browser, but one CaptionBubbleModel
  // per tab. A new CaptionBubbleModel is set when the active tab changes. A
  // CaptionBubbleModel is owned by the CaptionBubbleControllerViews. It is
  // created when a tab activates and exists for the lifetime of that tab.
  void SetModel(CaptionBubbleModel* model);

  // Changes the caption style of the caption bubble.
  void UpdateCaptionStyle(base::Optional<ui::CaptionStyle> caption_style);

  // Returns whether the bubble has activity, with the above definition of
  // activity.
  bool HasActivity();

  views::Label* GetLabelForTesting();
  base::RetainingOneShotTimer* GetInactivityTimerForTesting();
  void set_tick_clock_for_testing(const base::TickClock* tick_clock) {
    tick_clock_ = tick_clock;
  }

 protected:
  // views::BubbleDialogDelegateView:
  void Init() override;
  bool ShouldShowCloseButton() const override;
  std::unique_ptr<views::NonClientFrameView> CreateNonClientFrameView(
      views::Widget* widget) override;
  gfx::Rect GetBubbleBounds() override;
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override;
  void OnWidgetVisibilityChanged(views::Widget* widget, bool visible) override;
  void OnKeyEvent(ui::KeyEvent* event) override;
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  void OnFocus() override;
  void OnBlur() override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  std::u16string GetAccessibleWindowTitle() const override;
  void AddedToWidget() override;

 private:
  friend class CaptionBubbleControllerViewsTest;
  friend class CaptionBubbleModel;

  void CloseButtonPressed();
  void ExpandOrCollapseButtonPressed();

  // Called by CaptionBubbleModel to notify this object that the model's text
  // has changed. Sets the text of the caption bubble to the model's text.
  void OnTextChanged();

  // Called by CaptionBubbleModel to notify this object that the model's error
  // state has changed. Makes the caption bubble display an error message if
  // the model has an error, otherwise displays the latest text.
  void OnErrorChanged();

  // Called when the caption bubble expanded state has changed. Changes the
  // number of lines displayed.
  void OnIsExpandedChanged();

  // The caption bubble manages its own visibility based on whether there's
  // space for it to be shown, and if it has an error or text to display.
  void UpdateBubbleVisibility();
  void UpdateBubbleAndTitleVisibility();

  // For the provided line index, gets the corresponding rendered line in the
  // label and returns the text position of the first character of that line.
  // Returns the same value regardless of whether the label is visible or not.
  // TODO(crbug.com/1055150): This feature is launching for English first.
  // Make sure this is correct for all languages.
  size_t GetTextIndexOfLineInLabel(size_t line) const;

  // Returns the number of lines in the caption bubble label that are rendered.
  size_t GetNumLinesInLabel() const;
  int GetNumLinesVisible();
  void UpdateContentSize();
  void Redraw();
  void Hide();

  // The following methods set the caption bubble style based on the user's
  // preferences, which are stored in `caption_style_`.
  void SetCaptionBubbleStyle();
  double GetTextScaleFactor();
  void SetTextSizeAndFontFamily();
  void SetTextColor();
  void SetBackgroundColor();

  // After 5 seconds of inactivity, hide the caption bubble. Activity is defined
  // as transcription received from the speech service or user interacting with
  // the bubble through focus, pressing buttons, or dragging.
  void OnInactivityTimeout();

  // Unowned. Owned by views hierarchy.
  CaptionBubbleLabel* label_;
  views::Label* title_;
  views::Label* error_text_;
  views::ImageView* error_icon_;
  views::View* error_message_;
  views::ImageButton* close_button_;
  views::ImageButton* expand_button_;
  views::ImageButton* collapse_button_;
  CaptionBubbleFrameView* frame_;
  views::View* content_container_;

  base::Optional<ui::CaptionStyle> caption_style_;
  CaptionBubbleModel* model_ = nullptr;

  base::ScopedClosureRunner destroyed_callback_;

  // The bubble tries to stay relatively positioned in its parent.
  // ratio_in_parent_x_ represents the ratio along the parent width at which
  // to display the center of the bubble, if possible.
  double ratio_in_parent_x_;
  double ratio_in_parent_y_;
  gfx::Rect latest_bounds_;
  gfx::Rect latest_anchor_bounds_;

  // Whether there's space for the widget to layout within its parent window.
  bool can_layout_ = true;

  // A reference to the BrowserView holding this bubble. Unowned.
  BrowserView* browser_view_;

  // Whether the caption bubble is expanded to show more lines of text.
  bool is_expanded_ = false;

  // A timer which causes the bubble to hide if there is no activity after a
  // specified interval.
  std::unique_ptr<base::RetainingOneShotTimer> inactivity_timer_;
  const base::TickClock* tick_clock_;
};

BEGIN_VIEW_BUILDER(/* no export */,
                   CaptionBubble,
                   views::BubbleDialogDelegateView)
END_VIEW_BUILDER

}  // namespace captions

DEFINE_VIEW_BUILDER(/* no export */, captions::CaptionBubble)

#endif  // CHROME_BROWSER_UI_VIEWS_ACCESSIBILITY_CAPTION_BUBBLE_H_
