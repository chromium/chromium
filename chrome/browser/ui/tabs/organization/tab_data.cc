// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/organization/tab_data.h"

#include "base/functional/bind.h"
#include "chrome/browser/ui/tabs/organization/tab_sensitivity_cache.h"
#include "chrome/browser/ui/tabs/public/tab_interface.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

TabData::TabData(tabs::TabModel* tab)
    : WebContentsObserver(tab->contents()),
      tab_(tab->GetHandle()),
      original_url_(tab->contents()->GetLastCommittedURL()) {
  CHECK(tab->owning_model());
  CHECK(tab);
  CHECK(tab->contents());

  original_tab_strip_model_ = tab->owning_model();
  original_tab_strip_model_->AddObserver(this);

  tab_subscriptions_.push_back(tab->RegisterWillDetach(
      base::BindRepeating(&TabData::OnTabWillDetach, base::Unretained(this))));
  tab_subscriptions_.push_back(
      tab->RegisterWillDiscardContents(base::BindRepeating(
          &TabData::OnTabWillDiscardContents, base::Unretained(this))));
  Observe(tab->contents());
}

TabData::~TabData() {
  if (original_tab_strip_model_) {
    original_tab_strip_model_->RemoveObserver(this);
  }

  for (auto& observer : observers_) {
    observer.OnTabDataDestroyed(tab_id());
  }
}

void TabData::AddObserver(TabData::Observer* observer) {
  observers_.AddObserver(observer);
}

void TabData::RemoveObserver(TabData::Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool TabData::IsValidForOrganizing(
    std::optional<tab_groups::TabGroupId> allowed_group_id) const {
  // if the model or the tab have been destroyed, then it's not valid.
  if (!original_tab_strip_model_ || !tab_.Get() || !tab_.Get()->contents()) {
    return false;
  }

  // If the web_contents is no longer the same URL, then it's not valid.
  if (original_url_ != tab_.Get()->contents()->GetLastCommittedURL()) {
    return false;
  }

  // All non http(s) schemes are invalid.
  if (!original_url_.SchemeIsHTTPOrHTTPS()) {
    return false;
  }

  // Tab is not valid if it does not exist in the TabStripModel any more.
  // NOTE: we will hear of this from the TabStripModel in OnTabStripModelChanged
  // below, but IsValidForOrganizing might be called from another
  // TabStripModelObserver first - most notably from a TabData's in another
  // TabOrganization - so we cannot assume our web_contents_ is already nullptr.
  if (tab_.Get()->owning_model() != original_tab_strip_model_) {
    return false;
  }

  // Tab is not valid if it is grouped and does not belong to
  // |allowed_group_id|.
  const std::optional<tab_groups::TabGroupId> tab_group_id =
      tab_.Get()->group();
  if (tab_group_id.has_value() &&
      (!allowed_group_id.has_value() ||
       tab_group_id.value() != allowed_group_id.value())) {
    return false;
  }

  if (tab_.Get()->pinned()) {
    return false;
  }

  return true;
}

void TabData::OnTabStripModelDestroyed(TabStripModel* tab_strip_model) {
  if (original_tab_strip_model_ == tab_strip_model) {
    original_tab_strip_model_ = nullptr;
    tab_ = tabs::TabHandle::Null();
    NotifyObserversOfUpdate();
  }
}

void TabData::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  NotifyObserversOfUpdate();
}

void TabData::OnTabWillDetach(tabs::TabInterface* tab,
                              tabs::TabInterface::DetachReason reason) {
  if (tab_.Get() == nullptr) {
    return;
  }

  CHECK_EQ(tab, tab_.Get());

  // Drop this tab, whether the reason is deletion or moving to another window.
  // Either way, this tab will no longer be valid for organizing.
  tab_ = tabs::TabHandle::Null();
  Observe(nullptr);
  NotifyObserversOfUpdate();
}

void TabData::OnTabWillDiscardContents(tabs::TabInterface* tab,
                                       content::WebContents* old_contents,
                                       content::WebContents* new_contents) {
  if (tab_.Get() == nullptr) {
    return;
  }

  CHECK_EQ(tab, tab_.Get());

  Observe(new_contents);
  NotifyObserversOfUpdate();
}

void TabData::NotifyObserversOfUpdate() {
  for (auto& observer : observers_) {
    observer.OnTabDataUpdated(this);
  }
}
