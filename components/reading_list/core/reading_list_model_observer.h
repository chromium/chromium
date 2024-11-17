// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_READING_LIST_CORE_READING_LIST_MODEL_OBSERVER_H_
#define COMPONENTS_READING_LIST_CORE_READING_LIST_MODEL_OBSERVER_H_

#include "components/reading_list/core/reading_list_entry.h"

class GURL;
class ReadingListModel;

// Observer for the Reading List model. In the observer methods care should be
// taken to not modify the model.
class ReadingListModelObserver {
 public:
  ReadingListModelObserver(const ReadingListModelObserver&) = delete;
  ReadingListModelObserver& operator=(const ReadingListModelObserver&) = delete;

  // Invoked when the model has finished loading. Until this method is called it
  // is unsafe to use the model.
  virtual void ReadingListModelLoaded(const ReadingListModel* model) = 0;

  // Invoked when the batch updates are about to start. It will only be called
  // once before ReadingListModelCompletedBatchUpdates, even if several updates
  // are taking place at the same time.
  virtual void ReadingListModelBeganBatchUpdates(
      const ReadingListModel* model) {}

  // Invoked when the batch updates have completed. This is called once all
  // batch updates are completed.
  virtual void ReadingListModelCompletedBatchUpdates(
      const ReadingListModel* model) {}

  // Invoked before the destruction of the model.
  virtual void ReadingListModelBeingShutdown(const ReadingListModel* model) {}

  // Invoked from the destructor of the model. The model is no longer valid
  // after this call. There is no need to call RemoveObserver on the model from
  // here, as the observers are automatically deleted.
  virtual void ReadingListModelBeingDeleted(const ReadingListModel* model) {}

  // Invoked when elements are about to be removed from the read or unread list.
  virtual void ReadingListWillRemoveEntry(const ReadingListModel* model,
                                          const GURL& url) {}

  // Invoked when elements have been removed.
  virtual void ReadingListDidRemoveEntry(const ReadingListModel* model,
                                         const GURL& url) {}

  // TODO(crbug.com/40260548): Update the below comment as it seems not
  // accurate. Invoked when elements |MarkEntryUpdated| is called on an entry.
  // This means that the order of the entry may change and read/unread list may
  // change too.
  virtual void ReadingListWillMoveEntry(const ReadingListModel* model,
                                        const GURL& url) {}

  // Invoked when elements |MarkEntryUpdated| has been called on an entry. This
  // means that the order of the entry may have changed and read/unread list may
  // have changed too.
  virtual void ReadingListDidMoveEntry(const ReadingListModel* model,
                                       const GURL& url) {}

  // Invoked when elements are added.
  virtual void ReadingListWillAddEntry(const ReadingListModel* model,
                                       const ReadingListEntry& entry) {}

  // Invoked when elements have been added. This method is called after the
  // the entry has been added to the model and the entry can now be retrieved
  // from the model.
  // |source| says where the entry came from.
  virtual void ReadingListDidAddEntry(const ReadingListModel* model,
                                      const GURL& url,
                                      reading_list::EntrySource source) {}

  // Invoked when an entry is about to change.
  virtual void ReadingListWillUpdateEntry(const ReadingListModel* model,
                                          const GURL& url) {}

  // Invoked when an entry is changed.
  virtual void ReadingListDidUpdateEntry(const ReadingListModel* model,
                                         const GURL& url) {}

  // Called after all the changes signaled by calls to the "Will" methods are
  // done. All the "Will" methods are called as necessary, then the changes
  // are applied and then this method is called.
  virtual void ReadingListDidApplyChanges(ReadingListModel* model) {}

 protected:
  ReadingListModelObserver() = default;
  virtual ~ReadingListModelObserver() = default;
};

#endif  // COMPONENTS_READING_LIST_CORE_READING_LIST_MODEL_OBSERVER_H_
