// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/exclusive_access/exclusive_access_controller_base.h"

#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/web_contents.h"

using content::WebContents;

ExclusiveAccessControllerBase::ExclusiveAccessControllerBase(
    ExclusiveAccessManager* manager)
    : manager_(manager) {}

ExclusiveAccessControllerBase::~ExclusiveAccessControllerBase() {
}

GURL ExclusiveAccessControllerBase::GetExclusiveAccessBubbleURL() const {
  return manager_->GetExclusiveAccessBubbleURL();
}

GURL ExclusiveAccessControllerBase::GetURLForExclusiveAccessBubble() const {
  return exclusive_access_tab() ? exclusive_access_tab()->GetURL() : GURL();
}

void ExclusiveAccessControllerBase::OnTabDeactivated(
    WebContents* web_contents) {
  if (web_contents != exclusive_access_tab()) {
    return;
  }
  ExitExclusiveAccessIfNecessary();
}

void ExclusiveAccessControllerBase::OnTabDetachedFromView(
    WebContents* old_contents) {
  // Derived class will have to implement if necessary.
}

void ExclusiveAccessControllerBase::OnTabClosing(WebContents* web_contents) {
  if (web_contents != exclusive_access_tab()) {
    return;
  }

  ExitExclusiveAccessIfNecessary();

  // The call to exit exclusive access may result in asynchronous notification
  // of state change (e.g. fullscreen change on Linux). We don't want to rely
  // on it to call NotifyTabExclusiveAccessLost(), because at that point
  // |tab_with_exclusive_access_| may not be valid. Instead, we call it here
  // to clean up exclusive access tab related state.
  NotifyTabExclusiveAccessLost();
}

void ExclusiveAccessControllerBase::SetTabWithExclusiveAccess(
    WebContents* tab) {
  // Tab should never be replaced with another tab.
  CHECK(exclusive_access_tab() == tab || exclusive_access_tab() == nullptr ||
        tab == nullptr);
  web_contents_observer_.Observe(tab);
}

//////////
// ExclusiveAccessControllerBase::WebContentsObserver

ExclusiveAccessControllerBase::WebContentsObserver::WebContentsObserver(
    ExclusiveAccessControllerBase& controller)
    : controller_(controller) {}

void ExclusiveAccessControllerBase::WebContentsObserver::
    NavigationEntryCommitted(
        const content::LoadCommittedDetails& load_details) {
  if (!load_details.is_navigation_to_different_page()) {
    return;
  }
  controller_->ExitExclusiveAccessIfNecessary();
}
