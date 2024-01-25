// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_READING_LIST_IOS_READING_LIST_MODEL_BRIDGE_OBSERVER_H_
#define COMPONENTS_READING_LIST_IOS_READING_LIST_MODEL_BRIDGE_OBSERVER_H_

#import <Foundation/Foundation.h>

#include "base/memory/raw_ptr.h"
#include "components/reading_list/core/reading_list_model_observer.h"

// Protocol duplicating all Reading List Model Observer methods in Objective-C.
@protocol ReadingListModelBridgeObserver<NSObject>

@required

- (void)readingListModelLoaded:(const ReadingListModel*)model;

@optional
- (void)readingListModelDidApplyChanges:(const ReadingListModel*)model;

- (void)readingListModel:(const ReadingListModel*)model
         willRemoveEntry:(const GURL&)url;

- (void)readingListModel:(const ReadingListModel*)model
           willMoveEntry:(const GURL&)url;

- (void)readingListModel:(const ReadingListModel*)model
            willAddEntry:(const ReadingListEntry&)entry;

- (void)readingListModel:(const ReadingListModel*)model
             didAddEntry:(const GURL&)url
             entrySource:(reading_list::EntrySource)source;

- (void)readingListModelBeganBatchUpdates:(const ReadingListModel*)model;
- (void)readingListModelCompletedBatchUpdates:(const ReadingListModel*)model;

- (void)readingListModelBeingShutdown:(const ReadingListModel*)model;
- (void)readingListModelBeingDeleted:(const ReadingListModel*)model;

- (void)readingListModel:(const ReadingListModel*)model
         willUpdateEntry:(const GURL&)url;
- (void)readingListModel:(const ReadingListModel*)model
          didUpdateEntry:(const GURL&)url;

@end

// Observer for the Reading List model that translates all the callbacks to
// Objective-C calls.
class ReadingListModelBridge : public ReadingListModelObserver {
 public:
  explicit ReadingListModelBridge(id<ReadingListModelBridgeObserver> observer,
                                  ReadingListModel* model);

  ReadingListModelBridge(const ReadingListModelBridge&) = delete;
  ReadingListModelBridge& operator=(const ReadingListModelBridge&) = delete;

  ~ReadingListModelBridge() override;

 private:
  void ReadingListModelBeganBatchUpdates(
      const ReadingListModel* model) override;

  void ReadingListModelCompletedBatchUpdates(
      const ReadingListModel* model) override;
  void ReadingListModelLoaded(const ReadingListModel* model) override;
  void ReadingListModelBeingShutdown(const ReadingListModel* model) override;
  void ReadingListModelBeingDeleted(const ReadingListModel* model) override;
  void ReadingListWillRemoveEntry(const ReadingListModel* model,
                                  const GURL& url) override;
  void ReadingListWillMoveEntry(const ReadingListModel* model,
                                const GURL& url) override;
  void ReadingListWillAddEntry(const ReadingListModel* model,
                               const ReadingListEntry& entry) override;
  void ReadingListDidAddEntry(const ReadingListModel* model,
                              const GURL& url,
                              reading_list::EntrySource source) override;
  void ReadingListDidApplyChanges(ReadingListModel* model) override;
  void ReadingListWillUpdateEntry(const ReadingListModel* model,
                                  const GURL& url) override;
  void ReadingListDidUpdateEntry(const ReadingListModel* model,
                                 const GURL& url) override;

  __unsafe_unretained id<ReadingListModelBridgeObserver> observer_;

  raw_ptr<ReadingListModel> model_;  // weak
};

#endif  // COMPONENTS_READING_LIST_IOS_READING_LIST_MODEL_BRIDGE_OBSERVER_H_
