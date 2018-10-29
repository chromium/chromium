// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/dom_distiller_store.h"

#include <stddef.h>

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/dom_distiller/core/article_entry.h"
#include "components/sync/model/sync_change.h"
#include "components/sync/protocol/article_specifics.pb.h"
#include "components/sync/protocol/sync.pb.h"

using leveldb_proto::ProtoDatabase;
using sync_pb::ArticleSpecifics;
using sync_pb::EntitySpecifics;
using syncer::ModelType;
using syncer::SyncChange;
using syncer::SyncChangeList;
using syncer::SyncData;
using syncer::SyncDataList;
using syncer::SyncError;
using syncer::SyncMergeResult;

namespace {
// Statistics are logged to UMA with this string as part of histogram name. They
// can all be found under LevelDB.*.DomDistillerStore. Changing this needs to
// synchronize with histograms.xml, AND will also become incompatible with older
// browsers still reporting the previous values.
const char kDatabaseUMAClientName[] = "DomDistillerStore";
}

namespace dom_distiller {

DomDistillerStore::DomDistillerStore(
    std::unique_ptr<ProtoDatabase<ArticleEntry>> database,
    const base::FilePath& database_dir)
    : database_(std::move(database)),
      database_loaded_(false),
      weak_ptr_factory_(this) {
  database_->Init(kDatabaseUMAClientName, database_dir,
                  leveldb_proto::CreateSimpleOptions(),
                  base::BindOnce(&DomDistillerStore::OnDatabaseInit,
                                 weak_ptr_factory_.GetWeakPtr()));
}

DomDistillerStore::DomDistillerStore(
    std::unique_ptr<ProtoDatabase<ArticleEntry>> database,
    const std::vector<ArticleEntry>& initial_data,
    const base::FilePath& database_dir)
    : database_(std::move(database)),
      database_loaded_(false),
      model_(initial_data),
      weak_ptr_factory_(this) {
  database_->Init(kDatabaseUMAClientName, database_dir,
                  leveldb_proto::CreateSimpleOptions(),
                  base::BindOnce(&DomDistillerStore::OnDatabaseInit,
                                 weak_ptr_factory_.GetWeakPtr()));
}

DomDistillerStore::~DomDistillerStore() {}

bool DomDistillerStore::GetEntryById(const std::string& entry_id,
                                     ArticleEntry* entry) {
  return model_.GetEntryById(entry_id, entry);
}

bool DomDistillerStore::GetEntryByUrl(const GURL& url, ArticleEntry* entry) {
  return model_.GetEntryByUrl(url, entry);
}

bool DomDistillerStore::AddEntry(const ArticleEntry& entry) {
  return ChangeEntry(entry, SyncChange::ACTION_ADD);
}

bool DomDistillerStore::UpdateEntry(const ArticleEntry& entry) {
  return ChangeEntry(entry, SyncChange::ACTION_UPDATE);
}

bool DomDistillerStore::RemoveEntry(const ArticleEntry& entry) {
  return ChangeEntry(entry, SyncChange::ACTION_DELETE);
}

bool DomDistillerStore::ChangeEntry(const ArticleEntry& entry,
                                    SyncChange::SyncChangeType changeType) {
  if (!database_loaded_) {
    return false;
  }

  bool hasEntry = model_.GetEntryById(entry.entry_id(), nullptr);
  if (hasEntry) {
    if (changeType == SyncChange::ACTION_ADD) {
      DVLOG(1) << "Already have entry with id " << entry.entry_id() << ".";
      return false;
    }
  } else if (changeType != SyncChange::ACTION_ADD) {
    DVLOG(1) << "No entry with id " << entry.entry_id() << " found.";
    return false;
  }

  SyncChangeList changes_to_apply;
  changes_to_apply.push_back(
      SyncChange(FROM_HERE, changeType, CreateLocalData(entry)));

  SyncChangeList changes_applied;
  SyncChangeList changes_missing;

  ApplyChangesToModel(changes_to_apply, &changes_applied, &changes_missing);

  if (changeType == SyncChange::ACTION_UPDATE && changes_applied.size() != 1) {
    DVLOG(1) << "Failed to update entry with id " << entry.entry_id() << ".";
    return false;
  }

  DCHECK_EQ(size_t(0), changes_missing.size());
  DCHECK_EQ(size_t(1), changes_applied.size());

  ApplyChangesToDatabase(changes_applied);

  return true;
}

void DomDistillerStore::AddObserver(DomDistillerObserver* observer) {
  observers_.AddObserver(observer);
}

void DomDistillerStore::RemoveObserver(DomDistillerObserver* observer) {
  observers_.RemoveObserver(observer);
}

std::vector<ArticleEntry> DomDistillerStore::GetEntries() const {
  return model_.GetEntries();
}

void DomDistillerStore::NotifyObservers(const syncer::SyncChangeList& changes) {
  if (observers_.might_have_observers() && changes.size() > 0) {
    std::vector<DomDistillerObserver::ArticleUpdate> article_changes;
    for (auto it = changes.begin(); it != changes.end(); ++it) {
      DomDistillerObserver::ArticleUpdate article_update;
      switch (it->change_type()) {
        case SyncChange::ACTION_ADD:
          article_update.update_type = DomDistillerObserver::ArticleUpdate::ADD;
          break;
        case SyncChange::ACTION_UPDATE:
          article_update.update_type =
              DomDistillerObserver::ArticleUpdate::UPDATE;
          break;
        case SyncChange::ACTION_DELETE:
          article_update.update_type =
              DomDistillerObserver::ArticleUpdate::REMOVE;
          break;
        case SyncChange::ACTION_INVALID:
          NOTREACHED();
          break;
      }
      const ArticleEntry& entry = GetEntryFromChange(*it);
      article_update.entry_id = entry.entry_id();
      article_changes.push_back(article_update);
    }
    for (DomDistillerObserver& observer : observers_)
      observer.ArticleEntriesUpdated(article_changes);
  }
}

void DomDistillerStore::ApplyChangesToModel(const SyncChangeList& changes,
                                            SyncChangeList* changes_applied,
                                            SyncChangeList* changes_missing) {
  model_.ApplyChangesToModel(changes, changes_applied, changes_missing);
  NotifyObservers(*changes_applied);
}

void DomDistillerStore::OnDatabaseInit(bool success) {
  if (!success) {
    DVLOG(1) << "DOM Distiller database init failed.";
    database_.reset();
    return;
  }
  database_->LoadEntries(base::BindOnce(&DomDistillerStore::OnDatabaseLoad,
                                        weak_ptr_factory_.GetWeakPtr()));
}

void DomDistillerStore::OnDatabaseLoad(bool success,
                                       std::unique_ptr<EntryVector> entries) {
  if (!success) {
    DVLOG(1) << "DOM Distiller database load failed.";
    database_.reset();
    return;
  }
  database_loaded_ = true;

  SyncDataList data;
  for (auto it = entries->begin(); it != entries->end(); ++it) {
    data.push_back(CreateLocalData(*it));
  }
  SyncChangeList changes_applied;
  SyncChangeList database_changes_needed;
  MergeDataWithModel(data, &changes_applied, &database_changes_needed);
  ApplyChangesToDatabase(database_changes_needed);
}

void DomDistillerStore::OnDatabaseSave(bool success) {
  if (!success) {
    DVLOG(1) << "DOM Distiller database save failed."
             << " Disabling modifications and sync.";
    database_.reset();
    database_loaded_ = false;
  }
}

bool DomDistillerStore::ApplyChangesToDatabase(
    const SyncChangeList& change_list) {
  if (!database_loaded_) {
    return false;
  }
  if (change_list.empty()) {
    return true;
  }
  std::unique_ptr<ProtoDatabase<ArticleEntry>::KeyEntryVector> entries_to_save(
      new ProtoDatabase<ArticleEntry>::KeyEntryVector());
  std::unique_ptr<std::vector<std::string>> keys_to_remove(
      new std::vector<std::string>());

  for (auto it = change_list.begin(); it != change_list.end(); ++it) {
    if (it->change_type() == SyncChange::ACTION_DELETE) {
      ArticleEntry entry = GetEntryFromChange(*it);
      keys_to_remove->push_back(entry.entry_id());
    } else {
      ArticleEntry entry = GetEntryFromChange(*it);
      entries_to_save->push_back(std::make_pair(entry.entry_id(), entry));
    }
  }
  database_->UpdateEntries(std::move(entries_to_save),
                           std::move(keys_to_remove),
                           base::BindOnce(&DomDistillerStore::OnDatabaseSave,
                                          weak_ptr_factory_.GetWeakPtr()));
  return true;
}

SyncMergeResult DomDistillerStore::MergeDataWithModel(
    const SyncDataList& data, SyncChangeList* changes_applied,
    SyncChangeList* changes_missing) {
  // TODO(cjhopman): This naive merge algorithm could cause flip-flopping
  // between database/sync of multiple clients.
  DCHECK(changes_applied);
  DCHECK(changes_missing);

  SyncMergeResult result(syncer::DEPRECATED_ARTICLES);
  result.set_num_items_before_association(model_.GetNumEntries());

  SyncChangeList changes_to_apply;
  model_.CalculateChangesForMerge(data, &changes_to_apply, changes_missing);
  SyncError error;
  ApplyChangesToModel(changes_to_apply, changes_applied, changes_missing);

  int num_added = 0;
  int num_modified = 0;
  for (SyncChangeList::const_iterator it = changes_applied->begin();
       it != changes_applied->end(); ++it) {
    DCHECK(it->IsValid());
    switch (it->change_type()) {
      case SyncChange::ACTION_ADD:
        num_added++;
        break;
      case SyncChange::ACTION_UPDATE:
        num_modified++;
        break;
      default:
        NOTREACHED();
    }
  }
  result.set_num_items_added(num_added);
  result.set_num_items_modified(num_modified);
  result.set_num_items_deleted(0);

  result.set_pre_association_version(0);
  result.set_num_items_after_association(model_.GetNumEntries());
  result.set_error(error);

  return result;
}

}  // namespace dom_distiller
