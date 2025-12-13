// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_slot_animation_delegate.h"

TabSlotAnimationDelegate::TabSlotAnimationDelegate(TabContainer* tab_container,
                                                   TabSlotView* slot_view)
    : tab_container_(tab_container), slot_view_(slot_view) {
  slot_view_->set_animating(true);
  view_observation_.Observe(slot_view);
}

TabSlotAnimationDelegate::~TabSlotAnimationDelegate() = default;

void TabSlotAnimationDelegate::AnimationProgressed(
    const gfx::Animation* animation) {
  tab_container_->OnTabSlotAnimationProgressed(slot_view_);
}

void TabSlotAnimationDelegate::AnimationEnded(const gfx::Animation* animation) {
  if (slot_view_) {
    slot_view_->set_animating(false);
  }
  AnimationProgressed(animation);
  if (slot_view_) {
    slot_view_->DeprecatedLayoutImmediately();
  }
}

void TabSlotAnimationDelegate::AnimationCanceled(
    const gfx::Animation* animation) {
  AnimationEnded(animation);
}

void TabSlotAnimationDelegate::OnViewIsDeleting(views::View* observed_view) {
  // In some cases animations can end after a view is deleted. When the view
  // is deleted, the animation stops tracking it, and will not Layout on
  // completion.
  CHECK_EQ(slot_view_, observed_view);
  slot_view_ = nullptr;
}

void TabSlotAnimationDelegate::StopObserving() {
  view_observation_.Reset();
}
