// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/reading_list/ios/reading_list_model_bridge_observer.h"

#include "components/reading_list/core/reading_list_entry.h"
#include "components/reading_list/core/reading_list_model.h"

ReadingListModelBridge::ReadingListModelBridge(
    id<ReadingListModelBridgeObserver> observer,
    ReadingListModel* model)
    : observer_(observer), model_(model) {
  DCHECK(model);
  model_->AddObserver(this);
}

ReadingListModelBridge::~ReadingListModelBridge() {
  if (model_) {
    model_->RemoveObserver(this);
  }
}

void ReadingListModelBridge::ReadingListModelLoaded(
    const ReadingListModel* model) {
  [observer_ readingListModelLoaded:model];
}

void ReadingListModelBridge::ReadingListModelBeingShutdown(
    const ReadingListModel* model) {
  if ([observer_
          respondsToSelector:@selector(readingListModelBeingShutdown:)]) {
    [observer_ readingListModelBeingShutdown:model];
  }
}

void ReadingListModelBridge::ReadingListModelBeingDeleted(
    const ReadingListModel* model) {
  if ([observer_ respondsToSelector:@selector(readingListModelBeingDeleted:)]) {
    [observer_ readingListModelBeingDeleted:model];
  }
  model_ = nullptr;
}

void ReadingListModelBridge::ReadingListWillRemoveEntry(
    const ReadingListModel* model,
    const GURL& url) {
  if ([observer_
          respondsToSelector:@selector(readingListModel:willRemoveEntry:)]) {
    [observer_ readingListModel:model willRemoveEntry:url];
  }
}

void ReadingListModelBridge::ReadingListWillAddEntry(
    const ReadingListModel* model,
    const ReadingListEntry& entry) {
  if ([observer_
          respondsToSelector:@selector(readingListModel:willAddEntry:)]) {
    [observer_ readingListModel:model willAddEntry:entry];
  }
}

void ReadingListModelBridge::ReadingListDidAddEntry(
    const ReadingListModel* model,
    const GURL& url,
    reading_list::EntrySource source) {
  if ([observer_ respondsToSelector:@selector(readingListModel:
                                                   didAddEntry:
                                                   entrySource:)]) {
    [observer_ readingListModel:model didAddEntry:url entrySource:source];
  }
}

void ReadingListModelBridge::ReadingListDidApplyChanges(
    ReadingListModel* model) {
  if ([observer_
          respondsToSelector:@selector(readingListModelDidApplyChanges:)]) {
    [observer_ readingListModelDidApplyChanges:model];
  }
}

void ReadingListModelBridge::ReadingListModelBeganBatchUpdates(
    const ReadingListModel* model) {
  if ([observer_
          respondsToSelector:@selector(readingListModelBeganBatchUpdates:)]) {
    [observer_ readingListModelBeganBatchUpdates:model];
  }
}

void ReadingListModelBridge::ReadingListModelCompletedBatchUpdates(
    const ReadingListModel* model) {
  if ([observer_
          respondsToSelector:@selector(
                                 readingListModelCompletedBatchUpdates:)]) {
    [observer_ readingListModelCompletedBatchUpdates:model];
  }
}

void ReadingListModelBridge::ReadingListWillMoveEntry(
    const ReadingListModel* model,
    const GURL& url) {
  if ([observer_
          respondsToSelector:@selector(readingListModel:willMoveEntry:)]) {
    [observer_ readingListModel:model willMoveEntry:url];
  }
}

void ReadingListModelBridge::ReadingListWillUpdateEntry(
    const ReadingListModel* model,
    const GURL& url) {
  if ([observer_
          respondsToSelector:@selector(readingListModel:willUpdateEntry:)]) {
    [observer_ readingListModel:model willUpdateEntry:url];
  }
}

void ReadingListModelBridge::ReadingListDidUpdateEntry(
    const ReadingListModel* model,
    const GURL& url) {
  if ([observer_ respondsToSelector:@selector(readingListModel:
                                                didUpdateEntry:)]) {
    [observer_ readingListModel:model didUpdateEntry:url];
  }
}
