// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/infobars/core/infobar_container.h"

#include <algorithm>

#include "base/auto_reset.h"
#include "base/logging.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/metrics_hashes.h"
#include "base/stl_util.h"
#include "build/build_config.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_delegate.h"

namespace infobars {

InfoBarContainer::Delegate::~Delegate() {
}

InfoBarContainer::InfoBarContainer(Delegate* delegate)
    : delegate_(delegate),
      infobar_manager_(nullptr),
      ignore_infobar_state_changed_(false) {}

InfoBarContainer::~InfoBarContainer() {
  // RemoveAllInfoBarsForDestruction() should have already cleared our infobars.
  DCHECK(infobars_.empty());
  if (infobar_manager_)
    infobar_manager_->RemoveObserver(this);
}

void InfoBarContainer::ChangeInfoBarManager(InfoBarManager* infobar_manager) {
  if (infobar_manager_)
    infobar_manager_->RemoveObserver(this);

  bool state_changed = false;
  {
    // Ignore intermediate state changes.  We'll manually trigger processing
    // once we're finished.
    base::AutoReset<bool> ignore_changes(&ignore_infobar_state_changed_, true);

    // Hides all infobars in this container without animation.
    while (!infobars_.empty()) {
      InfoBar* infobar = infobars_.front();
      // Inform the infobar that it's hidden.  If it was already closing, this
      // deletes it.  Otherwise, this ensures the infobar will be deleted if
      // it's closed while it's not in an InfoBarContainer.
      infobar->Hide(false);
      state_changed = true;
    }

    infobar_manager_ = infobar_manager;
    if (infobar_manager_) {
      infobar_manager_->AddObserver(this);

      for (size_t i = 0; i < infobar_manager_->infobar_count(); ++i) {
        AddInfoBar(infobar_manager_->infobar_at(i), i, false);
        state_changed = true;
      }
    }
  }

  // Now that everything is up to date, signal the delegate to re-layout.
  if (state_changed)
    OnInfoBarStateChanged(false);
}

void InfoBarContainer::OnInfoBarStateChanged(bool is_animating) {
  if (ignore_infobar_state_changed_)
    return;
  if (delegate_)
    delegate_->InfoBarContainerStateChanged(is_animating);
  PlatformSpecificInfoBarStateChanged(is_animating);
}

void InfoBarContainer::RemoveInfoBar(InfoBar* infobar) {
  infobar->set_container(nullptr);
  auto i(std::find(infobars_.begin(), infobars_.end(), infobar));
  DCHECK(i != infobars_.end());
  PlatformSpecificRemoveInfoBar(infobar);
  infobars_.erase(i);
}

void InfoBarContainer::RemoveAllInfoBarsForDestruction() {
  // Before we remove any children, we reset |delegate_|, so that no removals
  // will result in us trying to call
  // delegate_->InfoBarContainerStateChanged().  This is important because at
  // this point |delegate_| may be shutting down, and it's at best unimportant
  // and at worst disastrous to call that.
  delegate_ = nullptr;
  ChangeInfoBarManager(nullptr);
}

void InfoBarContainer::OnInfoBarAdded(InfoBar* infobar) {
  AddInfoBar(infobar, infobars_.size(), true);
}

void InfoBarContainer::OnInfoBarRemoved(InfoBar* infobar, bool animate) {
  DCHECK(infobar_manager_);
  infobar->Hide(infobar_manager_->animations_enabled() && animate);
}

void InfoBarContainer::OnInfoBarReplaced(InfoBar* old_infobar,
                                         InfoBar* new_infobar) {
  PlatformSpecificReplaceInfoBar(old_infobar, new_infobar);
  InfoBars::const_iterator i(std::find(infobars_.begin(), infobars_.end(),
                                       old_infobar));
  DCHECK(i != infobars_.end());
  size_t position = i - infobars_.begin();
  old_infobar->Hide(false);
  AddInfoBar(new_infobar, position, false);
}

void InfoBarContainer::OnManagerShuttingDown(InfoBarManager* manager) {
  DCHECK_EQ(infobar_manager_, manager);
  infobar_manager_->RemoveObserver(this);
  infobar_manager_ = nullptr;
}

void InfoBarContainer::AddInfoBar(InfoBar* infobar,
                                  size_t position,
                                  bool animate) {
  DCHECK(!base::Contains(infobars_, infobar));
  DCHECK_LE(position, infobars_.size());
  infobars_.insert(infobars_.begin() + position, infobar);
  PlatformSpecificAddInfoBar(infobar, position);
  infobar->set_container(this);
  DCHECK(infobar_manager_);
  infobar->Show(infobar_manager_->animations_enabled() && animate);

  // Record the infobar being displayed.
  DCHECK_NE(InfoBarDelegate::INVALID, infobar->delegate()->GetIdentifier());
  base::UmaHistogramSparse("InfoBar.Shown",
                           infobar->delegate()->GetIdentifier());
}

}  // namespace infobars
