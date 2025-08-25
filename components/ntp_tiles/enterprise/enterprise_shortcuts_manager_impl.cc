// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_tiles/enterprise/enterprise_shortcuts_manager_impl.h"

#include <vector>

#include "base/functional/bind.h"
#include "components/ntp_tiles/custom_links_util.h"
#include "components/prefs/pref_service.h"

namespace ntp_tiles {

EnterpriseShortcutsManagerImpl::EnterpriseShortcutsManagerImpl(
    PrefService* prefs)
    : prefs_(prefs), store_(prefs) {
  DCHECK(prefs);
  current_links_ = store_.RetrieveLinks();

  store_subscription_ = store_.RegisterCallbackForOnChanged(
      base::BindRepeating(&EnterpriseShortcutsManagerImpl::OnStoreLinksChanged,
                          weak_ptr_factory_.GetWeakPtr()));
}

EnterpriseShortcutsManagerImpl::~EnterpriseShortcutsManagerImpl() = default;

void EnterpriseShortcutsManagerImpl::RestorePolicyLinks() {
  ClearLinks();
  current_links_ = store_.RetrieveLinks();
}

const std::vector<EnterpriseShortcut>&
EnterpriseShortcutsManagerImpl::GetLinks() const {
  return current_links_;
}

bool EnterpriseShortcutsManagerImpl::UpdateLink(const GURL& url,
                                                const std::u16string& title) {
  if (!url.is_valid() || title.empty()) {
    return false;
  }

  auto it = custom_links_util::FindLinkWithUrl<EnterpriseShortcut>(
      current_links_, url);
  if (it == current_links_.end() || it->allow_user_edit == false) {
    return false;
  }

  // Modify the title and change the policy origin to no policy.
  previous_links_ = current_links_;
  it->title = title;
  it->policy_origin = EnterpriseShortcut::PolicyOrigin::kNoPolicy;

  StoreLinks();
  return true;
}

bool EnterpriseShortcutsManagerImpl::ReorderLink(const GURL& url,
                                                 size_t new_pos) {
  if (!custom_links_util::ReorderLink<EnterpriseShortcut>(
          current_links_, previous_links_, url, new_pos)) {
    return false;
  }

  StoreLinks();
  return true;
}

bool EnterpriseShortcutsManagerImpl::DeleteLink(const GURL& url) {
  if (!url.is_valid()) {
    return false;
  }

  auto it = custom_links_util::FindLinkWithUrl<EnterpriseShortcut>(
      current_links_, url);
  if (it == current_links_.end() || it->allow_user_delete == false) {
    return false;
  }

  previous_links_ = current_links_;
  it->is_hidden_by_user = true;
  StoreLinks();
  return true;
}

bool EnterpriseShortcutsManagerImpl::UndoAction() {
  if (!custom_links_util::UndoAction<EnterpriseShortcut>(current_links_,
                                                         previous_links_)) {
    return false;
  }

  StoreLinks();
  return true;
}

void EnterpriseShortcutsManagerImpl::ClearLinks() {
  store_.ClearLinks();
  current_links_.clear();
  previous_links_ = std::nullopt;
}

void EnterpriseShortcutsManagerImpl::StoreLinks() {
  store_.StoreLinks(current_links_);
}

base::CallbackListSubscription
EnterpriseShortcutsManagerImpl::RegisterCallbackForOnChanged(
    base::RepeatingClosure callback) {
  return closure_list_.Add(callback);
}

void EnterpriseShortcutsManagerImpl::OnStoreLinksChanged() {
  std::vector<EnterpriseShortcut> new_links = store_.RetrieveLinks();
  if (new_links == current_links_) {
    return;
  }
  current_links_ = new_links;
  previous_links_ = std::nullopt;
  closure_list_.Notify();
}

// static
void EnterpriseShortcutsManagerImpl::RegisterProfilePrefs(
    PrefRegistrySimple* user_prefs) {
  EnterpriseShortcutsStore::RegisterProfilePrefs(user_prefs);
}

}  // namespace ntp_tiles
