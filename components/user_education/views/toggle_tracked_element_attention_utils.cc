// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/views/toggle_tracked_element_attention_utils.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_host.h"

namespace user_education {

void MaybeRemoveAttentionStateFromTrackedElement(views::View* tracked_element) {
  views::InkDropHost* const ink_drop_host =
      views::InkDrop::Get(tracked_element);
  if (!ink_drop_host) {
    return;
  }

  ink_drop_host->ToggleAttentionState(false);
}

void MaybeApplyAttentionStateToTrackedElement(views::View* tracked_element) {
  views::InkDropHost* const ink_drop_host =
      views::InkDrop::Get(tracked_element);
  if (!ink_drop_host) {
    return;
  }

  ink_drop_host->ToggleAttentionState(true);
}

}  // namespace user_education
