// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INFOBARS_CORE_INFOBAR_H_
#define COMPONENTS_INFOBARS_CORE_INFOBAR_H_

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "components/infobars/core/infobar_delegate.h"
#include "ui/gfx/animation/animation_delegate_notifier.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/geometry/size.h"

namespace infobars {

class InfoBarContainer;
class InfoBarManager;

// InfoBar is a cross-platform base class for an infobar "view" (in the MVC
// sense), which owns a corresponding InfoBarDelegate "model".  Typically,
// a caller will call XYZInfoBarDelegate::Create() and pass in the
// InfoBarManager for the relevant tab.  This will create an XYZInfoBarDelegate,
// create a platform-specific subclass of InfoBar to own it, and then call
// InfoBarManager::AddInfoBar() to give it ownership of the infobar.
// During its life, the InfoBar may be shown and hidden as the owning tab is
// switched between the foreground and background.  Eventually, InfoBarManager
// will instruct the InfoBar to close itself.  At this point, the InfoBar will
// optionally animate closed; once it's no longer visible, it deletes itself,
// destroying the InfoBarDelegate in the process.
//
// Thus, InfoBarDelegate and InfoBar implementations can assume they share
// lifetimes, and not NULL-check each other; but if one needs to reach back into
// the owning InfoBarManager, it must check whether that's still possible.
class InfoBar : public gfx::AnimationDelegate {
 public:
  explicit InfoBar(std::unique_ptr<InfoBarDelegate> delegate);

  InfoBar(const InfoBar&) = delete;
  InfoBar& operator=(const InfoBar&) = delete;

  ~InfoBar() override;

  InfoBarManager* owner() { return owner_; }
  InfoBarDelegate* delegate() const { return delegate_.get(); }
  void set_container(InfoBarContainer* container) { container_ = container; }

  // Sets |owner_|.  This also sets the nav entry ID on |delegate_|.  This must
  // only be called once as there's no way to extract an infobar from its owner
  // without deleting it, for reparenting in another tab.
  void SetOwner(InfoBarManager* owner);

  void SetNotifier(std::unique_ptr<gfx::AnimationDelegate> notifier);

  // Makes the infobar visible.  If |animate| is true, the infobar is then
  // animated to full size.
  void Show(bool animate);

  // Makes the infobar hidden.  If |animate| is false, the infobar is
  // immediately removed from the container, and, if now unowned, deleted.  If
  // |animate| is true, the infobar is animated to zero size, ultimately
  // triggering a call to AnimationEnded().
  void Hide(bool animate);

  // Notifies the infobar that it is no longer owned and should delete itself
  // once it is invisible.
  void CloseSoon();

  // Forwards a close request to our owner.  This is a no-op if we're already
  // unowned.
  void RemoveSelf();

  // Changes the target height of the infobar.
  void SetTargetHeight(int height);

  const gfx::SlideAnimation& animation() const { return animation_; }
  int computed_height() const { return height_; }

  InfoBarDelegate::InfoBarIdentifier GetIdentifier() const {
    return delegate_->GetIdentifier();
  }

 protected:
  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override;

  const InfoBarContainer* container() const { return container_; }
  InfoBarContainer* container() { return container_; }
  gfx::SlideAnimation* animation() { return &animation_; }
  int target_height() const { return target_height_; }

  // Platforms may optionally override these if they need to do work during
  // processing of the given calls.
  virtual void PlatformSpecificSetOwner() {}
  virtual void PlatformSpecificShow(bool animate) {}
  virtual void PlatformSpecificHide(bool animate) {}
  virtual void PlatformSpecificOnCloseSoon() {}
  virtual void PlatformSpecificOnHeightRecalculated() {}

 private:
  // gfx::AnimationDelegate:
  void AnimationEnded(const gfx::Animation* animation) override;

  // Finds the new desired height, and if it differs from the current height,
  // calls PlatformSpecificOnHeightRecalculated().  Informs our container our
  // state has changed if either the height has changed or |force_notify| is
  // set.
  void RecalculateHeight(bool force_notify);

  // Checks whether the infobar is unowned and done with all animations.  If so,
  // notifies the container that it should remove this infobar, and deletes
  // itself.
  void MaybeDelete();

  raw_ptr<InfoBarManager> owner_;
  std::unique_ptr<InfoBarDelegate> delegate_;
  raw_ptr<InfoBarContainer> container_;

  std::unique_ptr<gfx::AnimationDelegate> notifier_;
  gfx::SlideAnimation animation_;

  // The current and target heights.
  int height_;  // Includes both fill and bottom separator.
  int target_height_;
};

}  // namespace infobars

#endif  // COMPONENTS_INFOBARS_CORE_INFOBAR_H_
