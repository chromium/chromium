// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FLYING_INDICATOR_H_
#define CHROME_BROWSER_UI_VIEWS_FLYING_INDICATOR_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/multi_animation.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace gfx {
struct VectorIcon;
}

namespace views {
class View;
}

// Shows a flying indicator with an icon that takes a curved path from a
// starting position to a target view. This can be used to indicate a link
// opening in the background, an item flying to the downloads tray, etc.
//
// The indicator is a bubble with a fixed trajectory curve that fades in, cannot
// be interacted with, flies to the target, and fades out.
class FlyingIndicator : public views::WidgetObserver,
                        public gfx::AnimationDelegate {
 public:
  ~FlyingIndicator() override;

  // Play a flying indicator with |icon| from |start| position to the center of
  // view |target|. When the indicator is finished flying (and has started
  // fading out), or if the flying indicator is destroyed for any reason (e.g.
  // the target view has disappeared), |done_callback| is called.
  //
  // Note that the flying indicator will still be fading out for a small period
  // of time after |done_callback| is called, so do not immediately destroy the
  // indicator object.
  //
  // If the returned object is freed, destroying the flying indicator
  // externally, the indicator disappears immediately and |done_callback| is
  // *not* called.
  static std::unique_ptr<FlyingIndicator> StartFlyingIndicator(
      const gfx::VectorIcon& icon,
      const gfx::Point& start,
      views::View* target,
      base::OnceClosure done_callback);

  bool is_flying() const {
    return animation_.is_animating() && animation_.current_part_index() <= 1U;
  }

 private:
  FlyingIndicator(const gfx::VectorIcon& icon,
                  const gfx::Point& start,
                  views::View* target,
                  base::OnceClosure done_callback);

  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;

  // views::WidgetObserver:
  void OnWidgetDestroyed(views::Widget* widget) override;

  const gfx::Point start_;
  const raw_ptr<const views::View> target_;
  gfx::Size bubble_size_;
  gfx::MultiAnimation animation_;
  base::OnceClosure done_callback_;
  raw_ptr<views::Widget> widget_ = nullptr;
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      scoped_observation_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_FLYING_INDICATOR_H_
