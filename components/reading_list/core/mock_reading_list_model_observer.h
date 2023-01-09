// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_READING_LIST_CORE_MOCK_READING_LIST_MODEL_OBSERVER_H_
#define COMPONENTS_READING_LIST_CORE_MOCK_READING_LIST_MODEL_OBSERVER_H_

#include "components/reading_list/core/reading_list_model_observer.h"
#include "testing/gmock/include/gmock/gmock.h"

class MockReadingListModelObserver : public ReadingListModelObserver {
 public:
  MockReadingListModelObserver();
  ~MockReadingListModelObserver() override;

  MOCK_METHOD(void,
              ReadingListModelLoaded,
              (const ReadingListModel*),
              (override));
  MOCK_METHOD(void,
              ReadingListModelBeganBatchUpdates,
              (const ReadingListModel*),
              (override));
  MOCK_METHOD(void,
              ReadingListModelCompletedBatchUpdates,
              (const ReadingListModel*),
              (override));
  MOCK_METHOD(void,
              ReadingListModelBeingShutdown,
              (const ReadingListModel*),
              (override));
  MOCK_METHOD(void,
              ReadingListModelBeingDeleted,
              (const ReadingListModel*),
              (override));
  MOCK_METHOD(void,
              ReadingListWillRemoveEntry,
              (const ReadingListModel*, const GURL&),
              (override));
  MOCK_METHOD(void,
              ReadingListDidRemoveEntry,
              (const ReadingListModel*, const GURL&),
              (override));
  MOCK_METHOD(void,
              ReadingListWillMoveEntry,
              (const ReadingListModel*, const GURL&),
              (override));
  MOCK_METHOD(void,
              ReadingListDidMoveEntry,
              (const ReadingListModel*, const GURL&),
              (override));
  MOCK_METHOD(void,
              ReadingListWillAddEntry,
              (const ReadingListModel*, const ReadingListEntry& entry),
              (override));
  MOCK_METHOD(void,
              ReadingListDidAddEntry,
              (const ReadingListModel*,
               const GURL&,
               reading_list::EntrySource source),
              (override));
  MOCK_METHOD(void,
              ReadingListWillUpdateEntry,
              (const ReadingListModel*, const GURL&),
              (override));
  MOCK_METHOD(void,
              ReadingListDidUpdateEntry,
              (const ReadingListModel*, const GURL&),
              (override));
  MOCK_METHOD(void,
              ReadingListDidApplyChanges,
              (ReadingListModel * model),
              (override));
};

#endif  // COMPONENTS_READING_LIST_CORE_MOCK_READING_LIST_MODEL_OBSERVER_H_
