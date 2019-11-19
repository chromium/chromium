// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/observers/history_delete_observer.h"

namespace ukm {

HistoryDeleteObserver::HistoryDeleteObserver() {}

HistoryDeleteObserver::~HistoryDeleteObserver() {}

void HistoryDeleteObserver::ObserveServiceForDeletions(
    history::HistoryService* history_service) {
  if (history_service)
    history_observer_.Add(history_service);
}

void HistoryDeleteObserver::OnURLsDeleted(
    history::HistoryService* history_service,
    const history::DeletionInfo& deletion_info) {
  if (!deletion_info.is_from_expiration())
    OnHistoryDeleted();
}

void HistoryDeleteObserver::HistoryServiceBeingDeleted(
    history::HistoryService* history_service) {
  history_observer_.Remove(history_service);
}

}  // namespace ukm
