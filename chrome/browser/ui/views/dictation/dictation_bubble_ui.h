// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DICTATION_DICTATION_BUBBLE_UI_H_
#define CHROME_BROWSER_UI_VIEWS_DICTATION_DICTATION_BUBBLE_UI_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/dictation/ui_state.h"
#include "chrome/browser/ui/views/voice/voice_plate.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/animation/tween.h"
#include "ui/views/animation/animation_delegate_views.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace dictation {

// This class implements the agent toast UI for dictation.
class DictationBubbleUi : public voice::VoicePlate {
 public:
  // If `show_reviewing_page_status` is true, the bubble initially shows a
  // transient "Reviewing page" status in place of the toggle button.
  DictationBubbleUi(views::View* anchor_view,
                    base::RepeatingClosure close_callback,
                    base::RepeatingClosure toggle_active_stream_callback,
                    bool show_reviewing_page_status);
  ~DictationBubbleUi() override;

  void Show() override;
  void SetState(UiState state);
  void UpdateAudioLevel(float audio_level);
  void SetReviewingPageStatusDurationForTesting(base::TimeDelta duration);

  // views::BubbleDialogDelegate:
  void Init() override;
  views::View* GetInitiallyFocusedView() override;

  UiState state() const { return state_; }

  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kViewElementIdForTesting);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kCloseButtonElementIdForTesting);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kToggleButtonElementIdForTesting);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kWaveformElementIdForTesting);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kSeparatorElementIdForTesting);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(
      kReviewingPageStatusLabelElementIdForTesting);

 private:
  UiState state_ = UiState::kInactive;
  base::WeakPtrFactory<DictationBubbleUi> weak_ptr_factory_{this};
};

}  // namespace dictation

#endif  // CHROME_BROWSER_UI_VIEWS_DICTATION_DICTATION_BUBBLE_UI_H_
