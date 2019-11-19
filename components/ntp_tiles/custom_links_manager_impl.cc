// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_tiles/custom_links_manager_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "components/ntp_tiles/constants.h"
#include "components/ntp_tiles/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"

namespace ntp_tiles {

CustomLinksManagerImpl::CustomLinksManagerImpl(
    PrefService* prefs,
    history::HistoryService* history_service)
    : prefs_(prefs), store_(prefs), history_service_observer_(this) {
  DCHECK(prefs);
  if (history_service)
    history_service_observer_.Add(history_service);
  if (IsInitialized())
    current_links_ = store_.RetrieveLinks();

  base::RepeatingClosure callback =
      base::BindRepeating(&CustomLinksManagerImpl::OnPreferenceChanged,
                          weak_ptr_factory_.GetWeakPtr());
  pref_change_registrar_.Init(prefs_);
  pref_change_registrar_.Add(prefs::kCustomLinksInitialized, callback);
  pref_change_registrar_.Add(prefs::kCustomLinksList, callback);
}

CustomLinksManagerImpl::~CustomLinksManagerImpl() = default;

bool CustomLinksManagerImpl::Initialize(const NTPTilesVector& tiles) {
  if (IsInitialized())
    return false;

  for (const NTPTile& tile : tiles)
    current_links_.emplace_back(Link{tile.url, tile.title, true});

  {
    base::AutoReset<bool> auto_reset(&updating_preferences_, true);
    prefs_->SetBoolean(prefs::kCustomLinksInitialized, true);
  }
  StoreLinks();
  return true;
}

void CustomLinksManagerImpl::Uninitialize() {
  {
    base::AutoReset<bool> auto_reset(&updating_preferences_, true);
    prefs_->SetBoolean(prefs::kCustomLinksInitialized, false);
  }
  ClearLinks();
}

bool CustomLinksManagerImpl::IsInitialized() const {
  return prefs_->GetBoolean(prefs::kCustomLinksInitialized);
}

const std::vector<CustomLinksManager::Link>& CustomLinksManagerImpl::GetLinks()
    const {
  return current_links_;
}

bool CustomLinksManagerImpl::AddLink(const GURL& url,
                                     const base::string16& title) {
  if (!IsInitialized() || !url.is_valid() ||
      current_links_.size() == ntp_tiles::kMaxNumCustomLinks) {
    return false;
  }

  if (FindLinkWithUrl(url) != current_links_.end())
    return false;

  previous_links_ = current_links_;
  current_links_.emplace_back(Link{url, title, false});
  StoreLinks();
  return true;
}

bool CustomLinksManagerImpl::UpdateLink(const GURL& url,
                                        const GURL& new_url,
                                        const base::string16& new_title) {
  if (!IsInitialized() || !url.is_valid() ||
      (new_url.is_empty() && new_title.empty())) {
    return false;
  }

  // Do not update if |new_url| is invalid or already exists in the list.
  if (!new_url.is_empty() &&
      (!new_url.is_valid() ||
       FindLinkWithUrl(new_url) != current_links_.end())) {
    return false;
  }

  auto it = FindLinkWithUrl(url);
  if (it == current_links_.end())
    return false;

  // At this point, we will be modifying at least one of the values.
  previous_links_ = current_links_;

  if (!new_url.is_empty())
    it->url = new_url;
  if (!new_title.empty())
    it->title = new_title;
  it->is_most_visited = false;

  StoreLinks();
  return true;
}

bool CustomLinksManagerImpl::ReorderLink(const GURL& url, size_t new_pos) {
  if (!IsInitialized() || !url.is_valid() || new_pos < 0 ||
      new_pos >= current_links_.size()) {
    return false;
  }

  auto curr_it = FindLinkWithUrl(url);
  if (curr_it == current_links_.end())
    return false;

  auto new_it = current_links_.begin() + new_pos;
  if (new_it == curr_it)
    return false;

  previous_links_ = current_links_;

  // If the new position is to the left of the current position, left rotate the
  // range [new_pos, curr_pos] until the link is first.
  if (new_it < curr_it)
    std::rotate(new_it, curr_it, curr_it + 1);
  // If the new position is to the right, we only need to left rotate the range
  // [curr_pos, new_pos] once so that the link is last.
  else
    std::rotate(curr_it, curr_it + 1, new_it + 1);

  StoreLinks();
  return true;
}

bool CustomLinksManagerImpl::DeleteLink(const GURL& url) {
  if (!IsInitialized() || !url.is_valid())
    return false;

  auto it = FindLinkWithUrl(url);
  if (it == current_links_.end())
    return false;

  previous_links_ = current_links_;
  current_links_.erase(it);
  StoreLinks();
  return true;
}

bool CustomLinksManagerImpl::UndoAction() {
  if (!IsInitialized() || !previous_links_.has_value())
    return false;

  // Replace the current links with the previous state.
  current_links_ = *previous_links_;
  previous_links_ = base::nullopt;
  StoreLinks();
  return true;
}

void CustomLinksManagerImpl::ClearLinks() {
  {
    base::AutoReset<bool> auto_reset(&updating_preferences_, true);
    store_.ClearLinks();
  }
  current_links_.clear();
  previous_links_ = base::nullopt;
}

void CustomLinksManagerImpl::StoreLinks() {
  base::AutoReset<bool> auto_reset(&updating_preferences_, true);
  store_.StoreLinks(current_links_);
}

std::vector<CustomLinksManager::Link>::iterator
CustomLinksManagerImpl::FindLinkWithUrl(const GURL& url) {
  return std::find_if(current_links_.begin(), current_links_.end(),
                      [&url](const Link& link) { return link.url == url; });
}

std::unique_ptr<base::CallbackList<void()>::Subscription>
CustomLinksManagerImpl::RegisterCallbackForOnChanged(
    base::RepeatingClosure callback) {
  return callback_list_.Add(callback);
}

// history::HistoryServiceObserver implementation.
void CustomLinksManagerImpl::OnURLsDeleted(
    history::HistoryService* history_service,
    const history::DeletionInfo& deletion_info) {
  // We don't care about expired entries.
  if (!IsInitialized() || deletion_info.is_from_expiration())
    return;

  size_t initial_size = current_links_.size();
  if (deletion_info.IsAllHistory()) {
    base::EraseIf(current_links_,
                  [](auto& link) { return link.is_most_visited; });
  } else {
    for (const history::URLRow& row : deletion_info.deleted_rows()) {
      auto it = FindLinkWithUrl(row.url());
      if (it != current_links_.end() && it->is_most_visited)
        current_links_.erase(it);
    }
  }
  StoreLinks();
  previous_links_ = base::nullopt;

  // Alert MostVisitedSites that some links have been deleted.
  if (initial_size != current_links_.size())
    callback_list_.Notify();
}

void CustomLinksManagerImpl::HistoryServiceBeingDeleted(
    history::HistoryService* history_service) {
  history_service_observer_.RemoveAll();
}

void CustomLinksManagerImpl::OnPreferenceChanged() {
  if (updating_preferences_)
    return;

  if (IsInitialized())
    current_links_ = store_.RetrieveLinks();
  else
    current_links_.clear();
  previous_links_ = base::nullopt;
  callback_list_.Notify();
}

// static
void CustomLinksManagerImpl::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* user_prefs) {
  user_prefs->RegisterBooleanPref(
      prefs::kCustomLinksInitialized, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  CustomLinksStore::RegisterProfilePrefs(user_prefs);
}

}  // namespace ntp_tiles
