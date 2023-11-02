// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_items_collection/core/test_support/scoped_mock_offline_content_provider.h"

#include "components/offline_items_collection/core/offline_content_aggregator.h"
#include "components/offline_items_collection/core/offline_content_provider.h"

namespace offline_items_collection {

ScopedMockOfflineContentProvider::ScopedMockObserver::ScopedMockObserver(
    OfflineContentProvider* provider)
    : provider_(provider) {
  provider_->AddObserver(this);
}

ScopedMockOfflineContentProvider::ScopedMockObserver::ScopedMockObserver()
    : provider_(nullptr) {}

ScopedMockOfflineContentProvider::ScopedMockObserver::~ScopedMockObserver() {
  if (provider_)
    provider_->RemoveObserver(this);
}

void ScopedMockOfflineContentProvider::ScopedMockObserver::AddProvider(
    OfflineContentProvider* provider) {
  DCHECK(!provider_) << "Already has a provider.";
  provider_ = provider;
  provider_->AddObserver(this);
}

ScopedMockOfflineContentProvider::ScopedMockOfflineContentProvider(
    const std::string& name_space,
    OfflineContentAggregator* aggregator)
    : name_space_(name_space), aggregator_(aggregator) {
  aggregator_->RegisterProvider(name_space_, this);
}

ScopedMockOfflineContentProvider::~ScopedMockOfflineContentProvider() {
  Unregister();
}

void ScopedMockOfflineContentProvider::Unregister() {
  if (!aggregator_)
    return;

  aggregator_->UnregisterProvider(name_space_);
  aggregator_ = nullptr;
}

}  // namespace offline_items_collection
