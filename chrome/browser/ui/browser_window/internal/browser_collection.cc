// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_window/public/browser_collection.h"

#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"

BrowserCollection::BrowserCollection() = default;

BrowserCollection::~BrowserCollection() = default;

void BrowserCollection::AddObserver(BrowserCollectionObserver* observer) {
  observers_.AddObserver(observer);
}

void BrowserCollection::RemoveObserver(BrowserCollectionObserver* observer) {
  observers_.RemoveObserver(observer);
}
