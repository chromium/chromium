// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_SLOT_ANIMATION_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_SLOT_ANIMATION_DELEGATE_H_

#include "chrome/browser/ui/views/tabs/tab_container.h"
#include "ui/gfx/animation/animation_delegate.h"

// Provides the ability to monitor when a tab's bounds have been animated. Used
// to hook callbacks to adjust things like tabstrip preferred size and tab group
// underlines.
class TabSlotAnimationDelegate : public gfx::AnimationDelegate {
 public:
  TabSlotAnimationDelegate(TabContainer* tab_container, TabSlotView* slot_view);
  TabSlotAnimationDelegate(const TabSlotAnimationDelegate&) = delete;
  TabSlotAnimationDelegate& operator=(const TabSlotAnimationDelegate&) = delete;
  ~TabSlotAnimationDelegate() override;

  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationCanceled(const gfx::Animation* animation) override;

 protected:
  TabContainer* tab_container() { return tab_container_; }
  TabSlotView* slot_view() { return slot_view_; }

 private:
  const raw_ptr<TabContainer, DanglingUntriaged> tab_container_;
  const raw_ptr<TabSlotView, AcrossTasksDanglingUntriaged> slot_view_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_SLOT_ANIMATION_DELEGATE_H_
