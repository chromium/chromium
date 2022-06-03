// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/translate/translate_bubble_view_state_transition.h"

#include "base/check_op.h"
#include "base/metrics/histogram_macros.h"

namespace translate {

void ReportUiAction(translate::TranslateBubbleUiEvent action) {
  UMA_HISTOGRAM_ENUMERATION("Translate.BubbleUiEvent", action,
                            translate::TRANSLATE_BUBBLE_UI_EVENT_MAX);
}

}  // namespace translate

TranslateBubbleViewStateTransition::TranslateBubbleViewStateTransition(
    TranslateBubbleModel::ViewState view_state)
    : view_state_(view_state), view_state_before_advanced_view_(view_state) {
  // The initial view type must not be 'Advanced'.
  DCHECK_NE(TranslateBubbleModel::VIEW_STATE_SOURCE_LANGUAGE, view_state_);
  DCHECK_NE(TranslateBubbleModel::VIEW_STATE_TARGET_LANGUAGE, view_state_);
}

void TranslateBubbleViewStateTransition::SetViewState(
    TranslateBubbleModel::ViewState view_state) {
  view_state_ = view_state;
  if (view_state != TranslateBubbleModel::VIEW_STATE_SOURCE_LANGUAGE &&
      view_state != TranslateBubbleModel::VIEW_STATE_TARGET_LANGUAGE)
    view_state_before_advanced_view_ = view_state;
  else
    translate::ReportUiAction(translate::SET_STATE_OPTIONS);
}

void TranslateBubbleViewStateTransition::GoBackFromAdvanced() {
  DCHECK_NE(TranslateBubbleModel::VIEW_STATE_SOURCE_LANGUAGE, view_state_);
  DCHECK_NE(TranslateBubbleModel::VIEW_STATE_TARGET_LANGUAGE, view_state_);
  translate::ReportUiAction(translate::LEAVE_STATE_OPTIONS);
  SetViewState(view_state_before_advanced_view_);
}
