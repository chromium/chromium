// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/in_memory_url_index.h"

#include <cinttypes>
#include <memory>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "base/trace_event/trace_event.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/url_database.h"
#include "components/omnibox/browser/url_index_private_data.h"

using in_memory_url_index::InMemoryURLIndexCacheItem;

// Initializes a whitelist of URL schemes.
void InitializeSchemeWhitelist(
    SchemeSet* whitelist,
    const SchemeSet& client_schemes_to_whitelist) {
  DCHECK(whitelist);
  if (!whitelist->empty())
    return;  // Nothing to do, already initialized.

  whitelist->insert(client_schemes_to_whitelist.begin(),
                    client_schemes_to_whitelist.end());

  whitelist->insert(std::string(url::kAboutScheme));
  whitelist->insert(std::string(url::kFileScheme));
  whitelist->insert(std::string(url::kFtpScheme));
  whitelist->insert(std::string(url::kHttpScheme));
  whitelist->insert(std::string(url::kHttpsScheme));
  whitelist->insert(std::string(url::kMailToScheme));
}

// Restore/SaveCacheObserver ---------------------------------------------------

InMemoryURLIndex::RestoreCacheObserver::~RestoreCacheObserver() {
}

InMemoryURLIndex::SaveCacheObserver::~SaveCacheObserver() {
}

// RebuildPrivateDataFromHistoryDBTask -----------------------------------------

InMemoryURLIndex::RebuildPrivateDataFromHistoryDBTask::
    RebuildPrivateDataFromHistoryDBTask(
        InMemoryURLIndex* index,
        const SchemeSet& scheme_whitelist)
    : index_(index),
      scheme_whitelist_(scheme_whitelist),
      succeeded_(false) {
}

bool InMemoryURLIndex::RebuildPrivateDataFromHistoryDBTask::RunOnDBThread(
    history::HistoryBackend* backend,
    history::HistoryDatabase* db) {
  data_ = URLIndexPrivateData::RebuildFromHistory(db, scheme_whitelist_);
  succeeded_ = data_.get() && !data_->Empty();
  if (!succeeded_ && data_.get())
    data_->Clear();
  return true;
}

void InMemoryURLIndex::RebuildPrivateDataFromHistoryDBTask::
    DoneRunOnMainThread() {
  index_->DoneRebuidingPrivateDataFromHistoryDB(succeeded_, data_);
}

InMemoryURLIndex::RebuildPrivateDataFromHistoryDBTask::
    ~RebuildPrivateDataFromHistoryDBTask() {
}

// InMemoryURLIndex ------------------------------------------------------------

InMemoryURLIndex::InMemoryURLIndex(bookmarks::BookmarkModel* bookmark_model,
                                   history::HistoryService* history_service,
                                   TemplateURLService* template_url_service,
                                   const base::FilePath& history_dir,
                                   const SchemeSet& client_schemes_to_whitelist)
    : bookmark_model_(bookmark_model),
      history_service_(history_service),
      template_url_service_(template_url_service),
      history_dir_(history_dir),
      private_data_(new URLIndexPrivateData),
      restore_cache_observer_(nullptr),
      save_cache_observer_(nullptr),
      task_runner_(
          base::CreateSequencedTaskRunner({base::ThreadPool(), base::MayBlock(),
                                           base::TaskPriority::BEST_EFFORT})),
      shutdown_(false),
      restored_(false),
      needs_to_be_cached_(false),
      listen_to_history_service_loaded_(false) {
  InitializeSchemeWhitelist(&scheme_whitelist_, client_schemes_to_whitelist);
  // TODO(mrossetti): Register for language change notifications.
  if (history_service_)
    history_service_->AddObserver(this);

  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      this, "InMemoryURLIndex", base::ThreadTaskRunnerHandle::Get());
}

InMemoryURLIndex::~InMemoryURLIndex() {
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);

  // If there was a history directory (which there won't be for some unit tests)
  // then insure that the cache has already been saved.
  DCHECK(history_dir_.empty() || !needs_to_be_cached_);
  DCHECK(!history_service_);
  DCHECK(shutdown_);
}

void InMemoryURLIndex::Init() {
  PostRestoreFromCacheFileTask();
}

void InMemoryURLIndex::ClearPrivateData() {
  private_data_->Clear();
}

bool InMemoryURLIndex::GetCacheFilePath(base::FilePath* file_path) {
  if (history_dir_.empty())
    return false;
  *file_path = history_dir_.Append(FILE_PATH_LITERAL("History Provider Cache"));
  return true;
}

// Querying --------------------------------------------------------------------

ScoredHistoryMatches InMemoryURLIndex::HistoryItemsForTerms(
    const base::string16& term_string,
    size_t cursor_position,
    size_t max_matches) {
  return private_data_->HistoryItemsForTerms(
      term_string, cursor_position, max_matches, bookmark_model_,
      template_url_service_);
}

// Updating --------------------------------------------------------------------

void InMemoryURLIndex::DeleteURL(const GURL& url) {
  private_data_->DeleteURL(url);
}

void InMemoryURLIndex::OnURLVisited(history::HistoryService* history_service,
                                    ui::PageTransition transition,
                                    const history::URLRow& row,
                                    const history::RedirectList& redirects,
                                    base::Time visit_time) {
  DCHECK_EQ(history_service_, history_service);
  needs_to_be_cached_ |= private_data_->UpdateURL(history_service_,
                                                  row,
                                                  scheme_whitelist_,
                                                  &private_data_tracker_);
}

void InMemoryURLIndex::OnURLsModified(history::HistoryService* history_service,
                                      const history::URLRows& changed_urls) {
  DCHECK_EQ(history_service_, history_service);
  for (const auto& row : changed_urls) {
    needs_to_be_cached_ |= private_data_->UpdateURL(history_service_,
                                                    row,
                                                    scheme_whitelist_,
                                                    &private_data_tracker_);
  }
}

void InMemoryURLIndex::OnURLsDeleted(
    history::HistoryService* history_service,
    const history::DeletionInfo& deletion_info) {
  if (deletion_info.IsAllHistory()) {
    ClearPrivateData();
    needs_to_be_cached_ = true;
  } else {
    for (const auto& row : deletion_info.deleted_rows())
      needs_to_be_cached_ |= private_data_->DeleteURL(row.url());
  }
  // If we made changes, destroy the previous cache.  Otherwise, if we go
  // through an unclean shutdown (and therefore fail to write a new cache file),
  // when Chrome restarts and we restore from the previous cache, we'll end up
  // searching over URLs that may be deleted.  This would be wrong, and
  // surprising to the user who bothered to delete some URLs from their
  // history.  In this situation, deleting the cache is a better solution than
  // writing a new cache (after deleting the URLs from the in-memory structure)
  // because deleting the cache forces it to be rebuilt from history upon
  // startup.  If we instead write a new, updated cache then at the time of next
  // startup (after an unclean shutdown) we will not rebuild the in-memory data
  // structures from history but rather use the cache.  This solution is
  // mediocre because this cache may not have the most-recently-visited URLs
  // in it (URLs visited after user deleted some URLs from history), which
  // would be odd and confusing.  It's better to force a rebuild.
  base::FilePath path;
  if (needs_to_be_cached_ && GetCacheFilePath(&path))
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(base::IgnoreResult(base::DeleteFile), path, false));
}

void InMemoryURLIndex::OnHistoryServiceLoaded(
    history::HistoryService* history_service) {
  if (listen_to_history_service_loaded_)
    ScheduleRebuildFromHistory();
  listen_to_history_service_loaded_ = false;
}

bool InMemoryURLIndex::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* process_memory_dump) {
  size_t res = 0;

  res += base::trace_event::EstimateMemoryUsage(scheme_whitelist_);

  // TODO(dyaroshev): Add support for scoped_refptr in
  //                  base::trace_event::EstimateMemoryUsage.
  res += sizeof(URLIndexPrivateData) + private_data_->EstimateMemoryUsage();

  const std::string dump_name =
      base::StringPrintf("omnibox/in_memory_url_index/0x%" PRIXPTR,
                         reinterpret_cast<uintptr_t>(this));
  auto* dump = process_memory_dump->CreateAllocatorDump(dump_name);
  dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                  base::trace_event::MemoryAllocatorDump::kUnitsBytes, res);
  return true;
}

// Restoring from Cache --------------------------------------------------------

void InMemoryURLIndex::PostRestoreFromCacheFileTask() {
  DCHECK(thread_checker_.CalledOnValidThread());
  TRACE_EVENT0("browser", "InMemoryURLIndex::PostRestoreFromCacheFileTask");

  base::FilePath path;
  if (!GetCacheFilePath(&path) || shutdown_) {
    restored_ = true;
    if (restore_cache_observer_)
      restore_cache_observer_->OnCacheRestoreFinished(false);
    return;
  }

  base::PostTaskAndReplyWithResult(
      task_runner_.get(), FROM_HERE,
      base::BindOnce(&URLIndexPrivateData::RestoreFromFile, path),
      base::BindOnce(&InMemoryURLIndex::OnCacheLoadDone, AsWeakPtr()));
}

void InMemoryURLIndex::OnCacheLoadDone(
    scoped_refptr<URLIndexPrivateData> private_data) {
  if (private_data.get() && !private_data->Empty()) {
    private_data_tracker_.TryCancelAll();
    private_data_ = private_data;
    restored_ = true;
    if (restore_cache_observer_)
      restore_cache_observer_->OnCacheRestoreFinished(true);
  } else if (history_service_) {
    // When unable to restore from the cache file delete the cache file, if
    // it exists, and then rebuild from the history database if it's available,
    // otherwise wait until the history database loaded and then rebuild.
    base::FilePath path;
    if (!GetCacheFilePath(&path) || shutdown_)
      return;
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(base::IgnoreResult(base::DeleteFile), path, false));
    if (history_service_->backend_loaded()) {
      ScheduleRebuildFromHistory();
    } else {
      listen_to_history_service_loaded_ = true;
    }
  }
}

// Cleanup ---------------------------------------------------------------------

void InMemoryURLIndex::Shutdown() {
  if (history_service_) {
    history_service_->RemoveObserver(this);
    history_service_ = nullptr;
  }
  cache_reader_tracker_.TryCancelAll();
  shutdown_ = true;
  base::FilePath path;
  if (!GetCacheFilePath(&path))
    return;
  private_data_tracker_.TryCancelAll();
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(base::IgnoreResult(
                         &URLIndexPrivateData::WritePrivateDataToCacheFileTask),
                     private_data_, path));
  needs_to_be_cached_ = false;
}

// Restoring from the History DB -----------------------------------------------

void InMemoryURLIndex::ScheduleRebuildFromHistory() {
  DCHECK(history_service_);
  history_service_->ScheduleDBTask(
      FROM_HERE,
      std::unique_ptr<history::HistoryDBTask>(
          new InMemoryURLIndex::RebuildPrivateDataFromHistoryDBTask(
              this, scheme_whitelist_)),
      &cache_reader_tracker_);
}

void InMemoryURLIndex::DoneRebuidingPrivateDataFromHistoryDB(
    bool succeeded,
    scoped_refptr<URLIndexPrivateData> private_data) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (succeeded) {
    private_data_tracker_.TryCancelAll();
    private_data_ = private_data;
    PostSaveToCacheFileTask();  // Cache the newly rebuilt index.
  } else {
    private_data_->Clear();  // Dump the old private data.
    // There is no need to do anything with the cache file as it was deleted
    // when the rebuild from the history operation was kicked off.
  }
  restored_ = true;
  if (restore_cache_observer_)
    restore_cache_observer_->OnCacheRestoreFinished(succeeded);
}

void InMemoryURLIndex::RebuildFromHistory(
    history::HistoryDatabase* history_db) {
  private_data_tracker_.TryCancelAll();
  private_data_ = URLIndexPrivateData::RebuildFromHistory(history_db,
                                                          scheme_whitelist_);
}

// Saving to Cache -------------------------------------------------------------

void InMemoryURLIndex::PostSaveToCacheFileTask() {
  base::FilePath path;
  if (!GetCacheFilePath(&path))
    return;
  // If there is anything in our private data then make a copy of it and tell
  // it to save itself to a file.
  if (private_data_.get() && !private_data_->Empty()) {
    // Note that ownership of the copy of our private data is passed to the
    // completion closure below.
    scoped_refptr<URLIndexPrivateData> private_data_copy =
        private_data_->Duplicate();
    base::PostTaskAndReplyWithResult(
        task_runner_.get(), FROM_HERE,
        base::BindOnce(&URLIndexPrivateData::WritePrivateDataToCacheFileTask,
                       private_data_copy, path),
        base::BindOnce(&InMemoryURLIndex::OnCacheSaveDone, AsWeakPtr()));
  } else {
    // If there is no data in our index then delete any existing cache file.
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(base::IgnoreResult(base::DeleteFile), path, false));
  }
}

void InMemoryURLIndex::OnCacheSaveDone(bool succeeded) {
  if (save_cache_observer_)
    save_cache_observer_->OnCacheSaveFinished(succeeded);
}
