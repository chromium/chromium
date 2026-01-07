// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_window/public/browser_collection.h"

#include "base/notimplemented.h"

BrowserCollection::BrowserCollection() = default;

BrowserCollection::~BrowserCollection() = default;

void BrowserCollection::ForEach(
    base::FunctionRef<bool(BrowserWindowInterface*)> on_browser,
    Order order) {
  // TODO(crbug.com/474120522): implement
  NOTIMPLEMENTED();
}

void BrowserCollection::AddObserver(BrowserCollectionObserver* observer) {
  // TODO(crbug.com/474120522): implement
  NOTIMPLEMENTED();
}

void BrowserCollection::RemoveObserver(BrowserCollectionObserver* observer) {
  // TODO(crbug.com/474120522): implement
  NOTIMPLEMENTED();
}
