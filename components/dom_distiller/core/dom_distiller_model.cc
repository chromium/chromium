// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/dom_distiller_model.h"

#include <utility>

using syncer::SyncChange;
using syncer::SyncChangeList;
using syncer::SyncData;
using syncer::SyncDataList;

namespace dom_distiller {

DomDistillerModel::DomDistillerModel()
    : next_key_(1) {}

DomDistillerModel::DomDistillerModel(
    const std::vector<ArticleEntry>& initial_data)
    : next_key_(1) {
  for (size_t i = 0; i < initial_data.size(); ++i) {
    AddEntry(initial_data[i]);
  }
}

DomDistillerModel::~DomDistillerModel() {}

bool DomDistillerModel::GetEntryById(const std::string& entry_id,
                                     ArticleEntry* entry) const {
  KeyType key = 0;
  if (!GetKeyById(entry_id, &key)) {
    return false;
  }
  GetEntryByKey(key, entry);
  return true;
}

bool DomDistillerModel::GetEntryByUrl(const GURL& url,
                                     ArticleEntry* entry) const {
  KeyType key = 0;
  if (!GetKeyByUrl(url, &key)) {
    return false;
  }
  GetEntryByKey(key, entry);
  return true;
}

bool DomDistillerModel::GetKeyById(const std::string& entry_id,
                                   KeyType* key) const {
  auto it = entry_id_to_key_map_.find(entry_id);
  if (it == entry_id_to_key_map_.end()) {
    return false;
  }
  if (key != nullptr) {
    *key = it->second;
  }
  return true;
}

bool DomDistillerModel::GetKeyByUrl(const GURL& url, KeyType* key) const {
  auto it = url_to_key_map_.find(url.spec());
  if (it == url_to_key_map_.end()) {
    return false;
  }
  if (key != nullptr) {
    *key = it->second;
  }
  return true;
}

void DomDistillerModel::GetEntryByKey(KeyType key, ArticleEntry* entry) const {
  if (entry != nullptr) {
    auto it = entries_.find(key);
    DCHECK(it != entries_.end());
    *entry = it->second;
  }
}

size_t DomDistillerModel::GetNumEntries() const {
  return entries_.size();
}

std::vector<ArticleEntry> DomDistillerModel::GetEntries() const {
  std::vector<ArticleEntry> entries_list;
  for (auto it = entries_.begin(); it != entries_.end(); ++it) {
    entries_list.push_back(it->second);
  }
  return entries_list;
}

SyncDataList DomDistillerModel::GetAllSyncData() const {
  SyncDataList data;
  for (auto it = entries_.begin(); it != entries_.end(); ++it) {
    data.push_back(CreateLocalData(it->second));
  }
  return data;
}

void DomDistillerModel::CalculateChangesForMerge(
    const SyncDataList& data,
    SyncChangeList* changes_to_apply,
    SyncChangeList* changes_missing) {
  typedef base::hash_set<std::string> StringSet;
  StringSet entries_to_change;
  for (auto it = data.begin(); it != data.end(); ++it) {
    std::string entry_id = GetEntryIdFromSyncData(*it);
    std::pair<StringSet::iterator, bool> insert_result =
        entries_to_change.insert(entry_id);

    DCHECK(insert_result.second);

    SyncChange::SyncChangeType change_type = SyncChange::ACTION_ADD;
    if (GetEntryById(entry_id, nullptr)) {
      change_type = SyncChange::ACTION_UPDATE;
    }
    changes_to_apply->push_back(SyncChange(FROM_HERE, change_type, *it));
  }

  for (EntryMap::const_iterator it = entries_.begin(); it != entries_.end();
       ++it) {
    if (entries_to_change.find(it->second.entry_id()) ==
        entries_to_change.end()) {
      changes_missing->push_back(SyncChange(
          FROM_HERE, SyncChange::ACTION_ADD, CreateLocalData(it->second)));
    }
  }
}

void DomDistillerModel::ApplyChangesToModel(
    const SyncChangeList& changes,
    SyncChangeList* changes_applied,
    SyncChangeList* changes_missing) {
  DCHECK(changes_applied);
  DCHECK(changes_missing);

  for (auto it = changes.begin(); it != changes.end(); ++it) {
    ApplyChangeToModel(*it, changes_applied, changes_missing);
  }
}

void DomDistillerModel::AddEntry(const ArticleEntry& entry) {
  const std::string& entry_id = entry.entry_id();
  KeyType key = next_key_++;
  DCHECK(!GetKeyById(entry_id, nullptr));
  entries_.insert(std::make_pair(key, entry));
  entry_id_to_key_map_.insert(std::make_pair(entry_id, key));
  for (int i = 0; i < entry.pages_size(); ++i) {
    url_to_key_map_.insert(std::make_pair(entry.pages(i).url(), key));
  }
}

void DomDistillerModel::RemoveEntry(const ArticleEntry& entry) {
  const std::string& entry_id = entry.entry_id();
  KeyType key = 0;
  bool success = GetKeyById(entry_id, &key);
  DCHECK(success);

  entries_.erase(key);
  entry_id_to_key_map_.erase(entry_id);
  for (int i = 0; i < entry.pages_size(); ++i) {
    url_to_key_map_.erase(entry.pages(i).url());
  }
}

void DomDistillerModel::ApplyChangeToModel(
    const SyncChange& change,
    SyncChangeList* changes_applied,
    SyncChangeList* changes_missing) {
  DCHECK(change.IsValid());
  DCHECK(changes_applied);
  DCHECK(changes_missing);

  const std::string& entry_id = GetEntryIdFromSyncData(change.sync_data());

  if (change.change_type() == SyncChange::ACTION_DELETE) {
    ArticleEntry current_entry;
    if (GetEntryById(entry_id, &current_entry)) {
      RemoveEntry(current_entry);
      changes_applied->push_back(SyncChange(
          change.location(), SyncChange::ACTION_DELETE, change.sync_data()));
    }
    // If we couldn't find in sync db, we were deleting anyway so swallow the
    // error.
    return;
  }

  ArticleEntry entry = GetEntryFromChange(change);
  ArticleEntry current_entry;
  if (!GetEntryById(entry_id, &current_entry)) {
    AddEntry(entry);
    changes_applied->push_back(SyncChange(
        change.location(), SyncChange::ACTION_ADD, change.sync_data()));
  } else {
    if (!AreEntriesEqual(current_entry, entry)) {
      // Currently, conflicts are simply resolved by accepting the last one to
      // arrive.
      RemoveEntry(current_entry);
      AddEntry(entry);
      changes_applied->push_back(SyncChange(
          change.location(), SyncChange::ACTION_UPDATE, change.sync_data()));
    }
  }
}

}  // namespace dom_distiller
