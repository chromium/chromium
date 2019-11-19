// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_ICON_LOADING_INDICATOR_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_ICON_LOADING_INDICATOR_VIEW_H_

#include "base/macros.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/throb_animation.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

class PageActionIconView;

// The view that contains a throbber animation. It is shown when the action
// related to the page action icon is in progress.
// TODO(crbug.com/932818): Investigate the possibility of making this a layer
// instead of a view.
class PageActionIconLoadingIndicatorView : public views::View,
                                           public views::ViewObserver,
                                           public gfx::AnimationDelegate {
 public:
  explicit PageActionIconLoadingIndicatorView(PageActionIconView* parent);
  ~PageActionIconLoadingIndicatorView() override;

  void ShowAnimation();
  void StopAnimation();
  bool IsAnimating();

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;

  // views::ViewObserver:
  void OnViewBoundsChanged(views::View* observed_view) override;

  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override;

 private:
  base::Optional<base::TimeTicks> throbber_start_time_;

  gfx::ThrobAnimation animation_{this};

  PageActionIconView* const parent_;

  DISALLOW_COPY_AND_ASSIGN(PageActionIconLoadingIndicatorView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_ICON_LOADING_INDICATOR_VIEW_H_
