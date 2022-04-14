// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LIVE_CAPTION_VIEWS_CAPTION_BUBBLE_H_
#define COMPONENTS_LIVE_CAPTION_VIEWS_CAPTION_BUBBLE_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "build/buildflag.h"
#include "components/live_caption/views/caption_bubble_model.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/font_list.h"
#include "ui/native_theme/caption_style.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/metadata/view_factory.h"

namespace base {
class RetainingOneShotTimer;
class TickClock;
}

namespace views {
class Checkbox;
class ImageButton;
class ImageView;
class Label;
}  // namespace views

namespace ui {
struct AXNodeData;
}

namespace captions {
class CaptionBubbleFrameView;
class CaptionBubbleLabel;

///////////////////////////////////////////////////////////////////////////////
// Caption Bubble
//
//  A caption bubble that floats above all other windows and shows
//  automatically- generated text captions for audio and media streams. The
//  captions bubble's widget is a top-level window that has top z order and is
//  visible on all workspaces. It is draggable in and out of the tab.
//
class CaptionBubble : public views::BubbleDialogDelegateView {
 public:
  METADATA_HEADER(CaptionBubble);
  CaptionBubble(base::OnceClosure destroyed_callback, bool hide_on_inactivity);
  CaptionBubble(const CaptionBubble&) = delete;
  CaptionBubble& operator=(const CaptionBubble&) = delete;
  ~CaptionBubble() override;

  // Sets the caption bubble model currently being used for this caption bubble.
  // There exists one CaptionBubble per profile, but one CaptionBubbleModel per
  // media stream. A new CaptionBubbleModel is set when transcriptions from a
  // different media stream are received. A CaptionBubbleModel is owned by the
  // CaptionBubbleControllerViews. It is created when transcriptions from a new
  // media stream are received and exists until the audio stream ends for that
  // stream.
  void SetModel(CaptionBubbleModel* model);

  // Changes the caption style of the caption bubble.
  void UpdateCaptionStyle(absl::optional<ui::CaptionStyle> caption_style);

  // Returns whether the bubble has activity. Activity is defined as
  // transcription received from the speech service or user interacting with the
  // bubble through focus, pressing buttons, or dragging.
  bool HasActivity();

  views::Label* GetLabelForTesting();
  base::RetainingOneShotTimer* GetInactivityTimerForTesting();
  void set_tick_clock_for_testing(const base::TickClock* tick_clock) {
    tick_clock_ = tick_clock;
  }

#if BUILDFLAG(IS_WIN)
  void OnContentSettingsLinkClicked();
#endif

 protected:
  // views::BubbleDialogDelegateView:
  void Init() override;
  void OnBeforeBubbleWidgetInit(views::Widget::InitParams* params,
                                views::Widget* widget) const override;
  bool ShouldShowCloseButton() const override;
  std::unique_ptr<views::NonClientFrameView> CreateNonClientFrameView(
      views::Widget* widget) override;
  gfx::Rect GetBubbleBounds() override;
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override;
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  std::u16string GetAccessibleWindowTitle() const override;
  void OnThemeChanged() override;

 private:
  friend class CaptionBubbleControllerViewsTest;
  friend class CaptionBubbleModel;

  void BackToTabButtonPressed();
  void CloseButtonPressed();
  void ExpandOrCollapseButtonPressed();

  // Called by CaptionBubbleModel to notify this object that the model's text
  // has changed. Sets the text of the caption bubble to the model's text.
  void OnTextChanged();

  // Called by CaptionBubbleModel to notify this object that the model's error
  // state has changed. Makes the caption bubble display an error message if
  // the model has an error, otherwise displays the latest text.
  void OnErrorChanged(CaptionBubbleErrorType error_type,
                      OnErrorClickedCallback callback,
                      OnDoNotShowAgainClickedCallback error_silenced_callback);

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
  void ShowInactive();
  void Hide();

  // The following methods set the caption bubble style based on the user's
  // preferences, which are stored in `caption_style_`.
  void SetCaptionBubbleStyle();
  double GetTextScaleFactor();
  const gfx::FontList GetFontList();
  void SetTextSizeAndFontFamily();
  void SetTextColor();
  void SetBackgroundColor();

  // After 5 seconds of inactivity, hide the caption bubble. Activity is defined
  // as transcription received from the speech service or user interacting with
  // the bubble through focus, pressing buttons, or dragging.
  void OnInactivityTimeout();

  void MediaFoundationErrorCheckboxPressed();

  // Unowned. Owned by views hierarchy.
  raw_ptr<CaptionBubbleLabel> label_;
  raw_ptr<views::Label> title_;
  raw_ptr<views::Label> generic_error_text_;
  raw_ptr<views::ImageView> generic_error_icon_;
  raw_ptr<views::View> generic_error_message_;
  raw_ptr<views::ImageButton> back_to_tab_button_;
  raw_ptr<views::ImageButton> close_button_;
  raw_ptr<views::ImageButton> expand_button_;
  raw_ptr<views::ImageButton> collapse_button_;
  raw_ptr<CaptionBubbleFrameView> frame_;

#if BUILDFLAG(IS_WIN)
  raw_ptr<views::StyledLabel> media_foundation_renderer_error_text_;
  raw_ptr<views::ImageView> media_foundation_renderer_error_icon_;
  raw_ptr<views::View> media_foundation_renderer_error_message_;

  // Checkbox the user can use to indicate whether to silence the error message
  // for the origin.
  raw_ptr<views::Checkbox> media_foundation_renderer_error_checkbox_ = nullptr;
#endif

  absl::optional<ui::CaptionStyle> caption_style_;
  raw_ptr<CaptionBubbleModel> model_ = nullptr;

  OnErrorClickedCallback error_clicked_callback_;
  OnDoNotShowAgainClickedCallback error_silenced_callback_;
  base::ScopedClosureRunner destroyed_callback_;

  // Whether the caption bubble is expanded to show more lines of text.
  bool is_expanded_ = false;

  bool has_been_shown_ = false;

  // Whether we should hide the caption bubble on inactivity.
  bool const hide_on_inactivity_;

  // A timer which causes the bubble to hide if there is no activity after a
  // specified interval.
  std::unique_ptr<base::RetainingOneShotTimer> inactivity_timer_;
  raw_ptr<const base::TickClock> tick_clock_;
};

BEGIN_VIEW_BUILDER(/* no export */,
                   CaptionBubble,
                   views::BubbleDialogDelegateView)
END_VIEW_BUILDER

}  // namespace captions

DEFINE_VIEW_BUILDER(/* no export */, captions::CaptionBubble)

#endif  // COMPONENTS_LIVE_CAPTION_VIEWS_CAPTION_BUBBLE_H_
