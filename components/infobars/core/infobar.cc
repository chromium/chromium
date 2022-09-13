// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/infobars/core/infobar.h"

#include <cmath>
#include <memory>
#include <utility>

#include "base/check.h"
#include "build/build_config.h"
#include "components/infobars/core/infobar_container.h"
#include "components/infobars/core/infobar_manager.h"
#include "ui/gfx/animation/slide_animation.h"

namespace infobars {

InfoBar::InfoBar(std::unique_ptr<InfoBarDelegate> delegate)
    : owner_(nullptr),
      delegate_(std::move(delegate)),
      container_(nullptr),
      notifier_(std::make_unique<gfx::AnimationDelegateNotifier<>>(this)),
      animation_(notifier_.get()),
      height_(0),
      target_height_(0) {
  DCHECK(delegate_ != nullptr);
  animation_.SetTweenType(gfx::Tween::LINEAR);
  if (!gfx::Animation::ShouldRenderRichAnimation() ||
      !delegate_->ShouldAnimate()) {
    animation_.SetSlideDuration(base::TimeDelta());
  }
  delegate_->set_infobar(this);
}

InfoBar::~InfoBar() {
  DCHECK(!owner_);
}

void InfoBar::SetOwner(InfoBarManager* owner) {
  DCHECK(!owner_);
  owner_ = owner;
  delegate_->set_nav_entry_id(owner->GetActiveEntryID());
  PlatformSpecificSetOwner();
}

void InfoBar::SetNotifier(std::unique_ptr<gfx::AnimationDelegate> notifier) {
  notifier_ = std::move(notifier);
  animation_.set_delegate(notifier_.get());
}

void InfoBar::Show(bool animate) {
  PlatformSpecificShow(animate);
  if (animate) {
    animation_.Show();
  } else {
    animation_.Reset(1.0);
    RecalculateHeight(true);
  }
}

void InfoBar::Hide(bool animate) {
  PlatformSpecificHide(animate);
  if (animate) {
    animation_.Hide();
  } else {
    animation_.Reset(0.0);
    // We want to remove ourselves from the container immediately even if we
    // still have an owner, which MaybeDelete() won't do.
    DCHECK(container_);
    container_->RemoveInfoBar(this);
    MaybeDelete();  // Necessary if the infobar was already closing.
  }
}

void InfoBar::CloseSoon() {
  owner_ = nullptr;
  PlatformSpecificOnCloseSoon();
  MaybeDelete();
}

void InfoBar::RemoveSelf() {
  if (owner_)
    owner_->RemoveInfoBar(this);
}

void InfoBar::SetTargetHeight(int height) {
  if (target_height_ != height) {
    target_height_ = height;
    RecalculateHeight(false);
  }
}

void InfoBar::AnimationProgressed(const gfx::Animation* animation) {
  RecalculateHeight(false);
}

void InfoBar::AnimationEnded(const gfx::Animation* animation) {
  // When the animation ends, we must ensure the container is notified even if
  // the heights haven't changed, lest it never get an "animation finished"
  // notification.  (If the browser doesn't get this notification, it will not
  // bother to re-layout the content area for the new infobar size.)
  RecalculateHeight(true);
  MaybeDelete();
}

void InfoBar::RecalculateHeight(bool force_notify) {
  // If there's no container delegate, there's no way to compute the new height,
  // so return immediately.  We don't need to worry that this might leave us
  // with bogus sizes, because if we're ever re-added to a container, it will
  // call Show(false) while re-adding us, which will compute a correct set of
  // sizes.
  if (!container_ || !container_->delegate())
    return;

  int old_height = height_;
  height_ = animation_.CurrentValueBetween(0, target_height_);

  // Don't re-layout if nothing has changed, e.g. because the animation step was
  // not large enough to actually change the height by at least a pixel.
  bool height_differs = old_height != height_;
  if (height_differs)
    PlatformSpecificOnHeightRecalculated();

  if (height_differs || force_notify)
    container_->OnInfoBarStateChanged(animation_.is_animating());
}

void InfoBar::MaybeDelete() {
  if (!owner_ && (animation_.GetCurrentValue() == 0.0)) {
    if (container_)
      container_->RemoveInfoBar(this);
    delete this;
  }
}

}  // namespace infobars
