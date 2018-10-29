// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/search_ipc_router_policy_impl.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "content/public/browser/web_contents.h"

SearchIPCRouterPolicyImpl::SearchIPCRouterPolicyImpl(
    const content::WebContents* web_contents)
    : web_contents_(web_contents),
      is_incognito_(true) {
  DCHECK(web_contents);

  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  if (profile)
    is_incognito_ = profile->IsOffTheRecord();
}

SearchIPCRouterPolicyImpl::~SearchIPCRouterPolicyImpl() {}

bool SearchIPCRouterPolicyImpl::ShouldProcessFocusOmnibox(bool is_active_tab) {
  return is_active_tab && !is_incognito_ && search::IsInstantNTP(web_contents_);
}

bool SearchIPCRouterPolicyImpl::ShouldProcessDeleteMostVisitedItem() {
  return !is_incognito_ && search::IsInstantNTP(web_contents_);
}

bool SearchIPCRouterPolicyImpl::ShouldProcessUndoMostVisitedDeletion() {
  return !is_incognito_ && search::IsInstantNTP(web_contents_);
}

bool SearchIPCRouterPolicyImpl::ShouldProcessUndoAllMostVisitedDeletions() {
  return !is_incognito_ && search::IsInstantNTP(web_contents_);
}

bool SearchIPCRouterPolicyImpl::ShouldProcessAddCustomLink() {
  return !is_incognito_ && search::IsInstantNTP(web_contents_);
}

bool SearchIPCRouterPolicyImpl::ShouldProcessUpdateCustomLink() {
  return !is_incognito_ && search::IsInstantNTP(web_contents_);
}

bool SearchIPCRouterPolicyImpl::ShouldProcessDeleteCustomLink() {
  return !is_incognito_ && search::IsInstantNTP(web_contents_);
}

bool SearchIPCRouterPolicyImpl::ShouldProcessUndoCustomLinkAction() {
  return !is_incognito_ && search::IsInstantNTP(web_contents_);
}

bool SearchIPCRouterPolicyImpl::ShouldProcessResetCustomLinks() {
  return !is_incognito_ && search::IsInstantNTP(web_contents_);
}

bool SearchIPCRouterPolicyImpl::ShouldProcessDoesUrlResolve() {
  return !is_incognito_ && search::IsInstantNTP(web_contents_);
}

bool SearchIPCRouterPolicyImpl::ShouldProcessLogEvent() {
  return !is_incognito_ && search::IsInstantNTP(web_contents_);
}

bool SearchIPCRouterPolicyImpl::ShouldProcessPasteIntoOmnibox(
    bool is_active_tab) {
  return is_active_tab && !is_incognito_ && search::IsInstantNTP(web_contents_);
}

bool SearchIPCRouterPolicyImpl::ShouldProcessChromeIdentityCheck() {
  return !is_incognito_ && search::IsInstantNTP(web_contents_);
}

bool SearchIPCRouterPolicyImpl::ShouldProcessHistorySyncCheck() {
  return !is_incognito_ && search::IsInstantNTP(web_contents_);
}

bool SearchIPCRouterPolicyImpl::ShouldSendSetInputInProgress(
    bool is_active_tab) {
  return is_active_tab && !is_incognito_;
}

bool SearchIPCRouterPolicyImpl::ShouldSendOmniboxFocusChanged() {
  return !is_incognito_;
}

bool SearchIPCRouterPolicyImpl::ShouldSendMostVisitedItems() {
  return !is_incognito_ && search::IsInstantNTP(web_contents_);
}

bool SearchIPCRouterPolicyImpl::ShouldSendThemeBackgroundInfo() {
  return !is_incognito_ && search::IsInstantNTP(web_contents_);
}

bool SearchIPCRouterPolicyImpl::ShouldProcessSelectLocalBackgroundImage() {
  return !is_incognito_ && search::IsInstantNTP(web_contents_);
}

bool SearchIPCRouterPolicyImpl::ShouldProcessSetCustomBackgroundURL() {
  return !is_incognito_ && search::IsInstantNTP(web_contents_);
}

bool SearchIPCRouterPolicyImpl::
    ShouldProcessSetCustomBackgroundURLWithAttributions() {
  return !is_incognito_ && search::IsInstantNTP(web_contents_);
}
