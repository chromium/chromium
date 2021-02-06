// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_READING_LIST_CORE_READING_LIST_MODEL_H_
#define COMPONENTS_READING_LIST_CORE_READING_LIST_MODEL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "components/reading_list/core/reading_list_entry.h"
#include "components/reading_list/core/reading_list_model_observer.h"

class GURL;
class ReadingListModel;
class ScopedReadingListBatchUpdate;

namespace syncer {
class ModelTypeSyncBridge;
}

// The reading list model contains two list of entries: one of unread urls, the
// other of read ones. This object should only be accessed from one thread
// (Usually the main thread). The observers callbacks are also sent on the main
// thread.
class ReadingListModel {
 public:
  class ScopedReadingListBatchUpdate;

  // Returns true if the model finished loading. Until this returns true the
  // reading list is not ready for use.
  virtual bool loaded() const = 0;

  // Returns true if the model is performing batch updates right now.
  bool IsPerformingBatchUpdates() const;

  // Returns the ModelTypeSyncBridge responsible for handling sync message.
  virtual syncer::ModelTypeSyncBridge* GetModelTypeSyncBridge() = 0;

  // Tells model to prepare for batch updates.
  // This method is reentrant, i.e. several batch updates may take place at the
  // same time.
  // Returns a scoped batch update object that should be retained while the
  // batch update is performed. Deallocating this object will inform model that
  // the batch update has completed.
  std::unique_ptr<ScopedReadingListBatchUpdate> BeginBatchUpdates();

  // Creates a batch token that will freeze the model while in scope.
  virtual std::unique_ptr<ScopedReadingListBatchUpdate> CreateBatchToken();

  // Returns a vector of URLs in the model. The order of the URL is not
  // specified and can vary on successive calls.
  virtual const std::vector<GURL> Keys() const = 0;

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
  // deleted.
  virtual bool DeleteAllEntries() = 0;

  // Returns the flag about unseen entries on the device.
  // This flag is raised if some unseen items are added on this device.
  // The flag is reset if |ResetLocalUnseenFlag| is called or if all unseen
  // entries are removed.
  // This is a local flag and it can have different values on different devices,
  // even if they are synced.
  // (unseen_size() == 0 => GetLocalUnseenFlag() == false)
  virtual bool GetLocalUnseenFlag() const = 0;

  // Set the unseen flag to false.
  virtual void ResetLocalUnseenFlag() = 0;

  // Returns a specific entry. Returns null if the entry does not exist.
  virtual const ReadingListEntry* GetEntryByURL(const GURL& gurl) const = 0;

  // Returns the first unread entry. If |distilled| is true, prioritize the
  // entries available offline.
  virtual const ReadingListEntry* GetFirstUnreadEntry(bool distilled) const = 0;

  // Returns true if |url| can be added to the reading list.
  virtual bool IsUrlSupported(const GURL& url) = 0;

  // Adds |url| at the top of the unread entries, and removes entries with the
  // same |url| from everywhere else if they exist. The entry title will be a
  // trimmed copy of |title|.
  // The addition may be asynchronous, and the data will be available only once
  // the observers are notified.
  virtual const ReadingListEntry& AddEntry(
      const GURL& url,
      const std::string& title,
      reading_list::EntrySource source) = 0;

  // Removes an entry. The removal may be asynchronous, and not happen
  // immediately.
  virtual void RemoveEntryByURL(const GURL& url) = 0;

  // If the |url| is in the reading list and entry(|url|).read != |read|, sets
  // the read state of the URL to read. This will also update the update time of
  // the entry.
  virtual void SetReadStatus(const GURL& url, bool read) = 0;

  // Methods to mutate an entry. Will locate the relevant entry by URL. Does
  // nothing if the entry is not found.
  virtual void SetEntryTitle(const GURL& url, const std::string& title) = 0;
  virtual void SetEntryDistilledState(
      const GURL& url,
      ReadingListEntry::DistillationState state) = 0;

  // Sets the Distilled info for the entry |url|. This method sets the
  // DistillationState of the entry to PROCESSED and sets the |distilled_path|
  // (path of the file on disk), the |distilled_url| (url of the page that
  // was distilled, the |distillation_size| (the size of the offline data) and
  // the |distillation_date| (date of distillation in microseconds since Jan 1st
  // 1970.
  virtual void SetEntryDistilledInfo(const GURL& url,
                                     const base::FilePath& distilled_path,
                                     const GURL& distilled_url,
                                     int64_t distilation_size,
                                     const base::Time& distilation_time) = 0;

  // Sets the extra info for the entry |url|.
  virtual void SetContentSuggestionsExtra(
      const GURL& url,
      const reading_list::ContentSuggestionsExtra& extra) = 0;
  // Observer registration methods. The model will remove all observers upon
  // destruction automatically.
  void AddObserver(ReadingListModelObserver* observer);
  void RemoveObserver(ReadingListModelObserver* observer);

  // Helper class that is used to scope batch updates.
  class ScopedReadingListBatchUpdate {
   public:
    explicit ScopedReadingListBatchUpdate(ReadingListModel* model)
        : model_(model) {}

    virtual ~ScopedReadingListBatchUpdate();

   private:
    ReadingListModel* model_;

    DISALLOW_COPY_AND_ASSIGN(ScopedReadingListBatchUpdate);
  };

 protected:
  ReadingListModel();
  virtual ~ReadingListModel();

  // The observers.
  base::ObserverList<ReadingListModelObserver>::Unchecked observers_;

  // Tells model that batch updates have completed. Called from
  // ReadingListBatchUpdateToken dtor.
  virtual void EndBatchUpdates();

  // Called when model is entering batch update mode.
  virtual void EnteringBatchUpdates();

  // Called when model is leaving batch update mode.
  virtual void LeavingBatchUpdates();

  SEQUENCE_CHECKER(sequence_checker_);

 private:
  unsigned int current_batch_updates_count_;

  DISALLOW_COPY_AND_ASSIGN(ReadingListModel);
};

#endif  // COMPONENTS_READING_LIST_CORE_READING_LIST_MODEL_H_
