// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ACCESSIBILITY_CAPTION_BUBBLE_H_
#define CHROME_BROWSER_UI_VIEWS_ACCESSIBILITY_CAPTION_BUBBLE_H_

#include <memory>
#include <string>

#include "chrome/browser/ui/views/accessibility/caption_bubble_model.h"
#include "ui/native_theme/caption_style.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/button.h"

namespace gfx {
struct VectorIcon;
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

///////////////////////////////////////////////////////////////////////////////
// Caption Bubble
//
//  A caption bubble that floats above the BrowserView and shows automatically-
//  generated text captions for audio and media streams from the current tab.
//
class CaptionBubble : public views::BubbleDialogDelegateView,
                      public views::ButtonListener {
 public:
  CaptionBubble(views::View* anchor,
                BrowserView* browser_view,
                base::OnceClosure destroyed_callback);
  ~CaptionBubble() override;
  CaptionBubble(const CaptionBubble&) = delete;
  CaptionBubble& operator=(const CaptionBubble&) = delete;

  // Sets the caption bubble model currently being used for this caption bubble.
  // There exists one CaptionBubble per browser, but one CaptionBubbleModel
  // per tab. A new CaptionBubbleModel is set when the active tab changes. A
  // CaptionBubbleModel is owned by the CaptionBubbleControllerViews. It is
  // created when a tab activates and exists for the lifetime of that tab.
  void SetModel(CaptionBubbleModel* model);

  // Changes the caption style of the caption bubble. For now, this only sets
  // the caption text size.
  void UpdateCaptionStyle(base::Optional<ui::CaptionStyle> caption_style);

  // For the provided line index, gets the corresponding rendered line in the
  // label and returns the text position of the first character of that line.
  // Returns the same value regardless of whether the label is visible or not.
  // TODO(crbug.com/1055150): This feature is launching for English first.
  // Make sure this is correct for all languages.
  size_t GetTextIndexOfLineInLabel(size_t line) const;

  // Returns the number of lines in the caption bubble label that are rendered.
  size_t GetNumLinesInLabel() const;

  const char* GetClassName() const override;

  std::string GetLabelTextForTesting();

 protected:
  // views::BubbleDialogDelegateView:
  void Init() override;
  bool ShouldShowCloseButton() const override;
  std::unique_ptr<views::NonClientFrameView> CreateNonClientFrameView(
      views::Widget* widget) override;
  gfx::Rect GetBubbleBounds() override;
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override;
  void OnKeyEvent(ui::KeyEvent* event) override;
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  void OnFocus() override;
  void OnBlur() override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void AddedToWidget() override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

 private:
  friend class CaptionBubbleControllerViewsTest;
  friend class CaptionBubbleModel;

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

  double GetTextScaleFactor();
  int GetNumLinesVisible();
  void UpdateTextSize();
  void UpdateContentSize();
  void Redraw();
  std::unique_ptr<views::ImageButton> BuildImageButton(
      const gfx::VectorIcon& icon,
      const int tooltip_text_id);

  // Unowned. Owned by views hierarchy.
  views::Label* label_;
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
};

}  // namespace captions

#endif  // CHROME_BROWSER_UI_VIEWS_ACCESSIBILITY_CAPTION_BUBBLE_H_
