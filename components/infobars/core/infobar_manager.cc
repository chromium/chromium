// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/infobars/core/infobar_manager.h"

#include <utility>

#include "components/infobars/core/infobar.h"

namespace infobars {


// InfoBarManager::Observer ---------------------------------------------------

InfoBarManager::Observer::~Observer() {
}

void InfoBarManager::Observer::OnInfoBarAdded(InfoBar* infobar) {
}

void InfoBarManager::Observer::OnInfoBarRemoved(InfoBar* infobar,
                                                bool animate) {
}

void InfoBarManager::Observer::OnInfoBarReplaced(InfoBar* old_infobar,
                                                 InfoBar* new_infobar) {
}

void InfoBarManager::Observer::OnManagerShuttingDown(InfoBarManager* manager) {
}


// InfoBarManager --------------------------------------------------------------

InfoBar* InfoBarManager::AddInfoBar(std::unique_ptr<InfoBar> infobar,
                                    bool replace_existing) {
  DCHECK(infobar);

  for (InfoBars::const_iterator i(infobars_.begin()); i != infobars_.end();
       ++i) {
    if ((*i)->delegate()->EqualsDelegate(infobar->delegate())) {
      DCHECK_NE((*i)->delegate(), infobar->delegate());
      return replace_existing ? ReplaceInfoBar(*i, std::move(infobar))
                              : nullptr;
    }
  }

  InfoBar* infobar_ptr = infobar.release();
  infobars_.push_back(infobar_ptr);
  infobar_ptr->SetOwner(this);

  for (Observer& observer : observer_list_)
    observer.OnInfoBarAdded(infobar_ptr);

  return infobar_ptr;
}

void InfoBarManager::RemoveInfoBar(InfoBar* infobar) {
  RemoveInfoBarInternal(infobar, true);
}

void InfoBarManager::RemoveAllInfoBars(bool animate) {
  while (!infobars_.empty())
    RemoveInfoBarInternal(infobars_.back(), animate);
}

InfoBar* InfoBarManager::ReplaceInfoBar(InfoBar* old_infobar,
                                        std::unique_ptr<InfoBar> new_infobar) {
  DCHECK(old_infobar);
  DCHECK(new_infobar);

  auto i(std::find(infobars_.begin(), infobars_.end(), old_infobar));
  DCHECK(i != infobars_.end());

  InfoBar* new_infobar_ptr = new_infobar.release();
  i = infobars_.insert(i, new_infobar_ptr);
  new_infobar_ptr->SetOwner(this);

  // Remove the old infobar before notifying, so that if any observers call back
  // to AddInfoBar() or similar, we don't dupe-check against this infobar.
  infobars_.erase(++i);

  for (Observer& observer : observer_list_)
    observer.OnInfoBarReplaced(old_infobar, new_infobar_ptr);

  old_infobar->CloseSoon();
  return new_infobar_ptr;
}

void InfoBarManager::AddObserver(Observer* obs) {
  observer_list_.AddObserver(obs);
}

void InfoBarManager::RemoveObserver(Observer* obs) {
  observer_list_.RemoveObserver(obs);
}

InfoBarManager::InfoBarManager() = default;

InfoBarManager::~InfoBarManager() = default;

void InfoBarManager::ShutDown() {
  // Destroy all remaining InfoBars.  It's important to not animate here so that
  // we guarantee that we'll delete all delegates before we do anything else.
  RemoveAllInfoBars(false);
  for (Observer& observer : observer_list_)
    observer.OnManagerShuttingDown(this);
}

void InfoBarManager::OnNavigation(
    const InfoBarDelegate::NavigationDetails& details) {
  // NOTE: It is not safe to change the following code to count upwards or
  // use iterators, as the RemoveInfoBar() call synchronously modifies our
  // delegate list.
  for (size_t i = infobars_.size(); i > 0; --i) {
    InfoBar* infobar = infobars_[i - 1];
    if (infobar->delegate()->ShouldExpire(details))
      RemoveInfoBar(infobar);
  }
}

void InfoBarManager::RemoveInfoBarInternal(InfoBar* infobar, bool animate) {
  DCHECK(infobar);

  auto i(std::find(infobars_.begin(), infobars_.end(), infobar));
  DCHECK(i != infobars_.end());

  // Remove the infobar before notifying, so that if any observers call back to
  // AddInfoBar() or similar, we don't dupe-check against this infobar.
  infobars_.erase(i);

  // This notification must happen before the call to CloseSoon() below, since
  // observers may want to access |infobar| and that call can delete it.
  for (Observer& observer : observer_list_)
    observer.OnInfoBarRemoved(infobar, animate);

  infobar->CloseSoon();
}

}  // namespace infobars
