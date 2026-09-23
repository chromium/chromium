// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_tiles/custom_links_manager_impl.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/auto_reset.h"
#include "base/functional/bind.h"
#include "components/ntp_tiles/constants.h"
#include "components/ntp_tiles/custom_links_util.h"
#include "components/ntp_tiles/metrics.h"
#include "components/ntp_tiles/most_visited_sites.h"
#include "components/ntp_tiles/pref_names.h"
#include "components/ntp_tiles/tile_type.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/search/ntp_features.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace ntp_tiles {

CustomLinksManagerImpl::CustomLinksManagerImpl(const Options& options)
    : custom_links_initialized_pref_(
          options.scope == CustomLinksScope::kDesktop
              ? prefs::kCustomLinksInitialized
              : prefs::kCustomLinksInitializedMobile),
      prefs_(options.prefs),
      max_links_(
          base::FeatureList::IsEnabled(ntp_features::kNtpShortcutsRedesign)
              ? ntp_features::GetMaxShortcutsInExpandedState()
              : options.max_links),
      enable_ai_mode_tile_(options.enable_ai_mode_tile),
      store_(options.prefs, options.scope) {
  DCHECK(prefs_);
  if (options.history_service) {
    history_service_observation_.Observe(options.history_service.get());
  }
  if (IsInitialized()) {
    current_links_ = store_.RetrieveLinks();
    if (enable_ai_mode_tile_) {
      InsertAiModeLinkIfNeeded(current_links_);
    } else {
      GURL ai_mode_url(kAiModeTileUrl);
      std::erase_if(current_links_, [&ai_mode_url](const Link& link) {
        return link.url == ai_mode_url;
      });
    }
    RemoveCustomLinksForPreinstalledApps();
  }

  base::RepeatingClosure callback =
      base::BindRepeating(&CustomLinksManagerImpl::OnPreferenceChanged,
                          weak_ptr_factory_.GetWeakPtr());
  pref_change_registrar_.Init(prefs_);
  pref_change_registrar_.Add(custom_links_initialized_pref_, callback);
  pref_change_registrar_.Add(store_.list_pref_name(), callback);
}

CustomLinksManagerImpl::~CustomLinksManagerImpl() = default;

bool CustomLinksManagerImpl::Initialize(const NTPTilesVector& tiles) {
  if (IsInitialized()) {
    return false;
  }

  for (const NTPTile& tile : tiles) {
    current_links_.emplace_back(Link{tile.url, tile.title, true});
  }
  if (enable_ai_mode_tile_) {
    InsertAiModeLinkIfNeeded(current_links_);
  }

  {
    base::AutoReset<bool> auto_reset(&updating_preferences_, true);
    prefs_->SetBoolean(custom_links_initialized_pref_, true);
  }
  StoreLinks();
  return true;
}

void CustomLinksManagerImpl::Uninitialize() {
  {
    base::AutoReset<bool> auto_reset(&updating_preferences_, true);
    prefs_->SetBoolean(custom_links_initialized_pref_, false);
  }
  ClearLinks();
}

bool CustomLinksManagerImpl::IsInitialized() const {
  return prefs_->GetBoolean(custom_links_initialized_pref_);
}

const std::vector<CustomLinksManager::Link>& CustomLinksManagerImpl::GetLinks()
    const {
  return current_links_;
}

size_t CustomLinksManagerImpl::GetMaxLinks() const {
  return max_links_;
}

int CustomLinksManagerImpl::GetAiModeTileIndex() const {
  return prefs_->GetInteger(prefs::kCustomLinksAiModeTileIndex);
}

void CustomLinksManagerImpl::SetAiModeTileIndex(int index) {
  prefs_->SetInteger(prefs::kCustomLinksAiModeTileIndex, index);
}

bool CustomLinksManagerImpl::AddLinkTo(const GURL& url,
                                       const std::u16string& title,
                                       size_t pos) {
  if (!IsInitialized() || !url.is_valid() ||
      current_links_.size() == max_links_) {
    return false;
  }

  if (custom_links_util::FindLinkWithUrl<Link>(current_links_, url) !=
      current_links_.end()) {
    return false;
  }

  pos = std::min(pos, current_links_.size());

  previous_links_ = current_links_;
  current_links_.insert(current_links_.begin() + pos, Link{url, title, false});
  StoreLinks();
  return true;
}

bool CustomLinksManagerImpl::AddLink(const GURL& url,
                                     const std::u16string& title) {
  return AddLinkTo(url, title, current_links_.size());
}

bool CustomLinksManagerImpl::UpdateLink(const GURL& url,
                                        const GURL& new_url,
                                        const std::u16string& new_title) {
  if (!IsInitialized() || !url.is_valid() ||
      (new_url.is_empty() && new_title.empty())) {
    return false;
  }

  // Do not update if |new_url| is invalid or already exists in the list.
  if (!new_url.is_empty() &&
      (!new_url.is_valid() ||
       custom_links_util::FindLinkWithUrl<Link>(current_links_, new_url) !=
           current_links_.end())) {
    return false;
  }

  // The virtual AI Mode link cannot be modified.
  if (url == GURL(kAiModeTileUrl)) {
    return false;
  }

  auto it = custom_links_util::FindLinkWithUrl<Link>(current_links_, url);
  if (it == current_links_.end()) {
    return false;
  }

  // At this point, we will be modifying at least one of the values.
  previous_links_ = current_links_;

  if (!new_url.is_empty()) {
    it->url = new_url;
  }
  if (!new_title.empty()) {
    it->title = new_title;
  }
  it->is_most_visited = false;

  StoreLinks();
  return true;
}

bool CustomLinksManagerImpl::ReorderLink(const GURL& url, size_t new_pos) {
  if (!IsInitialized()) {
    return false;
  }

  if (!custom_links_util::ReorderLink<Link>(current_links_, previous_links_,
                                            url, new_pos)) {
    return false;
  }

  StoreLinks();
  return true;
}

bool CustomLinksManagerImpl::DeleteLink(const GURL& url) {
  if (!IsInitialized() || !url.is_valid()) {
    return false;
  }

  auto it = custom_links_util::FindLinkWithUrl<Link>(current_links_, url);
  if (it == current_links_.end()) {
    return false;
  }

  if (enable_ai_mode_tile_ && url == GURL(kAiModeTileUrl)) {
    SetAiModeTileIndex(-1);
  }

  previous_links_ = current_links_;
  current_links_.erase(it);
  StoreLinks();
  return true;
}

bool CustomLinksManagerImpl::UndoAction() {
  if (!IsInitialized()) {
    return false;
  }

  if (!custom_links_util::UndoAction<Link>(current_links_, previous_links_)) {
    return false;
  }

  StoreLinks();
  return true;
}

void CustomLinksManagerImpl::InsertAiModeLinkIfNeeded(
    std::vector<Link>& links) {
  if (!enable_ai_mode_tile_) {
    return;
  }

  int index = GetAiModeTileIndex();
  if (index < 0) {
    return;
  }

  GURL ai_mode_url(kAiModeTileUrl);
  auto it = std::find_if(
      links.begin(), links.end(),
      [&ai_mode_url](const Link& link) { return link.url == ai_mode_url; });
  if (it != links.end()) {
    return;
  }

  size_t insert_pos = std::min(static_cast<size_t>(index), links.size());
  links.insert(
      links.begin() + insert_pos,
      Link{ai_mode_url, l10n_util::GetStringUTF16(IDS_NTP_TILES_AI_MODE_TITLE),
           /*is_most_visited=*/false});
}

void CustomLinksManagerImpl::ClearLinks() {
  {
    base::AutoReset<bool> auto_reset(&updating_preferences_, true);
    store_.ClearLinks();
  }
  current_links_.clear();
  previous_links_ = std::nullopt;
}

void CustomLinksManagerImpl::StoreLinks() {
  base::AutoReset<bool> auto_reset(&updating_preferences_, true);
  std::vector<Link> links_to_store = current_links_;
  GURL ai_mode_url(kAiModeTileUrl);
  auto it = std::find_if(
      links_to_store.begin(), links_to_store.end(),
      [&ai_mode_url](const Link& link) { return link.url == ai_mode_url; });
  if (it != links_to_store.end()) {
    if (enable_ai_mode_tile_) {
      SetAiModeTileIndex(std::distance(links_to_store.begin(), it));
    }
    links_to_store.erase(it);
  }
  store_.StoreLinks(links_to_store);
}

void CustomLinksManagerImpl::RemoveCustomLinksForPreinstalledApps() {
  if (!prefs_->GetBoolean(prefs::kCustomLinksForPreinstalledAppsRemoved)) {
    bool default_app_links_deleted = false;
    for (const Link& link : current_links_) {
      if (MostVisitedSites::IsNtpTileFromPreinstalledApp(link.url) &&
          MostVisitedSites::WasNtpAppMigratedToWebApp(prefs_, link.url)) {
        DeleteLink(link.url);
        default_app_links_deleted = true;
      }
    }
    if (default_app_links_deleted) {
      metrics::RecordsMigratedDefaultAppDeleted(TileType::kCustomLinks);
      prefs_->SetBoolean(prefs::kCustomLinksForPreinstalledAppsRemoved, true);
    }
  }
}

base::CallbackListSubscription
CustomLinksManagerImpl::RegisterCallbackForOnChanged(
    base::RepeatingClosure callback) {
  return closure_list_.Add(callback);
}

// history::HistoryServiceObserver implementation.
void CustomLinksManagerImpl::OnHistoryDeletions(
    history::HistoryService* history_service,
    const history::DeletionInfo& deletion_info) {
  // We don't care about expired entries.
  if (!IsInitialized() || deletion_info.is_from_expiration()) {
    return;
  }

  size_t initial_size = current_links_.size();
  if (deletion_info.IsAllHistory()) {
    std::erase_if(current_links_,
                  [](auto& link) { return link.is_most_visited; });
  } else {
    for (const history::URLRow& row : deletion_info.deleted_rows()) {
      auto it =
          custom_links_util::FindLinkWithUrl<Link>(current_links_, row.url());
      if (it != current_links_.end() && it->is_most_visited) {
        current_links_.erase(it);
      }
    }
  }
  StoreLinks();
  previous_links_ = std::nullopt;

  // Alert MostVisitedSites that some links have been deleted.
  if (initial_size != current_links_.size()) {
    closure_list_.Notify();
  }
}

void CustomLinksManagerImpl::HistoryServiceBeingDeleted(
    history::HistoryService* history_service) {
  DCHECK(history_service_observation_.IsObserving());
  history_service_observation_.Reset();
}

void CustomLinksManagerImpl::OnPreferenceChanged() {
  if (updating_preferences_) {
    return;
  }

  if (IsInitialized()) {
    current_links_ = store_.RetrieveLinks();
    if (enable_ai_mode_tile_) {
      InsertAiModeLinkIfNeeded(current_links_);
    } else {
      std::erase_if(current_links_, [](const Link& link) {
        return link.url == GURL(kAiModeTileUrl);
      });
    }
  } else {
    current_links_.clear();
  }
  previous_links_ = std::nullopt;
  closure_list_.Notify();
}

// static
void CustomLinksManagerImpl::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* user_prefs) {
  // Register both desktop and mobile pref keys so that sync works correctly
  // regardless of which NTP type is active.
  user_prefs->RegisterBooleanPref(
      prefs::kCustomLinksInitialized, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  user_prefs->RegisterBooleanPref(
      prefs::kCustomLinksInitializedMobile, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  user_prefs->RegisterBooleanPref(prefs::kCustomLinksForPreinstalledAppsRemoved,
                                  false);
  user_prefs->RegisterIntegerPref(prefs::kCustomLinksAiModeTileIndex, 0);
  CustomLinksStore::RegisterProfilePrefs(user_prefs);
}

}  // namespace ntp_tiles
