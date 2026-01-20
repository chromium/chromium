// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"

#include <algorithm>

#include "base/memory/raw_ref.h"
#include "base/no_destructor.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/global_features.h"

GlobalBrowserCollection::GlobalBrowserCollection()
    : platform_delegate_(GlobalBrowserCollectionPlatformDelegate(*this)) {}

GlobalBrowserCollection::~GlobalBrowserCollection() = default;

// static
GlobalBrowserCollection* GlobalBrowserCollection::GetInstance() {
  return g_browser_process->GetFeatures()->global_browser_collection();
}

bool GlobalBrowserCollection::IsEmpty() const {
  return browsers_creation_order_.empty();
}

size_t GlobalBrowserCollection::GetSize() const {
  return browsers_creation_order_.size();
}

BrowserCollection::BrowserVector GlobalBrowserCollection::GetBrowsers(
    Order order) {
  CHECK(order == Order::kCreation || order == Order::kActivation);
  return order == Order::kCreation ? browsers_creation_order_
                                   : browsers_activation_order_;
}

GlobalBrowserCollectionPlatformDelegate*
GlobalBrowserCollection::GetPlatformDelegate() {
  return &platform_delegate_;
}
