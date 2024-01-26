// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/common/download_db_cache.h"

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "components/download/database/download_db.h"
#include "components/download/database/download_db_conversions.h"
#include "components/download/database/download_db_entry.h"
#include "components/download/public/common/download_features.h"
#include "components/download/public/common/download_stats.h"
#include "components/download/public/common/download_utils.h"

namespace download {

namespace {

// Interval between download DB updates.
// TODO(qinmin): make this finch configurable.
const int kUpdateDBIntervalMs = 10000;

enum class ShouldUpdateDownloadDBResult {
  NO_UPDATE,
  UPDATE,
  UPDATE_IMMEDIATELY,
};

ShouldUpdateDownloadDBResult ShouldUpdateDownloadDB(
    std::optional<DownloadDBEntry> previous,
    const DownloadDBEntry& current) {
  if (!previous)
    return ShouldUpdateDownloadDBResult::UPDATE_IMMEDIATELY;

  std::optional<InProgressInfo> previous_info;
  if (previous->download_info)
    previous_info = previous->download_info->in_progress_info;
  base::FilePath previous_path =
      previous_info ? previous_info->current_path : base::FilePath();

  bool previous_paused = previous_info ? previous_info->paused : false;

  std::optional<InProgressInfo> current_info;
  if (current.download_info)
    current_info = current.download_info->in_progress_info;

  base::FilePath current_path;
  bool paused = false;
  GURL url;
  DownloadItem::DownloadState state = DownloadItem::DownloadState::IN_PROGRESS;
  DownloadInterruptReason interrupt_reason = DOWNLOAD_INTERRUPT_REASON_NONE;
  if (current_info) {
    current_path = current_info->current_path;
    paused = current_info->paused;
    if (!current_info->url_chain.empty())
      url = current_info->url_chain.back();
    state = current_info->state;
    interrupt_reason = current_info->interrupt_reason;
  }

  // When download path is determined, Chrome should commit the history
  // immediately. Otherwise the file will be left permanently on the external
  // storage if Chrome crashes right away.
  if (current_path != previous_path || paused != previous_paused) {
    return ShouldUpdateDownloadDBResult::UPDATE_IMMEDIATELY;
  }

  if (previous.value() == current)
    return ShouldUpdateDownloadDBResult::NO_UPDATE;

  if (IsDownloadDone(url, state, interrupt_reason))
    return ShouldUpdateDownloadDBResult::UPDATE_IMMEDIATELY;

  return ShouldUpdateDownloadDBResult::UPDATE;
}

void CleanUpInProgressEntry(DownloadDBEntry* entry) {
  if (!entry->download_info)
    return;

  std::optional<InProgressInfo>& in_progress_info =
      entry->download_info->in_progress_info;
  if (!in_progress_info)
    return;

  if (in_progress_info->state == DownloadItem::DownloadState::IN_PROGRESS) {
    in_progress_info->state = DownloadItem::DownloadState::INTERRUPTED;
    in_progress_info->interrupt_reason =
        download::DOWNLOAD_INTERRUPT_REASON_CRASH;
    // We should not trust the hash value for crashed in-progress download, as
    // hash is not calculated for when download is in progress.
    in_progress_info->hash = std::string();
  }
}

void OnDownloadDBUpdated(bool success) {
  // TODO(qinmin): handle the case that update fails.
  if (!success)
    LOG(ERROR) << "Unable to update DB entries";
}

// Check if a DownloadDBEntry represents an in progress download.
bool IsInProgressEntry(std::optional<DownloadDBEntry> entry) {
  if (!entry || !entry->download_info ||
      !entry->download_info->in_progress_info)
    return false;

  return entry->download_info->in_progress_info->state ==
         DownloadItem::DownloadState::IN_PROGRESS;
}

}  // namespace

DownloadDBCache::DownloadDBCache(std::unique_ptr<DownloadDB> download_db)
    : initialized_(false), download_db_(std::move(download_db)) {
  DCHECK(download_db_);
}

DownloadDBCache::~DownloadDBCache() = default;

void DownloadDBCache::Initialize(InitializeCallback callback) {
  DCHECK(!initialized_);
  download_db_->Initialize(
      base::BindOnce(&DownloadDBCache::OnDownloadDBInitialized,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

std::optional<DownloadDBEntry> DownloadDBCache::RetrieveEntry(
    const std::string& guid) {
  auto iter = cached_entries_.find(guid);
  if (iter != cached_entries_.end())
    return iter->second;
  return std::nullopt;
}

void DownloadDBCache::AddOrReplaceEntry(const DownloadDBEntry& entry) {
  if (!entry.download_info)
    return;
  const std::string& guid = entry.download_info->guid;
  ShouldUpdateDownloadDBResult result =
      ShouldUpdateDownloadDB(RetrieveEntry(guid), entry);
  if (result == ShouldUpdateDownloadDBResult::NO_UPDATE)
    return;
  if (!update_timer_.IsRunning() &&
      result == ShouldUpdateDownloadDBResult::UPDATE) {
    update_timer_.Start(FROM_HERE, base::Milliseconds(kUpdateDBIntervalMs),
                        this, &DownloadDBCache::UpdateDownloadDB);
  }

  cached_entries_[guid] = entry;
  updated_guids_.emplace(guid);
  if (result == ShouldUpdateDownloadDBResult::UPDATE_IMMEDIATELY) {
    UpdateDownloadDB();
    update_timer_.Stop();
  }
}

void DownloadDBCache::RemoveEntry(const std::string& guid) {
  cached_entries_.erase(guid);
  updated_guids_.erase(guid);
  if (initialized_)
    download_db_->Remove(guid);
}

void DownloadDBCache::UpdateDownloadDB() {
  if (updated_guids_.empty())
    return;

  std::vector<DownloadDBEntry> entries;
  for (const auto& guid : updated_guids_) {
    std::optional<DownloadDBEntry> entry = RetrieveEntry(guid);
    DCHECK(entry);
    entries.emplace_back(entry.value());
    // If the entry is no longer in-progress, remove it from the cache as it may
    // not update again soon.
    if (!IsInProgressEntry(entry))
      cached_entries_.erase(guid);
  }
  updated_guids_.clear();
  if (initialized_) {
    download_db_->AddOrReplaceEntries(entries,
                                      base::BindOnce(&OnDownloadDBUpdated));
  }
}

void DownloadDBCache::OnDownloadUpdated(DownloadItem* download) {
  DownloadDBEntry entry = CreateDownloadDBEntryFromItem(
      *(static_cast<DownloadItemImpl*>(download)));
  AddOrReplaceEntry(entry);
}

void DownloadDBCache::OnDownloadRemoved(DownloadItem* download) {
  RemoveEntry(download->GetGuid());
}

void DownloadDBCache::OnDownloadDBInitialized(
    InitializeCallback callback,
    bool success) {
  if (success) {
    download_db_->LoadEntries(
        base::BindOnce(&DownloadDBCache::OnDownloadDBEntriesLoaded,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  } else {
    std::move(callback).Run(false,
                            std::make_unique<std::vector<DownloadDBEntry>>());
  }
}

void DownloadDBCache::OnDownloadDBEntriesLoaded(
    InitializeCallback callback,
    bool success,
    std::unique_ptr<std::vector<DownloadDBEntry>> entries) {
  initialized_ = success;
  for (auto& entry : *entries) {
    // If the entry is from the metadata cache migration, just remove it from
    // DB as the data is not being cleaned up properly.
    if (entry.download_info->id < 0)
      RemoveEntry(entry.download_info->guid);
    else
      CleanUpInProgressEntry(&entry);
  }
  std::move(callback).Run(success, std::move(entries));
}

void DownloadDBCache::SetTimerTaskRunnerForTesting(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  update_timer_.SetTaskRunner(task_runner);
}

}  //  namespace download
