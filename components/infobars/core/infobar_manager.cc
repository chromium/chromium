// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/infobars/core/infobar_manager.h"

#include <utility>

#include "base/command_line.h"
#include "base/observer_list.h"
#include "base/ranges/algorithm.h"
#include "build/branding_buildflags.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobars_switches.h"
#include "ui/gfx/switches.h"

namespace infobars {

namespace {

bool DisableInfoBars() {
  const auto* const command_line = base::CommandLine::ForCurrentProcess();
  // Infobars can only be disabled when Chrome is running in headless mode and
  // in Chrome for Testing.
  return command_line->HasSwitch(::switches::kDisableInfoBars)
#if !BUILDFLAG(CHROME_FOR_TESTING)
         && command_line->HasSwitch(::switches::kHeadless)
#endif
      ;
}

}  // namespace

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

InfoBar* InfoBarManager::AddInfoBar(std::unique_ptr<InfoBar> new_infobar,
                                    bool replace_existing) {
  DCHECK(new_infobar);

  for (infobars::InfoBar* infobar : infobars_) {
    if (infobar->delegate()->EqualsDelegate(new_infobar->delegate())) {
      DCHECK_NE(infobar->delegate(), new_infobar->delegate());
      return replace_existing ? ReplaceInfoBar(infobar, std::move(new_infobar))
                              : nullptr;
    }
  }

  if (!ShouldShowInfoBar(new_infobar.get())) {
    return nullptr;
  }

  InfoBar* infobar_ptr = new_infobar.release();
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

  if (!ShouldShowInfoBar(new_infobar.get())) {
    RemoveInfoBar(old_infobar);
    return nullptr;
  }

  auto i = base::ranges::find(infobars_, old_infobar);
  CHECK(i != infobars_.end());

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

InfoBarManager::InfoBarManager() : infobars_enabled_(!DisableInfoBars()) {}

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

  auto i = base::ranges::find(infobars_, infobar);
  // TODO(crbug.com/): Temporarily a CHECK instead of a DCHECK CHECK() in order
  // to help diagnose suspected memory smashing caused by invalid call of this
  // method happening in production code on iOS.
  CHECK(i != infobars_.end());

  // Remove the infobar before notifying, so that if any observers call back to
  // AddInfoBar() or similar, we don't dupe-check against this infobar.
  infobars_.erase(i);

  // This notification must happen before the call to CloseSoon() below, since
  // observers may want to access |infobar| and that call can delete it.
  for (Observer& observer : observer_list_)
    observer.OnInfoBarRemoved(infobar, animate);

  infobar->CloseSoon();
}

bool InfoBarManager::ShouldShowInfoBar(const InfoBar* infobar) const {
  DCHECK(infobar);

  if (infobars_enabled_) {
    return true;
  }

  // Only buttonless infobars should be disabled. The ones with buttons are
  // semantically message boxes and must be shown because certain functionality
  // depends on them, see crbug.com/333945848 and crbug.com/341947684.
  const auto* const delegate = infobar->delegate()->AsConfirmInfoBarDelegate();
  return delegate &&
         delegate->GetButtons() != ConfirmInfoBarDelegate::BUTTON_NONE;
}

}  // namespace infobars
