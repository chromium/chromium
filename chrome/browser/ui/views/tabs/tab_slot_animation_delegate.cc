// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_slot_animation_delegate.h"

TabSlotAnimationDelegate::TabSlotAnimationDelegate(TabContainer* tab_container,
                                                   TabSlotView* slot_view)
    : tab_container_(tab_container), slot_view_(slot_view) {
  slot_view_->set_animating(true);
}

TabSlotAnimationDelegate::~TabSlotAnimationDelegate() = default;

void TabSlotAnimationDelegate::AnimationProgressed(
    const gfx::Animation* animation) {
  tab_container_->OnTabSlotAnimationProgressed(slot_view_);
}

void TabSlotAnimationDelegate::AnimationEnded(const gfx::Animation* animation) {
  slot_view_->set_animating(false);
  AnimationProgressed(animation);
  slot_view_->DeprecatedLayoutImmediately();
}

void TabSlotAnimationDelegate::AnimationCanceled(
    const gfx::Animation* animation) {
  AnimationEnded(animation);
}
