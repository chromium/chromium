// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DICTATION_DICTATION_BUBBLE_UI_H_
#define CHROME_BROWSER_UI_VIEWS_DICTATION_DICTATION_BUBBLE_UI_H_

#include "base/functional/callback.h"
#include "chrome/browser/ui/views/dictation/ui_state.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/animation/tween.h"
#include "ui/views/animation/animation_delegate_views.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace views {
class Widget;
}

namespace dictation {

// This class implements the agent toast UI for dictation.
class DictationBubbleUi : public views::BubbleDialogDelegate {
 public:
  explicit DictationBubbleUi(
      views::View* anchor_view,
      base::RepeatingClosure close_callback,
      base::RepeatingClosure toggle_active_stream_callback);
  ~DictationBubbleUi() override;

  void Show();
  void SetState(UiState state);
  void UpdateAudioLevel(float audio_level);

  // views::BubbleDialogDelegate:
  gfx::Rect GetBubbleBounds() override;
  void Init() override;

  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kViewElementIdForTesting);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kCloseButtonElementIdForTesting);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kToggleButtonElementIdForTesting);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kWaveformElementIdForTesting);

 private:
  std::unique_ptr<views::Widget> widget_;
  UiState state_ = UiState::kInactive;
};

}  // namespace dictation

#endif  // CHROME_BROWSER_UI_VIEWS_DICTATION_DICTATION_BUBBLE_UI_H_
