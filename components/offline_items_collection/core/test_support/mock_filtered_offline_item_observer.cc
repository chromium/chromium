// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_items_collection/core/test_support/mock_filtered_offline_item_observer.h"

namespace offline_items_collection {

MockFilteredOfflineItemObserver::MockObserver::MockObserver() = default;
MockFilteredOfflineItemObserver::MockObserver::~MockObserver() = default;

MockFilteredOfflineItemObserver::ScopedMockObserver::ScopedMockObserver(
    FilteredOfflineItemObserver* observer,
    const ContentId& id)
    : id_(id), observer_(observer) {
  observer_->AddObserver(id_, this);
}

MockFilteredOfflineItemObserver::ScopedMockObserver::~ScopedMockObserver() {
  observer_->RemoveObserver(id_, this);
}

}  // namespace offline_items_collection
