// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_READING_LIST_CORE_READING_LIST_MODEL_H_
#define COMPONENTS_READING_LIST_CORE_READING_LIST_MODEL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/reading_list/core/reading_list_entry.h"
#include "google_apis/gaia/core_account_id.h"

class GURL;
class ReadingListModelObserver;

namespace base {
class Location;
}  // namespace base

namespace syncer {
class DataTypeControllerDelegate;
}  // namespace syncer

// The reading list model contains two list of entries: one of unread urls, the
// other of read ones. This object should only be accessed from one thread
// (Usually the main thread). The observers callbacks are also sent on the main
// thread.
class ReadingListModel : public KeyedService {
 public:
  class ScopedReadingListBatchUpdate;

  ReadingListModel() = default;
  ~ReadingListModel() override = default;

  ReadingListModel(const ReadingListModel&) = delete;
  ReadingListModel& operator=(const ReadingListModel&) = delete;

  // Returns true if the model finished loading. Until this returns true the
  // reading list is not ready for use.
  virtual bool loaded() const = 0;

  // Returns the delegate responsible for integrating with sync. This
  // corresponds to the regular sync mode, rather than transport-only sync (i.e.
  // the user opted into sync-the-feature).
  virtual base::WeakPtr<syncer::DataTypeControllerDelegate>
  GetSyncControllerDelegate() = 0;

  // Same as above, but specifically for sync-the-transport (i.e. the user is
  // signed in but didn't opt into sync-the-feature).
  virtual base::WeakPtr<syncer::DataTypeControllerDelegate>
  GetSyncControllerDelegateForTransportMode() = 0;

  // Returns true if the model is performing batch updates right now.
  virtual bool IsPerformingBatchUpdates() const = 0;

  // Tells model to prepare for batch updates.
  // This method is reentrant, i.e. several batch updates may take place at the
  // same time.
  // Returns a scoped batch update object that should be retained while the
  // batch update is performed. Deallocating this object will inform model that
  // the batch update has completed.
  virtual std::unique_ptr<ScopedReadingListBatchUpdate> BeginBatchUpdates() = 0;

  // Returns the set of URLs in the model.
  virtual base::flat_set<GURL> GetKeys() const = 0;

  // Returns the total number of entries in the model.
  virtual size_t size() const = 0;

  // Returns the total number of unread entries in the model.
  virtual size_t unread_size() const = 0;

  // Returns the total number of unseen entries in the model. Note: These
  // entries are also unread so unseen_size() <= unread_size().
  virtual size_t unseen_size() const = 0;

  // Mark all unseen entries as unread.
  virtual void MarkAllSeen() = 0;

  // Delete all the Reading List entries. Return true if entries where indeed
  // deleted. |location| is used for logging purposes and investigations.
  virtual bool DeleteAllEntries(const base::Location& location) = 0;

  // Returns a specific entry. Returns null if the entry does not exist.
  // Please note that the value saved to the account may not be identical to the
  // value returned by GetEntryByURL(). This may happen if DualReadingListModel
  // is dealing with two entries with the same URL, one local and one from the
  // account. In this case, GetEntryByURL() will return the merged view of the
  // two entries.
  virtual scoped_refptr<const ReadingListEntry> GetEntryByURL(
      const GURL& gurl) const = 0;

  // Returns true if |url| can be added to the reading list.
  virtual bool IsUrlSupported(const GURL& url) = 0;

  // If an account exists that syncs the entry which has the given `url`, that
  // account will be returned. Otherwise, the entry may be saved locally on the
  // device or may not exist, in that case an empty account will be returned.
  virtual CoreAccountId GetAccountWhereEntryIsSavedTo(const GURL& url) = 0;

  // Returns true if the entry with `url` requires explicit user action to
  // upload to sync servers.
  virtual bool NeedsExplicitUploadToSyncServer(const GURL& url) const = 0;

  // Uploads all entries (if any) that required explicit upload to sync servers.
  // The upload itself may take long to complete (depending on network
  // connectivity any many other factors).
  virtual void MarkAllForUploadToSyncServerIfNeeded() = 0;

  // Adds |url| at the top of the unread entries, and removes entries with the
  // same |url| from everywhere else if they exist. The entry title will be a
  // trimmed copy of |title|. |time_to_read_minutes| is the estimated time to
  // read the page. The addition may be asynchronous, and the data will be
  // available only once the observers are notified. Callers may use
  // GetAccountWhereEntryIsSavedTo() to determine whether the result of this
  // operation lead to data being saved to a particular account.
  virtual const ReadingListEntry& AddOrReplaceEntry(
      const GURL& url,
      const std::string& title,
      reading_list::EntrySource source,
      base::TimeDelta estimated_read_time) = 0;

  // Removes an entry. The removal may be asynchronous, and not happen
  // immediately. |location| is used for logging purposes and investigations.
  virtual void RemoveEntryByURL(const GURL& url,
                                const base::Location& location) = 0;

  // If the |url| is in the reading list and entry(|url|).read != |read|, sets
  // the read state of the URL to read. This will also update the update time of
  // the entry.
  virtual void SetReadStatusIfExists(const GURL& url, bool read) = 0;

  // Methods to mutate an entry. Will locate the relevant entry by URL. Does
  // nothing if the entry is not found.
  virtual void SetEntryTitleIfExists(const GURL& url,
                                     const std::string& title) = 0;
  virtual void SetEstimatedReadTimeIfExists(
      const GURL& url,
      base::TimeDelta estimated_read_time) = 0;
  virtual void SetEntryDistilledStateIfExists(
      const GURL& url,
      ReadingListEntry::DistillationState state) = 0;

  // Sets the Distilled info for the entry |url|. This method sets the
  // DistillationState of the entry to PROCESSED and sets the |distilled_path|
  // (path of the file on disk), the |distilled_url| (url of the page that
  // was distilled, the |distillation_size| (the size of the offline data) and
  // the |distillation_date| (date of distillation in microseconds since Jan 1st
  // 1970.
  virtual void SetEntryDistilledInfoIfExists(
      const GURL& url,
      const base::FilePath& distilled_path,
      const GURL& distilled_url,
      int64_t distilation_size,
      base::Time distilation_time) = 0;

  // Observer registration methods. The model will remove all observers upon
  // destruction automatically.
  virtual void AddObserver(ReadingListModelObserver* observer) = 0;
  virtual void RemoveObserver(ReadingListModelObserver* observer) = 0;

  virtual void RecordCountMetricsOnUMAUpload() const = 0;

  // Helper class that is used to scope batch updates.
  class ScopedReadingListBatchUpdate {
   public:
    ScopedReadingListBatchUpdate() = default;

    ScopedReadingListBatchUpdate(const ScopedReadingListBatchUpdate&) = delete;
    ScopedReadingListBatchUpdate& operator=(
        const ScopedReadingListBatchUpdate&) = delete;

    virtual ~ScopedReadingListBatchUpdate() = default;
  };
};

#endif  // COMPONENTS_READING_LIST_CORE_READING_LIST_MODEL_H_
